#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <memory>
#include "default.hpp"

namespace Helper {

void setSocketTimeout(boost::asio::ip::tcp::socket &socket, unsigned int timeout, unsigned int idletimeout, unsigned int maxtry);

void setSocketTimeout(std::unique_ptr<boost::asio::ip::tcp::socket> &socket, unsigned int timeout, unsigned int idletimeout, unsigned int maxtry);

void setSocketNoDelay(boost::asio::ip::tcp::socket &socket, bool noDelay);
void setSocketNoDelay(std::unique_ptr<boost::asio::ip::tcp::socket>, bool noDelay);
}

#endif // SOCKET_H
