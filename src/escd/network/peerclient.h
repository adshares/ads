#ifndef PEERCLIENT_H
#define PEERCLIENT_H

#include <memory>
#include <boost/asio.hpp>
#include <boost/thread/future.hpp>

#define DEFAULT_NET_TIMEOUT 15

class peer;

typedef boost::function<void(const boost::system::error_code&, size_t)> peerCallback;
typedef boost::function<void(const boost::system::error_code&)>         peerConnectCallback;

/*!
 * \brief Peer netwrok client. It is is responnsible for asynchronous operation with timeouts.
 */
class PeerClient
{    
public:
    PeerClient(boost::asio::ip::tcp::socket &socket, peer& peer);

    /** \brief Asynchronous connection to remote peer.
     *
     * \param tcpIterator
     * \param handler       The handler to be called when the connect operation completes.
     * \param timeout       Operation tiemout in seconds. If zero no timeout.
    */
    void asyncConnect(boost::asio::ip::tcp::resolver::iterator& tcpIterator, peerConnectCallback handler, int timeout = DEFAULT_NET_TIMEOUT);

    /** \brief Asynchronous data read from remote peer.
     *
     * \param data      Poinetr to data buffor
     * \param handler   Lenght of data buffor.
     * \param handler   The handler to be called when the connect operation completes.
     * \param timeout   Operation tiemout in seconds. If zero no timeout.
    */
    void asyncRead(void* data, uint32_t len, peerCallback handler, int timeout = DEFAULT_NET_TIMEOUT);

    /** \brief Asynchronous data write to remote peer.
     *
     * \param data     Poinetr to data buffor
     * \param len      Lenght of data buffor.
     * \param handler  The handler to be called when the connect operation completes.
     * \param timeout  Operation tiemout in seconds. If zero no timeout.
    */
    void asyncWrite(void* data, uint32_t len, peerCallback handler, int timeout = DEFAULT_NET_TIMEOUT);

    //expose writeSync and readSync only for peer
    friend class peer;
protected:
    //execute only from peer thread

    /** \brief Synchronous data write operation to remote peer.
     *
     * \param data      Poinetr to data buffor
     * \param len       Lenght of data buffor.
     * \param timeout   Operation tiemout in seconds.
     * \return size of data written.
    */
    std::size_t writeSync(void* data , uint32_t len,  int timeout = DEFAULT_NET_TIMEOUT);

    /** \brief Synchronous data read from remote peer.
     *
     * \param data      Poinetr to data buffor
     * \param len       Lenght of data buffor.
     * \param timeout   Operation tiemout in seconds.
     * \return size of data readed.
    */
    std::size_t readSync(void* data , uint32_t len, int timeout = DEFAULT_NET_TIMEOUT);

private:
    /** \brief Check deadline timer function for asynchronous operations.
     * If timeout pass peer manager will be delete this peer client and peer related to it.
     *
     * \param timer     Deadline timer.
     * \param ec        Result of operation.
    */
    void checkDeadline(boost::shared_ptr<boost::asio::deadline_timer> timer, const boost::system::error_code& ec);

    /** \brief Check deadline timer function for synchronous operations.
     * If timeout pass peer manager will be delete this peer client and peer related to it.
    */
    void checkDeadline();

    /** \brief Handler function for simulating synchronous operations.
     *
     * \param ec                Result of operation.
     * \param bytes_transferred Number of bytes successfully transferred.
    */
    void operationDone(const boost::system::error_code& ec, size_t bytes_transferred);

    /** \brief Handler function for asynchronous connect operations.
     *
     * \param ec                Result of operation.
     * \param bytes_transferred Number of bytes successfully transferred.
     * \param timer             Deadline timer.
     * \param handler           The handler to be called when the connect operation completes.
    */
    void asyncHandleConnect(const boost::system::error_code& ec, boost::shared_ptr<boost::asio::deadline_timer> timer, peerConnectCallback handler);

    /** \brief Handler function for async write operation.
     *
     * \param ec                Result of operation.
     * \param bytes_transferred Number of bytes successfully transferred.
    */
    void asyncHandleWrite(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler);

    /** \brief Handler function for async read operations.
     *
     * \param ec                Result of operation.
     * \param bytes_transferred Number of bytes successfully transferred.
     * \param handler           The handler to be called when the connect operation completes.
     * \param timeout           Operation tiemout in seconds. If zero no timeout.
    */
    void asyncHandleRead(const boost::system::error_code& ec, size_t bytes_transferred, boost::shared_ptr<boost::asio::deadline_timer> timer, peerCallback handler);    

private:
    peer&                           m_peer;                 ///< parent peer class
    boost::asio::ip::tcp::socket&   m_socket;               ///< socket related to parent peer
    boost::asio::deadline_timer     m_deadline;             ///< deadline timer. It is used in synchronus functions.
    boost::system::error_code       m_ec;                   ///< error code for synchronus functions.
    size_t                          m_bytes_transferred{0}; ///< size of data transfered. It is used in synchronus functions.
};


#endif // PEERCLIENT_H
