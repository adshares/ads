#ifndef ASCII_H
#define ASCII_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <boost/asio.hpp>

namespace Helper {

void setSocketTimeout(boost::asio::ip::tcp::socket &socket, unsigned int timeout, unsigned int idletimeout, unsigned int maxtry)
{
    int optval;
    socklen_t optlen = sizeof(optval);

    optval = maxtry;
    if(setsockopt(socket.native_handle(), SOL_TCP, TCP_KEEPCNT, &optval, optlen) < 0) {
        return;
    }

    optval = idletimeout;
    if(setsockopt(socket.native_handle(), SOL_TCP, TCP_KEEPIDLE, &optval, optlen) < 0) {
        return;
    }

    optval = timeout;
    if(setsockopt(socket.native_handle(), SOL_TCP, TCP_KEEPINTVL, &optval, optlen) < 0) {
        return;
    }

    boost::asio::socket_base::keep_alive optionk(true);
    socket.set_option(optionk);
}

void setSocketTimeout(std::unique_ptr<boost::asio::ip::tcp::socket> &socket, unsigned int timeout, unsigned int idletimeout, unsigned int maxtry)
{
    setSocketTimeout(*socket.get(), timeout, idletimeout, maxtry);
}

void setSocketNoDelay(boost::asio::ip::tcp::socket &socket, bool noDelay)
{    
    boost::asio::ip::tcp::no_delay option(noDelay);
    socket.set_option(option);
}

void setSocketNoDelay(std::unique_ptr<boost::asio::ip::tcp::socket>& socket, bool noDelay)
{
    setSocketNoDelay(*socket.get(), noDelay);
}

}//end of Helper

#endif // ASCII_H
