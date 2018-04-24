#include "peerclientmanager.h"
#include <memory>
#include <pthread.h>

#include "../options.hpp"
#include "../peer.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

PeerConnectManager::PeerConnectManager(server& server, options& opts):
    m_opts(opts),
    m_server(server),
    m_work(m_ioService),
    m_endpoint(boost::asio::ip::address::from_string(opts.addr), opts.port),	//TH
    m_acceptor(m_ioService, m_endpoint),
    m_connectTimer(m_ioService)
{
    m_connectTimer.expires_at(boost::posix_time::pos_infin);
    m_ioThread.reset(new boost::thread(boost::bind(&PeerConnectManager::ioRun, this)));

    if(m_ioThread){
        pthread_setname_np(m_ioThread->native_handle(), "PeerMgr");
    }
}

PeerConnectManager::~PeerConnectManager()
{        
    /*char name[16];

    pthread_getname_np(pthread_self(), &name[0], sizeof(name));

    m_activePeers.clear();
    m_peers.clear();


    DLOG("PeerConnectManager MANAGER destructor1 %s\n", name);
    stop();*/

    DLOG("PeerConnectManager MANAGER destructor2\n");
    if(!m_ioService.stopped()){
        m_ioService.stop();
    }


    if (boost::asio::detail::call_stack<PeerConnectManager>::contains(this))
    {
        DLOG("PeerConnectManager MANAGER destructor7777\n");
    }
    DLOG("PeerConnectManager MANAGER destructor3\n");
    if(m_ioThread && m_ioThread->joinable()) {
        //m_ioThread->interrupt();
        m_ioThread->join();
    } //try joining yourself error;

    DLOG("PeerConnectManager MANAGER destructor2\n");
}

void PeerConnectManager::start()
{
    timerNextTick(1);
}

void PeerConnectManager::stop()
{
    DLOG("Server MANAGER STOP\n");

    m_connectTimer.cancel();

    for(auto& peer: m_peers){
        peer.second->stop();
    }

    if(!m_ioService.stopped()){
        m_ioService.stop();
    }

}

void PeerConnectManager::startAccept()
{
    boost::shared_ptr<peer> new_peer(new peer(m_server, true, m_server.getBlockInPorgress(), m_opts, *this, "", 0));
    DLOG("..................peerAccept INIT %d", new_peer->m_peerId);
    m_acceptor.async_accept(new_peer->socket(),boost::bind(&PeerConnectManager::peerAccept, this, new_peer, boost::asio::placeholders::error));
}

void PeerConnectManager::peerAccept(boost::shared_ptr<peer> new_peer, const boost::system::error_code& error)
{

    DLOG("..................peerAccept START %d", new_peer->m_peerId);
    uint32_t now=time(NULL);

    if (now>= m_server.srvs_now()+BLOCKSEC || error)
    {
        DLOG("WARNING: dropping peer connection while creating new block\n");
        new_peer->stop();
        DLOG("WARNING: dropping peer connection while creating new block %d\n", new_peer->m_peerId);
    }
    else
    {
        auto address    = new_peer->socket().remote_endpoint().address().to_string();
        auto port       = new_peer->socket().remote_endpoint().port();


        in_addr_t addr = inet_addr(address.c_str());

        if(alreadyConnected(addr, port)){
            new_peer->stop();
            DLOG("WARNING: dropping PEER already connected %s : %d\n", address.c_str(), port);
        }
        else
        {
            new_peer->accept();

            boost::unique_lock< boost::shared_mutex > lock(m_peerMx);
            m_peers[std::make_pair(addr, port)] = new_peer;
            DLOG("peerAccept inet addr %s port: %d \n", address.c_str(), port);
        }
    }

    startAccept();
}

void PeerConnectManager::ioRun()
{
    try {
        m_ioService.run();
    } //Now we know the server is down.
    catch (std::exception& e) {
        ELOG("Server.Run error: %s\n",e.what());
    }
}

