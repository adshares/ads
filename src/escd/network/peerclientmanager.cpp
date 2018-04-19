#include "peerclientmanager.h"
#include <memory>
#include <pthread.h>

#include "../options.hpp"
#include "../peer.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

PeerConnectManager::PeerConnectManager(server& server, options& opts):
    m_server(server),
    m_opts(opts),
    m_work(m_ioService),
    m_ioService(),
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
    if(!m_ioService.stopped()){
        m_ioService.stop();
    }

    if(m_ioThread && m_ioThread->joinable()) {
        m_ioThread->join();
    } //try joining yourself error;
}

void PeerConnectManager::start()
{
    timerNextTick(1);
}

void PeerConnectManager::stop()
{
    m_connectTimer.cancel();

    for(auto& peer: m_peers){
        peer.second->stop();
    }
}

void PeerConnectManager::startAccept()
{
    boost::shared_ptr<peer> new_peer(new peer(m_server, true, m_server.getBlockInPorgress(), m_opts, *this));

    //new_peer->killme=true; // leave little time for the connection
    //m_peers[std::make_pair(peer_address.s_addr, port)] = new_peer;
    //new_peer->tryAsyncConnect(iterator, 15);


    //peer_ptr new_peer(new peer(*this,true,srvs_,opts_));
    //acceptor_.async_accept(new_peer->socket(),boost::bind(&server::peer_accept,this,new_peer,boost::asio::placeholders::error));
    m_acceptor.async_accept(new_peer->socket(),boost::bind(&PeerConnectManager::peerAccept, this, new_peer, boost::asio::placeholders::error));
}

