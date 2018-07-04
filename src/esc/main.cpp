#include <iostream>
#include <boost/property_tree/json_parser.hpp>
#include "user.hpp"
#include "settings.hpp"
#include "networkclient.h"
#include "responsehandler.h"
#include "helper/ascii.h"
#include "helper/hash.h"
#include "helper/json.h"

using namespace std;


ErrorCodes::Code talk(NetworkClient& netClient, ResponseHandler& respHandler, std::unique_ptr<IBlockCommand> command) {
    if(!netClient.reconnect()) {
        ELOG("Error: %s", ErrorCodes().getErrorMsg(ErrorCodes::Code::eConnectServerError));
        return ErrorCodes::Code::eConnectServerError;
    }

    if(command->send(netClient) ) {
        respHandler.onExecute(std::move(command));
    } else {
        ELOG("ERROR reading global info talk\n");
        return ErrorCodes::Code::eConnectServerError;
    }

    return ErrorCodes::Code::eNone;
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
    NetworkClient netClient(sts.host, std::to_string(sts.port), sts.nice);
    ResponseHandler respHandler(sts);

#if INTPTR_MAX == INT64_MAX
    assert(sizeof(long double)==16);
#endif
    try {
        std::string line;

        while (std::getline(std::cin, line)) {
            DLOG("GOT REQUEST %s\n", line.c_str());

            ErrorCodes::Code responseError = ErrorCodes::Code::eNone;

            if(line.at(0) == '.') {
                break;
            }
            if(line.at(0) == '\n' || line.at(0) == '#') {
                continue;
            }

            auto command = run_json(sts, line);

            if(command) {
                responseError = talk(netClient, respHandler, std::move(command));
            }
            else {
                responseError = ErrorCodes::Code::eCommandParseError;
            }

            if(responseError)
            {
                boost::property_tree::ptree pt;
                pt.put(ERROR_TAG, ErrorCodes().getErrorMsg(ErrorCodes::Code::eCommandParseError));
                boost::property_tree::write_json(std::cout, pt, sts.nice);
            }

        }
    } catch (std::exception& e) {
        ELOG("Main Exception: %s\n", e.what());
    }

    return 0;
}