void PeerConnectManager::addActivePeer(uint16_t svid, boost::shared_ptr<peer> peer)
{
    DLOG("Add active thread peer svid: %ud\n", svid);
    m_ioService.dispatch(boost::bind(&PeerConnectManager::addActivePeerImpl, this, svid, peer));
}

void PeerConnectManager::addPeer(std::string address, unsigned short port, boost::shared_ptr<peer> peer)
{
    DLOG("Add active thread peer port: %ud\n",port);
    m_ioService.dispatch(boost::bind(&PeerConnectManager::addPeerImpl, this, address, port, peer));
    //addPeerImpl(address, port, peer);
}

void PeerConnectManager::addPeerImpl(std::string address, unsigned short port, boost::shared_ptr<peer> peer)
{
    try{
        boost::unique_lock< boost::shared_mutex > lock(m_peerMx);
        DLOG("Add active peer port: %ud\n", port);

        in_addr   addr;
        inet_aton(address.c_str(), &addr);

        if(alreadyConnected(addr.s_addr, port)){
            DLOG("WARNING!!!: dropping PEER already connected %s:%d\n",address.c_str(),  port);
            return;
        }

        m_peers[std::make_pair(addr.s_addr, port)] = peer;
    }
    catch(std::exception &e)
    {
        ELOG("ERROR: Leave peer exception%s", e.what());
    }
}

void PeerConnectManager::addActivePeerImpl(uint16_t svid , boost::shared_ptr<peer> peer)
{
    try{
        boost::unique_lock< boost::shared_mutex > lock(m_peerMx);
        DLOG("Add active peer svid: %ud\n", svid);
        if(m_activePeers.find(svid) != m_activePeers.end())
        {
            DLOG("ERROR active peer svid: %ud\n", svid);
        }
        m_activePeers[svid] = peer;
    }
    catch(std::exception &e)
    {
        ELOG("ERROR: Leave peer exception%s", e.what());
    }
}

void PeerConnectManager::leevePeer(uint16_t svid, std::string address, unsigned short port)
{
    DLOG("leavePeer svid %d: %s : %d \n", svid, address.c_str(), port);
    in_addr   addr;
    inet_aton(address.c_str(), &addr);
    DLOG("leavePeer IIINNNNNN\n");
    leevePeer(svid, addr, port);
    DLOG("leavePeer IIIOUT\n");
}

void PeerConnectManager::leevePeer(uint16_t svid, in_addr address, unsigned short port)
{
    DLOG("leavePeer svid %d: %d : %d \n", svid, address, port);
    m_ioService.dispatch(boost::bind(&PeerConnectManager::leavePeerImpl, this, svid, address, port));
}

void PeerConnectManager::leavePeerImpl(uint16_t svid, in_addr address, unsigned short port)
{
    DLOG("leavePeerImpl svid %d: %d : %d ", svid, address, port);

    try
    {
        boost::unique_lock< boost::shared_mutex > lock(m_peerMx);



        auto peerIt = m_peers.find(std::make_pair(address.s_addr, port));

        //assert(peerIt!=m_peers.end());

        if(peerIt!=m_peers.end())
        {
            DLOG("leavePeerImpl done svid %d: %d : %d ", svid, address, port);

            auto activePeer = m_activePeers.find(svid);

            if(activePeer != m_activePeers.end())
            {
                if(activePeer->second->port == port)
                {
                    m_activePeers.erase(svid);
                    m_server.missing_sent_remove((*peerIt).second->svid);
                }
                else
                {
                    DLOG("leavePeerImpl active is ok svid %d: %d : %d ", svid, address, port);
                }
            }

            m_peers.erase(peerIt);

            if(m_peers.empty() && !m_opts.init && !m_server.do_sync)
            {
                if(!m_server.ofip_isreadonly()) {
                    ELOG("ERROR: no peers, set office readonly\n");
                    m_server.ofip_readonly();
                }
            }

            if (m_activePeers.size() < MIN_PEERS)
            {
                m_timeout = DEF_CONN_PERIOD;//PANIC_CONN_PERIOD;
            }
            else{
                m_timeout = DEF_CONN_PERIOD;
            }
        }


        timerNextTick(m_timeout);
    }
    catch(std::exception &e)
    {
        ELOG("ERROR: Leave peer exception%s", e.what());
    }
}

