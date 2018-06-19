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


void talk(NetworkClient& netClient, ResponseHandler& respHandler, std::unique_ptr<IBlockCommand> command) {
    if(!netClient.reconnect()) {
        ELOG("Error: %s", ErrorCodes().getErrorMsg(ErrorCodes::Code::eConnectServerError));
    }

    if(command->send(netClient) ) {
        respHandler.onExecute(std::move(command));
    } else {
        ELOG("ERROR reading global info talk\n");
    }
}

int main(int argc, char* argv[]) {

#ifndef NDEBUG
    std::setbuf(stdout,NULL);
#endif

    auto workdir = settings::get_workdir(argc, argv);
    if(workdir != ".") {
        settings::change_working_dir(workdir);
    }

    settings sts;
    sts.get(argc,argv);
    NetworkClient netClient(sts.host, std::to_string(sts.port));
    ResponseHandler respHandler(sts);

#if INTPTR_MAX == INT64_MAX
    assert(sizeof(long double)==16);
#endif
    try {
        std::string line;

        while (std::getline(std::cin, line)) {
            DLOG("GOT REQUEST %s\n", line.c_str());

            if(line.at(0) == '.') {
                break;
            }
            if(line.at(0) == '\n' || line.at(0) == '#') {
                continue;
            }

            auto command = run_json(sts, line);

            if(command) {
                talk(netClient, respHandler, std::move(command));
            }
            else {
                boost::property_tree::ptree pt;
                pt.put(ERROR_TAG, ErrorCodes().getErrorMsg(ErrorCodes::Code::eCommandParseError));
                boost::property_tree::write_json(std::cout, pt, sts.nice);
                continue;
            }
        }
    } catch (std::exception& e) {
        ELOG("Main Exception: %s\n", e.what());
    }

    return 0;
}
