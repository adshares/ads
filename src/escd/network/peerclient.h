#ifndef PEERCLIENT_H
#define PEERCLIENT_H

#include <memory>
#include <boost/asio.hpp>
#include <boost/thread/future.hpp>

class peer;

typedef boost::function<void(const boost::system::error_code&, size_t)> peerCallback;
typedef boost::function<void(const boost::system::error_code&)>         peerConnectCallback;

class PeerClient
{    
public:
    PeerClient(boost::asio::ip::tcp::socket &socket);

    void asyncRead(void* data, uint32_t len, peerCallback handler, int timeout = 30);
    void asyncWrite(void* data, uint32_t len, peerCallback handler, int timeout = 30);
    void asyncConnect(boost::asio::ip::tcp::resolver::iterator& tcpIterator, peerConnectCallback handler, int timeout = 15);

    //expose writeSync and readSync only for peer
    friend class peer;
protected:
    //execute only from peer thread
    std::size_t writeSync(void* data , uint32_t len,  int timeout = 30);
    std::size_t readSync(void* data , uint32_t len,  int timeout = 30);

private:
    void checkDeadline(boost::shared_ptr<boost::asio::deadline_timer> timer, const boost::system::error_code& ec);
    void checkDeadline();
    void operationDone(const boost::system::error_code& ec, size_t bytes_transferred);

    void asyncHandleWrite(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler);
    void asyncHandleRead(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler);
    void asyncHandleConnect(const boost::system::error_code& ec, boost::shared_ptr<boost::asio::deadline_timer> timer, peerConnectCallback handler);

private:    
    boost::asio::ip::tcp::socket&   m_socket;
    boost::asio::deadline_timer     m_deadline;
    boost::system::error_code       m_ec;
    size_t                          m_bytes_transferred{0};
};


#endif // PEERCLIENT_H
