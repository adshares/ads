#include "peerclientmanager.h"
#include <memory>
#include <pthread.h>
#include <iterator>

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

#ifdef LINUX
    if(m_ioThread){
        pthread_setname_np(m_ioThread->native_handle(), "PeerMgr");
    }
#endif
}

PeerConnectManager::~PeerConnectManager()
{            
    stop();
    m_activePeers.clear();
    m_peers.clear();

    if(!m_ioService.stopped()){
        m_ioService.stop();
    }

    if(m_ioThread && m_ioThread->joinable()) {
        m_ioThread->join();
    } //try joining yourself error;

    DLOG("PeerConnectManager destructor finished!!!\n");
}

void PeerConnectManager::startConnect()
{
    DLOG("PEER_MANAGER START CONNECT\n");
    timerNextTick(1);
}

void PeerConnectManager::stop()
{
    DLOG("Server MANAGER STOP\n");

    m_connectTimer.cancel();

    for(auto& peer: m_peers){
        peer.second->leave();
    }

    if(!m_ioService.stopped()){
        m_ioService.stop();
    }
}

void PeerConnectManager::startAccept()
{
    DLOG("PEER_MANAGER START ACCEPT\n");
    boost::shared_ptr<peer> new_peer(new peer(m_server, true, m_server.getBlockInPorgress(), m_opts, *this, "", 0));
    m_acceptor.async_accept(new_peer->socket(),boost::bind(&PeerConnectManager::peerAccept, this, new_peer, boost::asio::placeholders::error));
}

void PeerConnectManager::peerAccept(boost::shared_ptr<peer> new_peer, const boost::system::error_code& error)
{    
    if(m_peers.size() >= MAX_PEERS) {
        ILOG("%04X PEER DROPPED, max connections reached\n", new_peer->svid);
        new_peer->leave();
    } else {
        DLOG("PEER ACCEPT %04X\n", new_peer->svid);

        uint32_t now    = time(NULL);
        auto address    = new_peer->socket().remote_endpoint().address().to_string();
        auto port       = new_peer->socket().remote_endpoint().port();
        in_addr_t addr  = inet_addr(address.c_str());

        if (now>= m_server.srvs_now()+BLOCKSEC || error || alreadyConnected(addr, port, new_peer->svid) )
        {
            new_peer->leave();
            WLOG("WARNING: dropping connection %d\n", new_peer->svid);
        }
        else
        {
            new_peer->accept();

            boost::upgrade_lock< boost::shared_mutex > lock(m_peerMx);
            m_peers[std::make_pair(addr, port)] = new_peer;
            DLOG("PEER ACCEPT inet addr %s port: %d \n", address.c_str(), port);
        }
    }

    startAccept();
}

void PeerConnectManager::ioRun()
{
    for (;;)
    {
        try {
            DLOG("before io_service::run()\n");
            std::size_t val = m_ioService.run();
            DLOG("after io_service::run(), returned value = %lu\n", val);
            break; // run exited normally
        } //Now we know the server is down.
        catch (std::exception& e) {
            WLOG("Server.Run error: %s\n",e.what());
            RETURN_ON_SHUTDOWN();
        }
    }

    DLOG("after for loop\n");
}

void PeerConnectManager::addActivePeer(uint16_t svid, boost::shared_ptr<peer> peer)
{
    DLOG("Add active thread peer svid: %ud\n", svid);
    m_ioService.dispatch(boost::bind(&PeerConnectManager::addActivePeerImpl, this, svid, peer));
}

