#include <iostream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include "user.hpp"
#include "settings.hpp"
#include "networkclient.h"
#include "responsehandler.h"
#include "helper/ascii.h"
#include "helper/hash.h"
#include "helper/json.h"

using namespace std;

void verifyProtocol(INetworkClient& netClient) {
    int32_t version = CLIENT_PROTOCOL_VERSION;
    int32_t node_version;
#ifdef DEBUG
    version = -version;
#endif
    netClient.sendData(reinterpret_cast<unsigned char*>(&version), sizeof(version));

    if(!netClient.readData(&node_version, sizeof(node_version))) {
        netClient.disConnect();
        throw CommandException(ErrorCodes::Code::eConnectServerError, "Could not read version");
    }

    uint8_t version_error;
    if(!netClient.readData(&version_error, 1) || version_error) {
        ELOG("Version mismatch client(%d) <> node(%d)\n", version, node_version);
        netClient.disConnect();
        throw CommandException(ErrorCodes::Code::eProtocolMismatch, "Version mismatch client(%d) <> node(%d)");
    }
}



void talk(NetworkClient& netClient, settings sts, ResponseHandler& respHandler, std::unique_ptr<IBlockCommand> command) {
    if(sts.drun && command->getCommandType() == CommandType::eModifying) {
      respHandler.onDryRun(std::move(command));
      return;
    }

    if(!netClient.isConnected()) {
        if(!netClient.connect()) {
            ELOG("Error: %s", ErrorCodes().getErrorMsg(ErrorCodes::Code::eConnectServerError));
            throw CommandException(ErrorCodes::Code::eConnectServerError, "Connect failed");
        }

        verifyProtocol(netClient);
    }

    if(command->send(netClient)) {
        respHandler.onExecute(std::move(command));
    } else {
        ELOG("ERROR reading global info talk\n");
        netClient.disConnect();
        if(command->m_responseError) {
            throw CommandException(command->m_responseError, "");
        }
        throw CommandException(ErrorCodes::Code::eConnectServerError, "ERROR reading response");
    }
}

boost::property_tree::ptree getPropertyTree(const std::string& line) {
    try {
        boost::property_tree::ptree pt;
        std::stringstream ss(line);
        boost::property_tree::read_json(ss, pt);
        return pt;
    }
    catch (std::exception& e) {
        std::cerr << "RUN_JSON Exception: " << e.what() << "\n";
        throw CommandException(ErrorCodes::Code::eCommandParseError, e.what());
    }
}

bool shouldDecodeRaw(const boost::property_tree::ptree& pt) {
    const auto run = pt.get<std::string>("run");
    return run == "decode_raw";
}

void decodeRaw(bool nice, std::unique_ptr<IBlockCommand> command) {
    boost::property_tree::ptree pt;
    command->txnToJson(pt);
    boost::property_tree::write_json(std::cout, pt, nice);
}

int main(int argc, char* argv[]) {

#ifndef NDEBUG
    std::setbuf(stdout,NULL);
#endif

    auto workdir = settings::get_workdir(argc, argv, false);
    if(workdir != ".") {
        settings::change_working_dir(workdir);
    }

    settings sts;
    sts.get(argc,argv);
    boost::shared_ptr<NetworkClient> netClient(new NetworkClient(sts.host, std::to_string(sts.port), sts.nice));
    boost::shared_ptr<NetworkClient> netClientModify(netClient);
    ResponseHandler respHandler(sts);

#if INTPTR_MAX == INT64_MAX
    assert(sizeof(long double)==16);
#endif
    try {
        std::string line;

        while (std::getline(std::cin, line)) {
            respHandler.redirect_host = "";
            respHandler.redirect_port = "";
            DLOG("GOT REQUEST %s\n", line.c_str());

            if(line.at(0) == '.') {
                break;
            }
            if(line.at(0) == '\n' || line.at(0) == '#') {
                continue;
            }

            try {
                auto propertyTree = getPropertyTree(line);
                while(true) {
                    auto command = run_json(sts, propertyTree);

                    if(!command) {
                        throw CommandException(ErrorCodes::Code::eCommandParseError, "");
                    }

                    auto commandType = command->getCommandType();

                    if(shouldDecodeRaw(propertyTree)) {
                        decodeRaw(sts.nice, std::move(command));
                    } else {

                        try {
                            talk(commandType == CommandType::eModifying ? *netClientModify : *netClient, sts, respHandler, std::move(command));
                        }
                        catch(RedirectException &e) {
                            respHandler.redirect_host = e.getIp().to_string();
                            respHandler.redirect_port = std::to_string(e.getPort());
                            boost::shared_ptr<NetworkClient> x(new NetworkClient(respHandler.redirect_host, respHandler.redirect_port, sts.nice));
                            if(commandType == CommandType::eModifying) {
                                netClientModify.swap(x);
                            } else {
                                netClient.swap(x);
                            }
                            continue;
                        }
                        break;
                    }
                }
            }
            catch(const boost::property_tree::ptree_bad_path& e) {
                boost::property_tree::ptree pt;
                pt.put(ERROR_TAG, ErrorCodes().getErrorMsg(ErrorCodes::Code::eCommandParseError));
                pt.put(ERROR_CODE_TAG, ErrorCodes::Code::eCommandParseError);

                std::string info = "Required field is missing: " + e.path<boost::property_tree::path>().reduce();
                pt.put(ERROR_INFO_TAG, info);
                if(respHandler.redirect_host.length() > 0) {
                    pt.put("redirect", respHandler.redirect_host + ":" + respHandler.redirect_port);
                }
                boost::property_tree::write_json(std::cout, pt, sts.nice);
            }
            catch(const boost::property_tree::ptree_bad_data& e) {
                boost::property_tree::ptree pt;
                pt.put(ERROR_TAG, ErrorCodes().getErrorMsg(ErrorCodes::Code::eCommandParseError));
                pt.put(ERROR_CODE_TAG, ErrorCodes::Code::eCommandParseError);

                pt.put(ERROR_INFO_TAG, e.what());
                if(respHandler.redirect_host.length() > 0) {
                    pt.put("redirect", respHandler.redirect_host + ":" + respHandler.redirect_port);
                }
                boost::property_tree::write_json(std::cout, pt, sts.nice);
            }
            catch(const CommandException &e) {
                boost::property_tree::ptree pt;
                pt.put(ERROR_TAG, e.getError());
                pt.put(ERROR_CODE_TAG, e.getErrorCode());
                pt.put(ERROR_INFO_TAG, e.getErrorInfo());
                if(respHandler.redirect_host.length() > 0) {
                    pt.put("redirect", respHandler.redirect_host + ":" + respHandler.redirect_port);
                }
                boost::property_tree::write_json(std::cout, pt, sts.nice);
            }
            catch(const ErrorCodes::Code& responseError) {
                boost::property_tree::ptree pt;
                pt.put(ERROR_TAG, ErrorCodes().getErrorMsg(responseError));
                pt.put(ERROR_CODE_TAG, responseError);
                if(respHandler.redirect_host.length() > 0) {
                    pt.put("redirect", respHandler.redirect_host + ":" + respHandler.redirect_port);
                }
                boost::property_tree::write_json(std::cout, pt, sts.nice);
            }
        }
    } catch (const std::exception& e) {
        ELOG("Main Exception: %s\n", e.what());
        Helper::printErrorJson(ErrorCodes::Code::eUnknownError, "Unknown error occured", sts.nice);
    }

    netClient->disConnect();

    return 0;
}