void PeerConnectManager::connect(in_addr peer_address, unsigned short port)
{
    try
    {
        DLOG("...........................TRY connecting to peer %s : %d\n", inet_ntoa(peer_address), port);

        if(alreadyConnected(peer_address.s_addr, port)){
            DLOG("PEER already connected %s : %d\n", inet_ntoa(peer_address), port);
            return;
        }

        boost::asio::ip::tcp::resolver              resolver(m_ioService);
        boost::asio::ip::tcp::endpoint              connectpoint{ address::from_string(inet_ntoa({peer_address})) , port};
        boost::asio::ip::tcp::resolver::iterator    iterator = resolver.resolve(connectpoint);

        boost::shared_ptr<peer> new_peer(new peer(m_server, false, m_server.getBlockInPorgress(), m_opts, *this, inet_ntoa({peer_address}), port));

        boost::unique_lock< boost::shared_mutex > lock(m_peerMx);
        m_peers[std::make_pair(peer_address.s_addr, port)] = new_peer;
        new_peer->tryAsyncConnect(iterator, 15);

    } catch (std::exception& e){
            DLOG("Connection: %s\n",e.what());
    }
}

void PeerConnectManager::connect(boost::asio::ip::tcp::endpoint endpoint)
{
    auto addr = endpoint.address().to_string();
    auto port = endpoint.port();

    in_addr   inaddr;
    if(inet_aton(addr.c_str(), &inaddr) == 0){
        connect(inaddr, port);
    }
}


void PeerConnectManager::connect(node& nodeInfo)
{
    in_addr addr{nodeInfo.ipv4};

    connect(addr, static_cast<unsigned short>(nodeInfo.port));
}

void PeerConnectManager::connect(std::string peer_address, unsigned short port)
{
    in_addr   addr;
    if(inet_aton(peer_address.c_str(), &addr) != 0 ){
        connect(addr, port);
    }
}

void PeerConnectManager::connectPeersFromConfig(int& connNeeded)
{
    DLOG("TRY CONNECT connectPeersFromConfig\n");
    for(std::string addr : m_opts.peer)
    {
        if(connNeeded <= 0){
            break;
        }

        std::string  peer_address;
        std::string  port;
        std::string  svid;

        m_opts.get_address(addr, peer_address, port, svid);

        DLOG("TRY CONNECT to dns peer (%s:%s)\n", peer_address.c_str(), port.c_str());
        connect(peer_address, atoi(port.c_str()));
        --connNeeded;
    }
}

void PeerConnectManager::connectPeersFromDNS(int& connNeeded)
{
    try
    {
        boost::asio::ip::tcp::resolver              resolver(m_ioService);
        boost::asio::ip::tcp::resolver::query       query(m_opts.dnsa.c_str(),SERVER_PORT);
        boost::asio::ip::tcp::resolver::iterator    iterator = resolver.resolve(query);
        boost::asio::ip::tcp::resolver::iterator    end;
        std::map<uint32_t,boost::asio::ip::tcp::resolver::iterator> endpoints;

        while(iterator != end)
        {
            uint32_t r=random()%0xFFFFFFFF;
            endpoints[r]=iterator++;
        }

        for(auto ep=endpoints.begin(); ep!=endpoints.end(); ep++)
        {
            if(connNeeded <= 0){
                break;
            }

            DLOG("TRY CONNECT to dns peer (%s:%d)\n",ep->second->endpoint().address().to_string().c_str(),
                 ep->second->endpoint().port());
                 connect(ep->second->endpoint());
                 --connNeeded;
        }
    }
    catch (std::exception& e) {
        ELOG("DNS Connect Exception: %s", e.what());
    }
}

