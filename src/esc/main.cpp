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


void talk2(NetworkClient& netClient, ResponseHandler& respHandler, settings& /*sts*/, std::unique_ptr<IBlockCommand> txs) { //len can be deduced from txstype
    if(!netClient.reconnect()) {
        ELOG("Error: %s", ErrorCodes().getErrorMsg(ErrorCodes::Code::eConnectServerError));
    }

    if( txs->send(netClient) ) {
        respHandler.onExecute(std::move(txs));
    } else {
        ELOG("ERROR reading global info talk2\n");
    }
}

int main(int argc, char* argv[]) {

#ifndef NDEBUG
    std::setbuf(stdout,NULL);
#endif

    settings sts;
    sts.get(argc,argv);
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(sts.host,std::to_string(sts.port).c_str());
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    boost::asio::ip::tcp::socket socket(io_service);

    NetworkClient netClient(sts.host,std::to_string(sts.port));

    ResponseHandler respHandler(sts);

#if INTPTR_MAX == INT64_MAX
    assert(sizeof(long double)==16);
#endif
    try {

        usertxs_ptr txs;
        std::string line;
        std::unique_ptr<IBlockCommand> t2;

        while (std::getline(std::cin, line)) {
            int64_t deduct    = 0;
            int64_t fee       = 0;

            if(line.at(0) == '.') {
                break;
            }
            if(line.at(0) == '\n' || line.at(0) == '#') {
                continue;
            }

            txs = run_json(sts, line, deduct, fee, t2);

            if( !txs && ! t2) {
                continue;
            }

            //temporary solution for reimplementing
            if(t2) {
                talk2(netClient, respHandler, sts, std::move(t2));
            } else if(txs) {
                talk(endpoint_iterator, socket, sts, txs, deduct, fee);
            }
        }
    } catch (std::exception& e) {
        ELOG("Main Exception: %s\n", e.what());
    }

    return 0;
}
