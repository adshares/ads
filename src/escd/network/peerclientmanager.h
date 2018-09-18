#ifndef PEERCLIENTMANAGER_HPP
#define PEERCLIENTMANAGER_HPP

#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/asio.hpp>
#include <mutex>
#include <memory>
#include <queue>
#include <map>
#include "default.hpp"
#include "message.hpp"

class options;
class peer;
class server;
class node;
class options;

/*!
 * \brief Peer network manager class. It is is responnsible managing connectiona nd operation on peers.
 */
class PeerConnectManager
{
    enum{
        DEF_CONN_ATTEMPT_PERIOD     = 10,   ///< Defualt timeout between attempts to connect to new peers.
        PANIC_CONN_ATTEMPT_PERIOD   = 2,    ///< Timeout between attempts to connect to new peers when we have less peers than minimum quantity.
        MAX_INIT_CONN               = 5,    ///< Connection timeout when we have to low quantity of connected peers.
        PEER_CONNECT_TIMEOUT        = 15    ///< Max time when connection must be accomplished.
    };

public:
    PeerConnectManager(server& server, options& opts);
    ~PeerConnectManager();

    /** Start connectong with peers. */
    void startConnect();
    /** Start accept connectong with peers.*/
    void startAccept();
    /** Stop connection thread and all peers */
    void stop();
    /** Add new peer to list of already connected peers*/
    //void addPeer(std::string address, unsigned short port, boost::shared_ptr<peer> peer);
    /** Add peer to list of active(connected and synced) peers*/
    void addActivePeer(uint16_t svid, boost::shared_ptr<peer> peer);
    /** Remove peer from connected peers*/
    void leevePeer(uint16_t svid, in_addr addr, unsigned short port);
    /** Remove peer from connected peers*/
    void leevePeer(uint16_t svid, std::string address, unsigned short port);


    /** Deliver message to particular peer*/
    void deliver(message_ptr msg, uint16_t svid);
    /** Deliver message to all peers*/
    void deliverToAll(message_ptr msg);    
    void update(message_ptr msg, uint16_t svid);
    void updateAll(message_ptr msg);

    /** Get acitive and not busy peers*/
    void getReadyPeers(std::set<uint16_t>& ready);

    /** Get list with informations about current peers*/
    std::string getActualPeerList();
    /** Get how many peers we have connections with */
    int getPeersCount(bool activeOnly);

    /** Check if we already connected with perr with svid */
    bool checkDuplicate(uint16_t svid);
    /** Request header from random active peer */
    void getMoreHeaders(uint32_t now);
    /** Put information to message with all peers that are already active */
    void fillknown(message_ptr msg);
    /** Print time and type of the last received message from every peer */
    void printLastMsgInfoForPeers();

    friend class peer;
private:
    /** Main connection thread function */
    void ioRun();

    /** Timer timeout method to request connect with new peers if needed. */
    void timerNextTick(int timeout);
    /** Request connect to new peers if needed from options, dns and nodes form server.srv */
    void connectPeers(const boost::system::error_code& error);    
    /** Connect to peer from server.srv */
    void connectPeersFromServerFile(int& connNeeded);
    /** Connect to peer from options.cfg */
    void connectPeersFromConfig(int& connNeeded);
    /** Connect to peer from DNS */
    void connectPeersFromDNS(int& connNeeded);

    /** Accept new peer */
    void peerAccept(boost::shared_ptr<peer> new_peer, const boost::system::error_code& error);
    /** Connect to peer */
    void connect(node& _node, uint16_t svid);
    /** Connect to peer */
    void connect(std::string peer_address, unsigned short port, uint16_t svid);
    /** Connect to peer */
    void connect(in_addr peer_address, unsigned short port, uint16_t svid);
    /** Connect to peer */
    void connect(boost::asio::ip::tcp::endpoint endpoint, uint16_t svid);
    /** Check if peer is already connected */
    bool alreadyConnected(in_addr_t addr, unsigned short port, uint16_t svid);

    /** Remove peer form peer lists (Done in peer manager thread) */
    void leavePeerImpl(uint16_t svid, in_addr address, unsigned short port);
    /** Add peer to connected peer lists (Done in peer manager thread) */
    //void addPeerImpl(std::string address, unsigned short port, boost::shared_ptr<peer> peer);
    /** Add peer to active peer lists (Done in peer manager thread) */
    void addActivePeerImpl(uint16_t svid, boost::shared_ptr<peer> peer);

    /** Deliver message to particular peer (Done in peer manager thread)*/
    void deliverImpl(message_ptr msg, uint16_t svid);
    /** Deliver message to all peers (Done in peer manager thread)*/
    void deliverToAllImpl(message_ptr msg);
    void updateImpl(message_ptr msg, uint16_t svid);
    void updateAllImpl(message_ptr msg);
private:
    options&                                        m_opts;
    server&                                         m_server;    
    std::map<std::pair<in_addr_t, unsigned short>, boost::shared_ptr<peer>>     m_peers;
    std::map<uint16_t, boost::shared_ptr<peer>>     m_activePeers;    
    boost::shared_mutex                             m_peerMx; //finally it should disapear. Access to peers should be only from io_service thread

    boost::asio::io_service                         m_ioService;    
    boost::asio::io_service::work                   m_work;
    boost::asio::ip::tcp::endpoint                  m_endpoint;
    boost::asio::ip::tcp::acceptor                  m_acceptor;    
    boost::asio::deadline_timer                     m_connectTimer;

    uint8_t                                         m_timeout{DEF_CONN_ATTEMPT_PERIOD};
    boost::thread_attributes                        m_threadAttributes;
    std::unique_ptr<boost::thread>                  m_ioThread;
    unsigned int                                    m_sourceCounter{0}; ///< counter which helper to randomlly connect to diffrent peer source (dns, config...)
};

#endif // PEERCLIENTMANAGER_HPP
