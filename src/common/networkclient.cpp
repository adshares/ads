#include "networkclient.h"

#include <iostream>
#include <boost/array.hpp>
#include "helper/json.h"
#include "helper/ascii.h"
#include "helper/socket.h"

#define NETCL_SOCK_TIMEOUT 4
#define NETCL_SOCK_IDLE    15
#define NETCL_SOCK_MAXTRY  3

NetworkClient::NetworkClient(const std::string& address, const std::string& port, bool pretty):
    m_query(address, port.c_str()),
    m_resolver(m_ioService),
    m_prettyJson(pretty),
    m_timeout(0)
{
}

NetworkClient::~NetworkClient() {
}

void NetworkClient::setTimeout()
 {
    m_timeout = time(NULL) + NETCL_SOCK_TIMEOUT;
 }

bool NetworkClient::isConnected() {
    if(m_timeout && time(NULL) >= m_timeout) {
        m_socket->close();
        m_connected = false;
    }
    return m_connected;
}

bool NetworkClient::connect() {
    if(!m_connected) {
        try {
            boost::asio::ip::tcp::resolver::iterator    end;
            boost::asio::ip::tcp::resolver::iterator    connectpoint    = m_resolver.resolve(m_query);
            boost::system::error_code                   error           = boost::asio::error::host_not_found;

            while (error && connectpoint != end) {
                if(m_socket && m_socket->is_open()) {
                    m_socket->close();
                }

                m_socket.reset(new boost::asio::ip::tcp::socket(m_ioService));

                if(m_socket) {
                    m_socket->connect(*connectpoint++, error);
                    Helper::setSocketTimeout(m_socket, NETCL_SOCK_TIMEOUT, NETCL_SOCK_IDLE, NETCL_SOCK_MAXTRY);
                }
            }

            if (error) {
                throw CommandException(ErrorCodes::Code::eConnectServerError, error.message());
            }
        } catch(std::exception& e) {
            m_connected = false;
            throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
        }

        m_connected = true;
        setTimeout();
    }

    return true;
}

bool NetworkClient::disConnect() {
    if(m_connected) {
        try {
            if(m_socket) {
                m_socket->close();
            }
            m_socket.reset();
        } catch(std::exception& e) {
            m_connected = false;
            throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
        }

        m_connected = false;
    }

    return true;
}

bool NetworkClient::reconnect() {
    disConnect();
    return connect();
}

bool NetworkClient::sendData(std::vector<uint8_t> buff) {
    try {
        if(m_connected && m_socket) {
            auto size = buff.size();
            if( boost::asio::write(*m_socket.get(), boost::asio::buffer(std::move(buff))) == size ) {
                setTimeout();
                return true;
            }
        }
    } catch(std::exception& e) {
        m_connected = false;
        throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
    }

    return false;
}

bool NetworkClient::sendData(uint8_t* buff, int size) {

    try {
        if(m_connected && m_socket) {
            if( boost::asio::write(*m_socket.get(), boost::asio::buffer(buff, size)) == (std::size_t)size ) {
                setTimeout();
                return true;
            }
        }
    } catch(std::exception& e) {
        m_connected = false;
        throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
    }

    return false;
}

bool NetworkClient::readData(std::vector<uint8_t>& buff) {
    try {
        if(m_connected && m_socket) {
            auto size = buff.size();
            if( boost::asio::read(*m_socket.get(), boost::asio::buffer(buff)) == size ) {
                setTimeout();
                return true;
            }
        }
    } catch(std::exception& e) {
        m_connected = false;
        throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
    }

    return false;
}

bool NetworkClient::readData(uint8_t* buff, int size) {
    try {
        if(m_connected && m_socket) {
            if( boost::asio::read(*m_socket.get(), boost::asio::buffer(buff, size)) == (size_t)size ) {
                setTimeout();
                return true;
            }
        }
    } catch(std::exception& e) {
        m_connected = false;
        throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
    }

    return false;
}

bool NetworkClient::readData(char* buff, int size) {
    try {
        if(m_connected && m_socket) {
            if( boost::asio::read(*m_socket.get(), boost::asio::buffer(buff, size)) == (size_t)size ) {
                setTimeout();
                return true;
            }
        }
    } catch(std::exception& e) {
        m_connected = false;
        throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
    }

    return false;
}

bool NetworkClient::readData(int32_t *buff, int size) {
    try {
        if(m_connected && m_socket) {
            if (boost::asio::read(*m_socket.get(), boost::asio::buffer(buff, size)) == (size_t)size) {
                setTimeout();
                return true;
            }
        }
    } catch(std::exception& e) {
        m_connected = false;
        throw CommandException(ErrorCodes::Code::eConnectServerError, e.what());
    }

    return false;
}