void PeerConnectManager::connectPeersFromServerFile(int& connNeeded)
{
    while(connNeeded>0)
    {
        DLOG("connectPeersFromServerFile \n");

        bool        foundNew{false};
        node        nodeInfo;
        uint16_t    nodeId     = m_server.getRandomNodeId();
        uint16_t    maxNodeId  = m_server.getMaxNodeId();

        //@TODO: improve efficiency of finding new node
        for(int i = 0; i<maxNodeId; ++i)
        {
            //Don't connect to myself
            if(nodeId != m_opts.svid && m_server.getNode(nodeId, nodeInfo))
            {
                if(nodeInfo.port > 0
                        && m_peers.find(std::make_pair(nodeInfo.ipv4, nodeInfo.port)) == m_peers.end() )
                {
                    foundNew = true;
                    break;
                }
            }

            nodeId = (++nodeId)%maxNodeId;
        }

        if(foundNew){
            DLOG("connectPeersFromServerFile found\n");
           connect(nodeInfo);
           --connNeeded;
           return;
        }
        --connNeeded;
    }
}

/*void PeerConnectManager::connectPeersFromServerFile(int& connNeeded)
{
    //while(connNeeded>0)
    {
        DLOG("connectPeersFromServerFile \n");

        bool        foundNew{false};
        node        nodeInfo;
        uint16_t    nodeId     = m_server.getRandomNodeId();
        uint16_t    maxNodeId  = m_server.getMaxNodeId();

        for(int i=1; i<=maxNodeId; i++)
        {
            m_server.getNode(i, nodeInfo);

            if(nodeInfo.port > 0 && i !=m_opts.svid)
            {
                connect(nodeInfo);
            }
        }

    }
}*/

void PeerConnectManager::timerNextTick(int timeout)
{
    DLOG("timerNextTick  \n");

    m_connectTimer.expires_from_now(boost::posix_time::seconds(timeout));
    m_connectTimer.async_wait(boost::bind(&PeerConnectManager::connectPeers, this, boost::asio::placeholders::error));
}

void PeerConnectManager::connectPeers(const boost::system::error_code& error)
{
    DLOG("connectPeer : %d \n", error.value());

    if(error ==  boost::asio::error::operation_aborted){
        return;
    }

    int neededPeers = MAX_PEERS - m_peers.size();
    neededPeers = neededPeers > 4 ? 4 : neededPeers ;

    DLOG("connectPeer needed: %d \n", neededPeers);

    if(!error && neededPeers > 0)
    {
        /*if( (m_sourceCounter++)%10 <= 1) {
            connectPeersFromConfig(neededPeers);
        }

        if(!m_opts.init && m_server.getMaxNodeId() < 2){
            connectPeersFromDNS(neededPeers);
        }*/

        connectPeersFromServerFile(neededPeers);
    }

    DLOG("connectPeer FINISHED \n");

    timerNextTick(m_timeout);
}


bool PeerConnectManager::alreadyConnected(in_addr_t address, unsigned short port)
{
    DLOG("PEER alreadyConnected \n");

    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    DLOG("PEER alreadyConnected2 \n");

    for(auto& peer2: m_peers)
    {
        DLOG("PEER \n");
        struct in_addr add{address};

        DLOG("%d PEER alreadyConnected to port %s, %d\n", peer2.second->svid, peer2.second->addr.c_str(), peer2.second->port);
    }

    auto peer = m_peers.find(std::make_pair(address, port));



    if( peer != m_peers.end()){
        DLOG("%d PEER alreadyConnected address %d\n", peer->second->svid, port);
        return true;
    }

    return false;
}

void PeerConnectManager::deliver(message_ptr msg, uint16_t svid)
{
    m_ioService.dispatch(boost::bind(&PeerConnectManager::deliverImpl, this, msg, svid));
}

void PeerConnectManager::deliverImpl(message_ptr msg, uint16_t svid)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    //TODO think about speed up. Maybe m_peers should be map with svid as a key.
    for(auto& peer: m_peers)
    {
        if(peer.second->svid == svid){
            peer.second->deliver(msg);
            return;
        }
    }
    msg->sent_erase(svid);
}

