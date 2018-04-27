#ifndef ASCII_H
#define ASCII_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <boost/asio.hpp>

namespace Helper {

void setSocketTimeout(boost::asio::ip::tcp::socket &socket, unsigned int timeout /* = 10*/)
{
    timeval tv{timeout,0};

    if(setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 ) {
        if ( setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0 )
        {
            boost::asio::socket_base::keep_alive optionk(true);
            socket.set_option(optionk);
        }
    }
}

void setSocketTimeout(std::unique_ptr<boost::asio::ip::tcp::socket> &socket, unsigned int timeout /*= 10*/)
{
    setSocketTimeout(*socket.get(), timeout);
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
