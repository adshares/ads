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


class PeerConnectManager
{
    enum{
        DEF_CONN_PERIOD = 5,
        PANIC_CONN_PERIOD = 1
    };

public:
    PeerConnectManager(server& server, options& opts);
    ~PeerConnectManager();

    void start();
    void stop();
    void startAccept();
    void addPeer(std::string address, unsigned short port, boost::shared_ptr<peer> peer);
    void addActivePeer(uint16_t svid, boost::shared_ptr<peer> peer);
    void leevePeer(uint16_t svid, in_addr, unsigned short port);
    void leevePeer(uint16_t svid, std::string address, unsigned short port);

    void deliver(message_ptr msg, uint16_t svid);
    void deliverToAll(message_ptr msg);
    void deliverToReady(message_ptr msg);
    void updateAll(message_ptr msg);

    void getReadyPeers(std::set<uint16_t>& ready);

    std::string getActualPeerList();
    int getPeersCount(bool activeOnly);

    bool checkDuplicate(uint16_t svid);
    void getMoreHeaders(uint32_t now);
    void fillknown(message_ptr msg);

    friend class peer;
private:
    void ioRun();
    void requestConnect();
    void connectPeers(const boost::system::error_code& error);
    void peerAccept(boost::shared_ptr<peer> new_peer, const boost::system::error_code& error);
    void connectPeersFromServerFile(int& connNeeded);
    void connectPeersFromConfig(int& connNeeded);
    void connectPeersFromDNS(int& connNeeded);

    void connect(node& _node);
    void connect(std::string peer_address, unsigned short port);
    void connect(in_addr peer_address, unsigned short port = 8081);
    void connect(boost::asio::ip::tcp::endpoint endpoint);
    void timerNextTick(int timeout);
    void leavePeerImpl(uint16_t svid, in_addr address, unsigned short port);
    void addActivePeerImpl(uint16_t svid, boost::shared_ptr<peer> peer);
    void addPeerImpl(std::string address, unsigned short port, boost::shared_ptr<peer> peer);

    void deliverImpl(message_ptr msg, uint16_t svid);
    void deliverToAllImpl(message_ptr msg);
    void updateAllImpl(message_ptr msg);
    bool alreadyConnected(in_addr_t addr, unsigned short port);

private:
    options&                                        m_opts;
    server&                                         m_server;
    //std::multimap<uint16_t, boost::shared_ptr<peer>>     m_peers;
    std::map<std::pair<in_addr_t, unsigned short>, boost::shared_ptr<peer>>     m_peers;
    std::map<uint16_t, boost::shared_ptr<peer>>     m_activePeers;
    //std::vector<uint16_t>                           m_activePeerSvid;
    boost::shared_mutex                             m_peerMx; //finally it should disapear. Access to peers should be only from io_service thread

    boost::asio::io_service                         m_ioService;
    boost::asio::ip::tcp::endpoint                  m_endpoint;
    boost::asio::ip::tcp::acceptor                  m_acceptor;
    boost::asio::io_service::work                   m_work;
    boost::asio::deadline_timer                     m_connectTimer;

    uint8_t                                         m_timeout{DEF_CONN_PERIOD};
    bool                                            m_stop{false};
    boost::thread_attributes                        m_threadAttributes;
    std::unique_ptr<boost::thread>                  m_ioThread;
    bool                                            m_sourceCounter{0};
};

#endif // PEERCLIENTMANAGER_HPP
