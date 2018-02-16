#include <iostream>
#include <boost/property_tree/json_parser.hpp>

#include "user.hpp"
#include "settings.hpp"
#include "networkclient.h"


using namespace std;

int main(int argc, char* argv[])
{
    settings sts;
    sts.get(argc,argv);
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(sts.host,std::to_string(sts.port).c_str());
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    boost::asio::ip::tcp::socket socket(io_service);

    NetworkClient netClient(sts.host,std::to_string(sts.port));

    #if INTPTR_MAX == INT64_MAX
      assert(sizeof(long double)==16);
    #endif
    try{
        std::string line;
        usertxs_ptr txs;

        while(cin)
        {
            cin >> line;
            int64_t deduct    = 0;
            int64_t fee       = 0;

            if(line.at(0) == '.'){
                break;
            }
            if(line.at(0) == '\n' || line.at(0) == '#'){
                continue;
            }

            std::unique_ptr<IBlockCommand> t2;

            txs = run_json(sts, line, deduct, fee, t2);

            if( !txs ){
                continue;
            }

            //temporary solution for reimplementing
            if(txs->ttype == TXSTYPE_INF)
            {
                talk2(netClient, sts, t2, deduct, fee);
            }
            else
            {

                talk(endpoint_iterator, socket, sts, txs, deduct, fee);
            }
        }
      }
    catch (std::exception& e){
      std::cerr << "Main Exception: " << e.what() << "\n";
    }
    return 0;
}
