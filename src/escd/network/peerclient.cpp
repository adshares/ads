#include "peerclient.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "../peer.hpp"
#include <boost/lambda/lambda.hpp>

using namespace boost::asio;
using namespace boost::lambda;

PeerClient::PeerClient(ip::tcp::socket& socket, peer& peer):
    m_peer(peer),
    m_socket(socket),
    m_deadline(m_socket.get_io_service())
{}

void PeerClient::checkDeadline(boost::shared_ptr<deadline_timer> timer, const boost::system::error_code& ec)
{    
    if (ec == boost::asio::error::operation_aborted ) {
        return;
    }

    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (timer->expires_at() <= deadline_timer::traits_type::now())
    {      
      // The deadline has passed. The socket is closed so that any outstanding
      // Peer should leave. It will be done by peermanager.

      ELOG("%04X DEADLINE ERROR. PEER STATUS %02d \n", m_peer.getSvid(), m_peer.getState());
      m_socket.close();
      m_peer.leave();
      //throw boost::system::system_error(boost::asio::error::timed_out);

      // There is no longer an active deadline. The expiry is set to positive
      // infinity so that the actor takes no action until a new deadline is set.
      timer->expires_at(boost::posix_time::pos_infin);
    }

    timer->async_wait(boost::bind(&PeerClient::checkDeadline, this, timer, boost::asio::placeholders::error));
}

void PeerClient::checkDeadline()
{    
    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (m_deadline.expires_at() <= deadline_timer::traits_type::now())
    {
      ELOG("%04X DEADLINE ERROR.  PEER STATUS %02d \n", m_peer.getSvid(), m_peer.getState());

      // The deadline has passed. The socket is closed so that any outstanding
      // Peer should leave. It will be done by peermanager.

      m_socket.close();
      m_peer.leave();
      //throw boost::system::system_error(boost::asio::error::timed_out);

      // There is no longer an active deadline. The expiry is set to positive
      // infinity so that the actor takes no action until a new deadline is set.
      m_deadline.expires_at(boost::posix_time::pos_infin);
    }

    m_deadline.async_wait(boost::bind(&PeerClient::checkDeadline, this));
 }

std::size_t PeerClient::writeSync(void* data , uint32_t len,  int timeout)
{
    if(timeout > 0){
        m_deadline.expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline();
    }

    m_bytes_transferred = 0;
    m_ec = boost::asio::error::would_block;
    boost::asio::async_write(m_socket, boost::asio::buffer(data, len), boost::bind(&PeerClient::operationDone, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

    do{
        auto& ioService = m_socket.get_io_service();
        if (ioService.stopped()) {
            ELOG("Stopping writeSync task because io_service is stopped\n");
            break;
        }
        ioService.run_one();
    }
    while (m_ec == boost::asio::error::would_block);

    m_deadline.expires_at(boost::posix_time::pos_infin);

    if (m_ec) {
         throw boost::system::system_error(m_ec);
    }

    assert(m_bytes_transferred==len);

    return m_bytes_transferred;
}

std::size_t PeerClient::readSync(void* data , uint32_t len,  int timeout)
{
    if(timeout > 0){
        m_deadline.expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline();
    }        

    m_ec = boost::system::error_code(boost::asio::error::would_block);
    m_bytes_transferred = 0;
    boost::asio::async_read(m_socket, boost::asio::buffer(data, len), boost::bind(&PeerClient::operationDone, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

    do{
        auto& ioService = m_socket.get_io_service();
        if (ioService.stopped()) {
            ELOG("Stopping readSync task because io_service is stopped\n");
            break;
        }
        ioService.run_one();
    }
    while (m_ec == boost::asio::error::would_block);

    m_deadline.expires_at(boost::posix_time::pos_infin);

    if (m_ec) {
         throw boost::system::system_error(m_ec);
    }

    assert(m_bytes_transferred==len);       

    return m_bytes_transferred;
}

void PeerClient::asyncRead(void* data, uint32_t len, peerCallback handler, int timeout)
{    
    boost::shared_ptr<deadline_timer> timer = boost::make_shared<deadline_timer>(m_socket.get_io_service());

    if(timeout > 0){
        timer->expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline(timer, boost::asio::error::timed_out);
    }

    boost::asio::async_read(m_socket, boost::asio::buffer(data, len), boost::bind(&PeerClient::asyncHandleRead, this,
                                            boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, timer, handler));

}

void PeerClient::asyncConnect(boost::asio::ip::tcp::resolver::iterator& tcpIterator, peerConnectCallback handler, int timeout)
{    
    boost::shared_ptr<deadline_timer> timer = boost::make_shared<deadline_timer>(m_socket.get_io_service());

    if(timeout > 0)
    {
        timer->expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline(timer, boost::asio::error::timed_out);
    }

    boost::asio::async_connect(m_socket, tcpIterator, boost::bind(&PeerClient::asyncHandleConnect, this, boost::asio::placeholders::error, timer, handler));
}

void PeerClient::asyncWrite(void* data, uint32_t len, peerCallback handler, int timeout)
{    
    boost::shared_ptr<deadline_timer> timer = boost::make_shared<deadline_timer>(m_socket.get_io_service());

    if(timeout > 0){
        timer->expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline(timer, boost::asio::error::timed_out);
    }

    boost::asio::async_write(m_socket, boost::asio::buffer(data, len), boost::bind(&PeerClient::asyncHandleWrite, this,
                                            boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, timer, handler));
}

void PeerClient::operationDone(const boost::system::error_code& ec, size_t bytes_transferred)
{    
    m_deadline.expires_at(boost::posix_time::pos_infin);
    m_ec = ec;
    m_bytes_transferred = bytes_transferred;
}

void PeerClient::asyncHandleConnect(const boost::system::error_code& ec, boost::shared_ptr<boost::asio::deadline_timer> timer, peerConnectCallback handler)
{
    timer->expires_at(boost::posix_time::pos_infin);    
    handler(ec);        
}

void PeerClient::asyncHandleWrite(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler)
{
    timer->expires_at(boost::posix_time::pos_infin);    
    handler(ec, bytes_transferred);
}

void PeerClient::asyncHandleRead(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler)
{
    timer->expires_at(boost::posix_time::pos_infin);    
    handler(ec, bytes_transferred);
}