void PeerConnectManager::peerAccept(boost::shared_ptr<peer> new_peer, const boost::system::error_code& error)
{
    uint32_t now=time(NULL);

    if (now>= m_server.srvs_now()+BLOCKSEC || error)
    {
        DLOG("WARNING: dropping peer connection while creating new block\n");
        new_peer->stop();
    }
    else
    {

        {
            auto address    = new_peer->socket().remote_endpoint().address().to_string();
            auto port       = new_peer->socket().remote_endpoint().port();

            in_addr_t addr = inet_addr(address.c_str());
            new_peer->accept();

            std::lock_guard<std::mutex> lck(m_peerMx);
            m_peers[std::make_pair(addr, port)] = new_peer;
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
    m_ioService.post(boost::bind(&PeerConnectManager::addActivePeerImpl, this, svid, peer));
}

void PeerConnectManager::addActivePeerImpl(uint16_t svid , boost::shared_ptr<peer> peer)
{
    try{
        std::lock_guard<std::mutex> lck(m_peerMx);
        DLOG("Add active peer svid: %ud", svid);
        if(m_activePeers.find(svid) != m_activePeers.end())
        {
            int test = 0;
            DLOG("ERROR active peer svid: %ud", svid);
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
    in_addr   addr;
    inet_aton(address.c_str(), &addr);
    leevePeer(svid, addr, port);
}

void PeerConnectManager::leevePeer(uint16_t svid, in_addr address, unsigned short port)
{
    m_ioService.post(boost::bind(&PeerConnectManager::leavePeerImpl, this, svid, address, port));
}

void PeerConnectManager::leavePeerImpl(uint16_t svid, in_addr address, unsigned short port)
{
    DLOG("leavePeerImpl svid %d: %d : %d ", svid, address, port);

    try
    {        
        std::lock_guard<std::mutex> lck(m_peerMx);

        auto peerIt = m_peers.find(std::make_pair(address.s_addr, port));
        if(peerIt!=m_peers.end())
        {
            m_server.missing_sent_remove((*peerIt).second->svid);            
            m_activePeers.erase(svid);
            m_peers.erase(peerIt);

            //if(svids.empty() && !opts_.init && !do_sync) {
            if(m_peers.empty() && !m_opts.init && !m_server.do_sync)
            {
                if(!m_server.ofip_isreadonly()) {
                    ELOG("ERROR: no peers, set office readonly\n");
                    m_server.ofip_readonly();
                }
            }

            if (m_activePeers.size() < MIN_PEERS){
                m_timeout = 5;//PANIC_CONN_PERIOD;
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

        boost::asio::ip::tcp::resolver              resolver(m_ioService);
        boost::asio::ip::tcp::endpoint              connectpoint{ address::from_string(inet_ntoa({peer_address})) , port};
        boost::asio::ip::tcp::resolver::iterator    iterator = resolver.resolve(connectpoint);        
        boost::shared_ptr<peer> new_peer(new peer(m_server, false, m_server.getBlockInPorgress(), m_opts, *this));

        //new_peer->killme=true; // leave little time for the connection


        if(alreadyConnected(peer_address.s_addr, port)){
            DLOG("PEER already connected %s : %d\n", inet_ntoa(peer_address), port);
            return;
        }        

        std::lock_guard<std::mutex> lck(m_peerMx);
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


void PeerConnectManager::connect(node& nodeInfo, uint16_t peerId)
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

    for(std::string addr : m_opts.peer)
    {
        if(connNeeded <= 0){
            break;
        }

        std::string  peer_address;
        std::string  port;
        std::string  svid;

        m_opts.get_address(addr, peer_address, port, svid);

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
            srandom(time(NULL));
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
        DLOG("connectNextPeer  \n");

        bool        foundNew{false};
        node        nodeInfo;
        uint16_t    nodeId     = m_server.getRandomNodeId();
        uint16_t    maxNodeId  = m_server.getMaxNodeId();

        if(nodeId == 4)
        {
            int t= 9;

        }
        //@TODO: improve efficiency of finding new node
        for(int i = 0; i<maxNodeId; ++i)
        {
            if(m_server.getNode((nodeId%maxNodeId), nodeInfo))
            {
                if( m_peers.find(std::make_pair(nodeInfo.ipv4, nodeInfo.port)) == m_peers.end() )
                {
                    foundNew = true;
                    break;
                }
            }

            ++nodeId;
        }

        if(foundNew){
            connect(nodeInfo, nodeId);
        }
        --connNeeded;
    }
}

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

    if(!error && neededPeers > 0)
    {
        connectPeersFromConfig(neededPeers);
        connectPeersFromServerFile(neededPeers);

        if(!m_opts.init){
            connectPeersFromDNS(neededPeers);
        }
    }



    DLOG("connectPeer FINISHED \n");

    timerNextTick(m_timeout);
}


bool PeerConnectManager::alreadyConnected(in_addr_t address, unsigned short port)
{
    std::lock_guard<std::mutex> lck(m_peerMx);

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
    std::lock_guard<std::mutex> lck(m_peerMx);
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
    std::lock_guard<std::mutex> lck(m_peerMx);

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
    std::lock_guard<std::mutex> lck(m_peerMx);

    for(auto& peer: m_peers)
    {
        peer.second->update(msg);
    }
}

void PeerConnectManager::getReadyPeers(std::set<uint16_t>& ready)
{
    std::lock_guard<std::mutex> lck(m_peerMx);

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
    std::lock_guard<std::mutex> lck(m_peerMx);
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
    std::lock_guard<std::mutex> lck(m_peerMx);
    if(activeOnly)
    {
        /*int count = 0;
        for(auto& peer: m_peers)
        {
            if(peer.second->svid!=BANK_MAX && !peer.second->do_sync){
                ++count;
            }
        }*/

        return m_activePeers.size();
    }

    return m_peers.size();
}

bool PeerConnectManager::checkDuplicate(uint16_t svid)
{
    std::lock_guard<std::mutex> lck(m_peerMx);
    return m_activePeers.find(svid) != m_activePeers.end();
}

void PeerConnectManager::getMoreHeaders(uint32_t now)
{
    boost::shared_ptr<peer> peer;

    {
        std::lock_guard<std::mutex> lck(m_peerMx);
        auto pi = m_activePeers.begin();

        if(m_activePeers.size()>=1)
        {
            int64_t num = ((uint64_t)random())%m_activePeers.size();
            advance(pi,num);
            peer = pi->second;
        }
        else
        {
            DLOG("REQUEST more headers no peer to request \n");
        }
    }

    if(peer)
    {
        if(!peer->do_sync)
        {
            DLOG("REQUEST more headers from peer %04X\n", peer->svid);
            peer->request_next_headers(now);
        }
    }
}

void PeerConnectManager::fillknown(message_ptr msg)
{
    static uint32_t r=0;

    std::lock_guard<std::mutex> lck(m_peerMx);

    for(auto& peer: m_activePeers)
    {
        msg->know_insert(peer.second->svid);
    }
    /*uint32_t n = m_activePeers.size();
    for(uint32_t i=0; i<n; i++) {
        msg->know_insertm_activePeers[(r+i)%n]);
    }*/
    //r++;
}