void PeerConnectManager::deliverToAll(message_ptr msg)
{
    m_ioService.dispatch(boost::bind(&PeerConnectManager::deliverToAllImpl, this, msg));
}


void PeerConnectManager::deliverToAllImpl(message_ptr msg)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    for(auto& peer: m_peers)
    {
        peer.second->deliver(msg);
    }
}

void PeerConnectManager::updateAll(message_ptr msg)
{
    m_ioService.dispatch(boost::bind(&PeerConnectManager::updateAllImpl, this, msg));
}

void PeerConnectManager::updateAllImpl(message_ptr msg)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    for(auto& peer: m_peers)
    {
        peer.second->update(msg);
    }
}

void PeerConnectManager::getReadyPeers(std::set<uint16_t>& ready)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    for(auto& peer: m_activePeers)
    {
        if(!peer.second->busy){
            ready.insert(peer.second->svid);
        }
    }
}

std::string PeerConnectManager::getActualPeerList()
{
    //TODO set member string value for it and get copyt if needed.
    // string value should be cheanged when m_peers is changing.
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    std::stringstream ss;

    for(auto& peer: m_activePeers)
    {
        ss << ",";
        if((peer.second)->svid!=BANK_MAX) {
            ss << std::to_string((peer.second)->svid);
        } else {
            ss << "-";
        }
        if((peer.second)->killme) {
            ss << "*";
        }
    }
    return ss.str();
}

int PeerConnectManager::getPeersCount(bool activeOnly)
{
    DLOG("SYLWESTER GetPeersCount\n");
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    if(activeOnly){
         DLOG("SYLWESTER GetPeersCount %d - %ud\n",activeOnly, m_activePeers.size());
        return m_activePeers.size();
    }

    DLOG("SYLWESTER GetPeersCount2 %d - %ud \n",activeOnly, m_peers.size());
    return m_peers.size();
}

bool PeerConnectManager::checkDuplicate(uint16_t svid)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    return m_activePeers.find(svid) != m_activePeers.end();
}

void PeerConnectManager::getMoreHeaders(uint32_t now)
{
    DLOG("TRY REQUEST getMoreHeaders \n");

    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    if(m_activePeers.size() == 0)
    {
        DLOG("REQUEST NO active PEERS!! \n");
        return;
    }

    auto    peer  = m_activePeers.begin();
    int     num   = random() % (m_activePeers.size());

    advance(peer,num);

    if(peer != m_activePeers.end())
    {
        assert(peer->second);
        if(!peer->second->do_sync)
        {
            DLOG("REQUEST more headers from sync peer %04X\n", peer->second->svid);
            peer->second->request_next_headers(now);
        }
    }
    else
    {
        DLOG("REQUEST more headers no peer to request \n");
    }

    DLOG("TRY REQUEST getMoreHeaders FINISHED num %d, active %u\n", num, m_activePeers.size());

    /*for(auto& peer: m_activePeers)
    {
        DLOG("REQUEST more headers from sync peer %04X\n", peer.second->svid);
        peer.second->request_next_headers(now);
    }

    if(m_activePeers.size() == 0)
    {
        DLOG("REQUEST more headers no peer to request \n");
    }*/

}

void PeerConnectManager::fillknown(message_ptr msg)
{
    DLOG("FILLKNOW peers \n")

    static uint32_t r=0;

    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    if(m_activePeers.size() == 0){
        DLOG("FILLKNOW peers list empty \n");
        return;
    }

    auto pi = m_activePeers.begin();
    //advance(pi,(r%m_activePeers.size()));

    for(auto it = pi; it != m_activePeers.end(); it++)
    {
        msg->know_insert(pi->second->svid);
        DLOG("FILLKNOW peers: %d \n", pi->second->svid);
    }
    for(auto it = m_activePeers.begin(); it != pi; it++)
    {
        msg->know_insert(pi->second->svid);
        DLOG("FILLKNOW peers: %d \n", pi->second->svid);
    }
    ++r;
}
