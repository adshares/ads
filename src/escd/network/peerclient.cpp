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

PeerClient::~PeerClient()
{
    DLOG("PeerClient destructor\n");
}

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
      DLOG(".....................------------check_deadline error  %d \n", ec.value());


      // The deadline has passed.
      // asynchronous operations are cancelled.

      //m_ec = boost::asio::error::timed_out;
      //m_socket.cancel();
      //m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);

      DLOG("\nDEADLINE.....................check_deadline error2");
      m_peer.stop();
      DLOG("\nDEADLINE.....................check_deadline error3");

      /*if(m_socket.is_open())
      {
          DLOG("\nDEADLINE.....................check_deadline error2");
          m_socket.cancel();
          m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
          //m_socket.close();
          DLOG("\nDEADLINE.....................check_deadline error3");
        //DLOG("PEER KILL TIMEOUT locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
      }*/


      // There is no longer an active deadline. The expiry is set to positive
      // infinity so that the actor takes no action until a new deadline is set.
      timer->expires_at(boost::posix_time::pos_infin);
    }

    //DLOG(".....................check_deadline OK");
    // Put the actor back to sleep.
    //boost::system::error_code ec2 = boost::system::error_code(boost::asio::error::would_block);
    timer->async_wait(boost::bind(&PeerClient::checkDeadline, this, timer, boost::asio::placeholders::error));
}

void PeerClient::checkDeadline()
{
    //DLOG(".....................check_deadline EC\n");

    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (m_deadline.expires_at() <= deadline_timer::traits_type::now())
    {
      DLOG("\nDEADLINE.....................check_deadline error");

      // The deadline has passed. The socket is closed so that any outstanding
      // asynchronous operations are cancelled.
      //m_socket.get_io_service().reset();
      //m_socket.cancel()
      //m_ec = boost::asio::error::timed_out;;

      /*if(m_socket.is_open())
      {

          //m_socket.close();
          m_socket.cancel();
          m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        //m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        //DLOG("%04X PEER KILL TIMEOUT %d<\n",m_socket.local_endpoint().port());
        //DLOG("PEER KILL TIMEOUT locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
      }*/
      m_peer.stop();
      //m_socket.cancel();
      //m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);

      // There is no longer an active deadline. The expiry is set to positive
      // infinity so that the actor takes no action until a new deadline is set.
      m_deadline.expires_at(boost::posix_time::pos_infin);
    }

    //DLOG(".....................check_deadline OK");
    // Put the actor back to sleep.
    //boost::system::error_code ec2 = boost::system::error_code(boost::asio::error::would_block);
    m_deadline.async_wait(boost::bind(&PeerClient::checkDeadline, this));
 }

std::size_t PeerClient::writeSync(void* data , uint32_t len,  int timeout)
{    
    DLOG("PEER PeerClient::writeSync type %d len %d \n", ((char*)data)[0], len);

    m_lastTimeout   =timeout;
    m_lastOperation = 1;
    m_lastData      = ((char*)data)[0];
    m_lastLenght    = len;

    if(timeout > 0){
        m_deadline.expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline();
    }

    m_bytes_transferred = 0;
    m_ec = boost::asio::error::would_block;
    boost::asio::async_write(m_socket, boost::asio::buffer(data, len), boost::bind(&PeerClient::operationDone, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));



    do{
        m_socket.get_io_service().run_one();
    }
    while (m_ec == boost::asio::error::would_block);

    m_deadline.expires_at(boost::posix_time::pos_infin);

    if (m_ec) {
         throw boost::system::system_error(m_ec);
    }

    assert(m_bytes_transferred==len);

    DLOG("PEER PeerClient::writeSync finish %d\n", m_bytes_transferred);

    return m_bytes_transferred;
}

std::size_t PeerClient::readSync(void* data , uint32_t len,  int timeout)
{
    DLOG("PEER PeerClient::readSync type %d len %d \n", ((char*)data)[0], len);
    m_lastTimeout   =timeout;
    m_lastOperation = 2;
    m_lastData      = ((char*)data)[0];
    m_lastLenght    = len;



    if(timeout > 0){
        m_deadline.expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline();
    }        

    m_ec = boost::system::error_code(boost::asio::error::would_block);
    m_bytes_transferred = 0;
    boost::asio::async_read(m_socket, boost::asio::buffer(data, len), boost::bind(&PeerClient::operationDone, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));



    /*if(m_socket.is_open())
    {
        if(len>0)
        {
            DLOG("PEER PeerClient::readSync type %d len %d \n", ((char*)data)[0], len);
        }

        //DLOG("PEER PeerClient::readSync locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
    }*/

    do{
        m_socket.get_io_service().run_one();
    }
    while (m_ec == boost::asio::error::would_block);

    m_deadline.expires_at(boost::posix_time::pos_infin);

    if (m_ec) {
         throw boost::system::system_error(m_ec);
    }

    assert(m_bytes_transferred==len);       
    DLOG("PEER PeerClient::readSync finish %d\n", m_bytes_transferred);

    return m_bytes_transferred;
}

