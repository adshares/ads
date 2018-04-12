#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <memory>
#include "default.hpp"

namespace Helper {

void setSocketTimeout(boost::asio::ip::tcp::socket &socket, unsigned int timeout = 10);

void setSocketTimeout(std::unique_ptr<boost::asio::ip::tcp::socket> &socket, unsigned int timeout = 10);

void setSocketNoDelay(boost::asio::ip::tcp::socket &socket, bool noDelay);
void setSocketNoDelay(std::unique_ptr<boost::asio::ip::tcp::socket>, bool noDelay);

/*void parseAddress(std::string fullAddres, std::string& address2, std::string& port)
{
    DLOG("TRY connecting to address %s\n", addres.c_str());
    //const char* port = SERVER_PORT;
    char* p=strchr((char*)fullAddres.c_str(),'/'); //separate peer id
    if(p!=NULL) {
        peer_address[p-addres.c_str()]='\0';
    }
    p=strchr((char*)fullAddres.c_str(),':'); //detect port
    if(p!=NULL) {
        peer_address[p-addres.c_str()]='\0';
        port=p+1;
    }
}*/



}

#endif // SOCKET_H