void PeerConnectManager::addActivePeerImpl(uint16_t svid , boost::shared_ptr<peer> peer)
{
    try{
        boost::upgrade_lock< boost::shared_mutex > lock(m_peerMx);
        DLOG("Add active peer svid: %ud\n", svid);
        if(m_activePeers.find(svid) != m_activePeers.end())
        {
            WLOG("ERROR active peer svid: %ud\n", svid);
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
    DLOG("%04X PEER LEAVE \n", svid);

    try
    {
        bool cleanMissing = false;

        {
            boost::upgrade_lock< boost::shared_mutex > lock(m_peerMx);

            auto peerIt = m_peers.find(std::make_pair(address.s_addr, port));

            if(peerIt!=m_peers.end())
            {
                auto activePeer = m_activePeers.find(svid);

                if(activePeer != m_activePeers.end())
                {
                    if(activePeer->second->port == port)
                    {
                        m_activePeers.erase(svid);
                        cleanMissing = true;
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

                if (m_activePeers.size() < MIN_PEERS){
                    m_timeout = PANIC_CONN_ATTEMPT_PERIOD;
                }
                else{
                    m_timeout = DEF_CONN_ATTEMPT_PERIOD;
                }
            }
        } //end of boost::upgrade_lock

        if(cleanMissing){
            m_server.missing_sent_remove(svid);
        }

        DLOG("%04X PEER LEAVE FINISHED \n", svid);
    }
    catch(std::exception &e)
    {
        ELOG("ERROR: Leave peer exception%s", e.what());
    }

    timerNextTick(m_timeout);
}

void PeerConnectManager::connect(in_addr peer_address, unsigned short port, uint16_t svid)
{
    try
    {
        DLOG("TRY CONNECT to peer %s : %d\n", inet_ntoa(peer_address), port);

        if(alreadyConnected(peer_address.s_addr, port, svid)){
            DLOG("PEER already connected %s : %d\n", inet_ntoa(peer_address), port);
            return;
        }

        boost::asio::ip::tcp::resolver              resolver(m_ioService);
        boost::asio::ip::tcp::endpoint              connectpoint{ address::from_string(inet_ntoa({peer_address})) , port};
        boost::asio::ip::tcp::resolver::iterator    iterator = resolver.resolve(connectpoint);

        boost::shared_ptr<peer> new_peer(new peer(m_server, false, m_server.getBlockInPorgress(), m_opts, *this, inet_ntoa({peer_address}), port));

        boost::upgrade_lock< boost::shared_mutex > lock(m_peerMx);
        m_peers[std::make_pair(peer_address.s_addr, port)] = new_peer;
        new_peer->tryAsyncConnect(iterator, PEER_CONNECT_TIMEOUT);

    } catch (std::exception& e){
            ELOG("Connection: %s\n",e.what());
    }
}

void PeerConnectManager::connect(boost::asio::ip::tcp::endpoint endpoint, uint16_t svid)
{
    auto addr = endpoint.address().to_string();
    auto port = endpoint.port();

    in_addr   inaddr;
    if(inet_aton(addr.c_str(), &inaddr) != 0){
        connect(inaddr, port, svid);
    }

}

void PeerConnectManager::connect(node& nodeInfo, uint16_t svid)
{
    in_addr addr{nodeInfo.ipv4};

    connect(addr, static_cast<unsigned short>(nodeInfo.port), svid);
}

void PeerConnectManager::connect(std::string peer_address, unsigned short port, uint16_t svid)
{
    in_addr   addr;
    if(inet_aton(peer_address.c_str(), &addr) != 0 ){
        connect(addr, port, svid);
    } else {
        boost::asio::ip::tcp::resolver              resolver(m_ioService);
        boost::asio::ip::tcp::resolver::query       query(peer_address.c_str(),SERVER_PORT);
        boost::asio::ip::tcp::resolver::iterator    iterator = resolver.resolve(query);
        boost::asio::ip::tcp::resolver::iterator    end;


        if(iterator != end)
        {
            connect(iterator->endpoint().address().to_string(), port, svid);
        }
    }
}


void PeerConnectManager::getPeersFromConfig(std::vector<std::pair<in_addr_t, unsigned short>> &peerAddrs)
{    
    for(std::string address : m_opts.peer)
    {
        std::string  peer_address;
        std::string  port;
        std::string  svid;
        in_addr   addr;

        m_opts.get_address(address, peer_address, port, svid);

        if(inet_aton(peer_address.c_str(), &addr) != 0 ){
            peerAddrs.push_back(std::make_pair(addr.s_addr, atol(port.c_str())));
        } else {
            boost::asio::ip::tcp::resolver              resolver(m_ioService);
            boost::asio::ip::tcp::resolver::query       query(peer_address.c_str(),SERVER_PORT);
            boost::asio::ip::tcp::resolver::iterator    iterator = resolver.resolve(query);
            boost::asio::ip::tcp::resolver::iterator    end;


            if(iterator != end && inet_aton(iterator->endpoint().address().to_string().c_str(), &addr) != 0)
            {
                peerAddrs.push_back(std::make_pair(addr.s_addr, atol(port.c_str())));
            }
        }
    }
}

void PeerConnectManager::getPeersFromDNS(std::vector<std::pair<in_addr_t, unsigned short>> &peerAddrs)
{
    try
    {
        boost::asio::ip::tcp::resolver              resolver(m_ioService);
        boost::asio::ip::tcp::resolver::query       query(m_opts.dnsa.c_str(),SERVER_PORT);
        boost::asio::ip::tcp::resolver::iterator    iterator = resolver.resolve(query);
        boost::asio::ip::tcp::resolver::iterator    end;

        while(iterator != end)
        {
            auto addr = iterator->endpoint().address().to_string();
            auto port = iterator->endpoint().port();

            in_addr   inaddr;
            if(inet_aton(addr.c_str(), &inaddr) != 0){
                peerAddrs.push_back(std::make_pair(inaddr.s_addr, port));
            }
            ++iterator;
        }
    }
    catch (std::exception& e) {
        ELOG("DNS Connect Exception: %s\n", e.what());
    }
}

void PeerConnectManager::getPeersFromServerFile(std::vector<std::pair<in_addr_t, unsigned short>> &peerAddrs)
{
    node        nodeInfo;
    uint16_t    maxNodeIndx   = m_server.getMaxNodeIndx();

    if(maxNodeIndx == 0){
        assert(maxNodeIndx != 0);
        ELOG("maxNodeIndx == 0, returning\n");
        return;
    }

    //@TODO: improve efficiency of finding new node
    for(int i = 0; i<=maxNodeIndx; ++i)
    {
        //Don't connect to myself
        if(i != m_opts.svid && m_server.getNode(i, nodeInfo))
        {
            if(nodeInfo.mtim >= m_server.last_srvs_.now - BLOCKSEC*BLOCKDIV) {
                peerAddrs.push_back(std::make_pair(nodeInfo.ipv4, static_cast<unsigned short>(nodeInfo.port)));
            }
        }
    }
}


void PeerConnectManager::timerNextTick(int timeout)
{    
    m_connectTimer.expires_from_now(boost::posix_time::seconds(timeout));
    m_connectTimer.async_wait(boost::bind(&PeerConnectManager::connectPeers, this, boost::asio::placeholders::error));
}

void PeerConnectManager::connectPeers(const boost::system::error_code& error)
{    
    if(error ==  boost::asio::error::operation_aborted){
        DLOG("ERROR connectPeers\n");
        return;
    }

    int neededPeers = MIN_PEERS - m_peers.size();
    neededPeers = neededPeers > MAX_INIT_CONN ? MAX_INIT_CONN : neededPeers;

    DLOG("Need %d peers\n", neededPeers);

    //@TODO: ADD check if we cannot access any server.
    //In this case SHUTDOWN_AND_RETURN() should be called
    if(!error && neededPeers > 0)
    {
        std::vector<std::pair<in_addr_t, unsigned short>> peerAddrs;

        getPeersFromConfig(peerAddrs);
        getPeersFromDNS(peerAddrs);
        getPeersFromServerFile(peerAddrs);

        DLOG("Available peers: %d\n", peerAddrs.size());

        if(peerAddrs.size() > 0) {
            for(unsigned int i=0; i<peerAddrs.size() && neededPeers > 0; i++) {
                int inx = random()%peerAddrs.size();
                auto peer = peerAddrs[inx];

                if(peer.second > 0 && m_peers.find(peer) == m_peers.end() )
                {
                    DLOG("Selected peer %d. addr %d, port %d\n", inx, peer.first, peer.second);
                    in_addr addr;
                    addr.s_addr = peer.first;
                    connect(addr, peer.second, BANK_MAX);
                    peerAddrs[inx].second=0;
                    neededPeers--;
                } else {
                    if(peer.second > 0) {
                        DLOG("Selected peer already connected\n");
                    }
                }
            }
        } else {
            ELOG("No peers available. Update your config file.\n");
        }
    }    

    timerNextTick(m_timeout);
}


bool PeerConnectManager::alreadyConnected(in_addr_t address, unsigned short port, uint16_t svid)
{    
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);    

    auto activePeer = m_activePeers.find(svid);

    if( activePeer != m_activePeers.end()){
        DLOG("%04X PEER alreadyConnected\n", svid);
        return true;
    }

    auto peer = m_peers.find(std::make_pair(address, port));

    if( peer != m_peers.end()){
        DLOG("%04X PEER alreadyConnected address %d\n", peer->second->svid, port);
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

    auto svidPeer = m_activePeers.find(svid);
    if(svidPeer != m_activePeers.end()){
        svidPeer->second->deliver(msg);
        return;
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

    for(auto& peer: m_activePeers){
        peer.second->deliver(msg);
    }
}

void PeerConnectManager::update(message_ptr msg, uint16_t svid)
{
    m_ioService.dispatch(boost::bind(&PeerConnectManager::updateImpl, this, msg, svid));
}

void PeerConnectManager::updateImpl(message_ptr msg, uint16_t svid)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    auto svidPeer = m_activePeers.find(svid);
    if(svidPeer != m_activePeers.end()){
        svidPeer->second->update(msg);
    }
}

void PeerConnectManager::updateAll(message_ptr msg)
{
    m_ioService.dispatch(boost::bind(&PeerConnectManager::updateAllImpl, this, msg));
}

void PeerConnectManager::updateAllImpl(message_ptr msg)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    for(auto& peer: m_activePeers)
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
        if((peer.second)->svid != BANK_MAX) {
            ss << std::to_string((peer.second)->svid);
        } else {
            ss << "-";
        }
        if((peer.second)->getState() == peer::ST_STOPED) {
            ss << "*";
        }
    }
    return ss.str();
}

int PeerConnectManager::getPeersCount(bool activeOnly)
{    
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    if(activeOnly){         
        return m_activePeers.size();
    }    
    return m_peers.size();
}

bool PeerConnectManager::checkDuplicate(uint16_t svid)
{
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    return m_activePeers.find(svid) != m_activePeers.end();
}

void PeerConnectManager::getMoreHeaders(uint32_t now)
{    
    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);
    if(m_activePeers.size() == 0)
    {
        DLOG("REQUEST more headers NO active PEERS!! \n");
        return;
    }

    auto    peer  = m_activePeers.begin();
    int     num   = random() % (m_activePeers.size());

    std::advance(peer,num);

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
}

void PeerConnectManager::fillknown(message_ptr msg)
{
    static uint32_t r=0;

    boost::shared_lock< boost::shared_mutex > lock(m_peerMx);

    if(m_activePeers.size() == 0){        
        return;
    }

    auto pi = m_activePeers.begin();
    std::advance(pi,(r % m_activePeers.size()));

    for(auto it = pi; it != m_activePeers.end(); it++)
    {
        msg->know_insert(pi->second->svid);        
    }
    for(auto it = m_activePeers.begin(); it != pi; it++)
    {
        msg->know_insert(pi->second->svid);        
    }
    ++r;
}

void PeerConnectManager::printLastMsgInfoForPeers() {
    uint32_t currentTime = time(NULL);
    ILOG("LAST RECEIVED MSG FOR ALL PEERS FOR NODE SVID=%04X, CURRENT TIME=%u\n", m_opts.svid, currentTime);

    for(const auto& peer : m_activePeers) {
        uint32_t timeDiff = currentTime-peer.second->last_received_msg_time;
        ILOG("\tLast received msg from peer svid:%04X was %u seconds ago (timestamp:%u), msg type:%u\n",
             peer.second->svid,
             timeDiff,
             peer.second->last_received_msg_time,
             peer.second->last_received_msg_type);
    }
}