void PeerClient::asyncRead(void* data, uint32_t len, peerCallback handler, int timeout)
{    
    DLOG("PEER PeerClient::asyncRead type %d len %d \n", ((char*)data)[0], len);

    m_lastTimeout   =timeout;
    m_lastOperation = 3;
    m_lastData      = ((char*)data)[0];
    m_lastLenght    = len;


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
    DLOG("PEER PeerClient::asyncConnect \n");

    m_lastTimeout   =timeout;
    m_lastOperation = 5;
    m_lastData      = 0;//((char*)data)[0];
    m_lastLenght    = -1;


    boost::shared_ptr<deadline_timer> timer = boost::make_shared<deadline_timer>(m_socket.get_io_service());

    if(timeout > 0)
    {
        timer->expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline(timer, boost::asio::error::timed_out);
    }

    boost::asio::async_connect(m_socket, tcpIterator, boost::bind(&PeerClient::asyncHandleConnect, this, boost::asio::placeholders::error, timer, handler));

    DLOG("PEER PeerClient::asyncConnect2 \n");

    //if(m_socket.is_open())
    {
        //DLOG("PEER PeerClient::asyncConnect locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
    }
    DLOG("PEER PeerClient::asyncConnect3 \n");
}

void PeerClient::asyncWrite(void* data, uint32_t len, peerCallback handler, int timeout)
{    
    DLOG("PEER PeerClient::asyncWrite type %d len %d \n", ((char*)data)[0], len);

    m_lastTimeout   =timeout;
    m_lastOperation = 4;
    m_lastData      = ((char*)data)[0];
    m_lastLenght    = len;


    boost::shared_ptr<deadline_timer> timer = boost::make_shared<deadline_timer>(m_socket.get_io_service());

    if(timeout > 0){
        timer->expires_from_now(boost::posix_time::seconds(timeout));
        checkDeadline(timer, boost::asio::error::timed_out);
    }

    boost::asio::async_write(m_socket, boost::asio::buffer(data, len), boost::bind(&PeerClient::asyncHandleWrite, this,
                                            boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, timer, handler));

    /*if(m_socket.is_open())
    {
        if(len>0)
        {
            DLOG("PEER PeerClient::asyncWrite type %d len %d \n", ((char*)data)[0], len);
        }

        //DLOG("PEER PeerClient::asyncWrite locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
    }*/
}


void PeerClient::operationDone(const boost::system::error_code& ec, size_t bytes_transferred)
{    
    DLOG("PEER PeerClient::operationDone start bytes %d res %d\n", m_bytes_transferred, ec.value());

    /*if (ec.value() == 2)
    {
        DLOG("PEER PeerClient:: 2\n");
        throw boost::system::system_error(ec);
        //timer->cancel();
        return;
    }*/

    m_deadline.expires_at(boost::posix_time::pos_infin);
    m_ec = ec;
    m_bytes_transferred = bytes_transferred;


    //if(m_socket.is_open())
    {
        //DLOG("PEER PeerClient::operationDone locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
    }

    DLOG("PEER PeerClient::operationDone finish bytes %d res %d\n", m_bytes_transferred, ec.value());

}

void PeerClient::asyncHandleConnect(const boost::system::error_code& ec, boost::shared_ptr<boost::asio::deadline_timer> timer, peerConnectCallback handler)
{
    DLOG("PEER PeerClient::asyncHandleConnect start  res %d\n", ec.value());

    /*if (ec.value() == 2)
    {
        DLOG("PEER PeerClient:: 2\n");
        throw boost::system::system_error(ec);
        //timer->cancel();
        return;
    }*/

    //if(m_socket.is_open())
    {
        //DLOG("PEER PeerClient::asyncHandleConnect locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
    }

    timer->expires_at(boost::posix_time::pos_infin);    
    handler(ec);        
}


void PeerClient::asyncHandleWrite(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler)
{
    DLOG("PEER PeerClient::asyncHandleWrite start  res %d\n", ec.value());

    /*if (ec.value() == 2)
    {
        DLOG("PEER PeerClient:: 2\n");
        //timer->cancel();
        throw boost::system::system_error(ec);
        return;
    }*/
    /*if (ec == boost::asio::error::operation_aborted) {
        return;
    }*/

    //if(m_socket.is_open())
    {
        //DLOG("PEER PeerClient::asyncHandleWrite locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
    }

    timer->expires_at(boost::posix_time::pos_infin);    
    handler(ec, bytes_transferred);
}

void PeerClient::asyncHandleRead(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler)
{
    DLOG("PEER PeerClient::asyncHandleRead start  res %d\n", ec.value());

    /*if (ec.value() == 2)
    {
        DLOG("PEER PeerClient:: 2\n");
        throw boost::system::system_error(ec);
        //timer->cancel();
        return;
    }*/
    /*if (ec == boost::asio::error::operation_aborted) {
        return;
    }*/

    //if(m_socket.is_open())
    {
        //DLOG("PEER PeerClient::asyncHandleRead locaddr %s:%d remote %s:%d \n",m_socket.local_endpoint().address().to_string().c_str(),
        //     m_socket.local_endpoint().port(), m_socket.remote_endpoint().address().to_string().c_str(),
        //     m_socket.remote_endpoint().port());
    }
    timer->expires_at(boost::posix_time::pos_infin);    
    handler(ec, bytes_transferred);
}

