#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <string>
#include <atomic>
#include <memory>

#include "abstraction/interfaces.h"
#include <boost/asio.hpp>


class NetworkClient : public INetworkClient {
  public:
    explicit NetworkClient(const std::string& address, const std::string& port, bool pretty);
    virtual ~NetworkClient();

    // INetworkClient interface
    virtual bool connect()                              override;
    virtual bool reconnect()                            override;
    virtual bool disConnect()                           override;
    virtual bool sendData(uint8_t* buff, int size)      override;
    virtual bool sendData(std::vector<uint8_t> buff)    override;
    virtual bool readData(uint8_t* buff, int size)      override;
    virtual bool readData(char* buff, int size)         override;
    virtual bool readData(std::vector<uint8_t>& buff)   override;
    virtual bool readData(int32_t* buff, int size)      override;
    virtual bool isConnected()                          override;


  private:
    boost::asio::io_service                             m_ioService;
    boost::asio::ip::tcp::resolver::query               m_query;
    boost::asio::ip::tcp::resolver                      m_resolver;
    std::unique_ptr<boost::asio::ip::tcp::socket>       m_socket;
    std::atomic<bool>                                   m_connected{false};
    bool                                                m_prettyJson;
    int32_t                                             m_timeout;

    void setTimeout();
};

#endif // NETWORKCLIENT_H
