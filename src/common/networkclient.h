#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <string>
#include <atomic>
#include <memory>

#include <boost/asio.hpp>

class INetworkClient
{
public:
    //INetworkClient(const std::string& address, const std::string& port);
    virtual bool connect()      = 0;
    virtual bool reconnect()    = 0;
    virtual bool disConnect()   = 0;
    virtual bool sendData(uint8_t* buff, int size)      = 0;
    virtual bool sendData(std::vector<uint8_t> buff)    = 0;
    virtual bool readData(uint8_t* buff, int size)      = 0;
    virtual bool readData(char* buff, int size)         = 0;
    virtual bool readData(std::vector<uint8_t>& buff)   = 0;
    virtual ~INetworkClient() = default;
};

class NetworkClient : public INetworkClient
{
public:
    explicit NetworkClient(const std::string& address, const std::string& port);
    virtual ~NetworkClient();

    // INetworkClient interface
    virtual bool connect()      override;
    virtual bool reconnect()    override;
    virtual bool disConnect()   override;
    virtual bool sendData(uint8_t* buff, int size)      override;
    virtual bool sendData(std::vector<uint8_t> buff)    override;
    virtual bool readData(uint8_t* buff, int size)      override;
    virtual bool readData(char* buff, int size)         override;
    virtual bool readData(std::vector<uint8_t>& buff)   override;


private:
    boost::asio::io_service                             m_ioService;
    boost::asio::ip::tcp::resolver::query               m_query;
    boost::asio::ip::tcp::resolver                      m_resolver;
    std::unique_ptr<boost::asio::ip::tcp::socket>       m_socket;
    std::atomic<bool>                                   m_connected{false};

};

#endif // NETWORKCLIENT_H
