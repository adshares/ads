#ifndef PEER_HPP
#define PEER_HPP

#include <boost/asio.hpp>
#include <boost/smart_ptr.hpp>
#include <pthread.h>
#include "server.hpp"
#include "options.hpp"
#include "helper/socket.h"
#include "network/peerclient.h"
#include "network/peerclientmanager.h"

#define NETPEER_SOCK_TIMEOUT 5
#define NETPEER_SOCK_IDLE    120
#define NETPEER_SOCK_MAXTRY  5


class peer : public boost::enable_shared_from_this<peer> {

public:
    enum PeerState
    {
        ST_INIT = 0,
        ST_CONNECTING,
        ST_CONNECTED,
        ST_AUTHENTCATING,
        ST_SYNCD,
        ST_VOTING,
        ST_STOPED
    };

  public:
    peer(server& srv,bool in,servers& srvs,options& opts, PeerConnectManager& peerManager, std::string addr, uint32_t port):
        svid(BANK_MAX),
        do_sync(1), //remove, use peer_hs.do_sync
        //killme(false),
        busy(0),
        addr(addr),
        port(port),
        peer_io_service_(),
        work_(peer_io_service_),
        socket_(peer_io_service_),
        m_netclient(socket_, *this),
        m_peerManager(peerManager),
        m_state{ST_INIT},
        server_(srv),        
        incoming_(in),
        srvs_(srvs),
        opts_(opts),        
        files_out(0),
        files_in(0),
        bytes_out(0),
        bytes_in(0),        
        BLOCK_MODE(0),
        BLOCK_SERV(false),
        BLOCK_PEER(false)
    {
        read_msg_ = boost::make_shared<message>();

        iothp_.reset(new boost::thread(boost::bind(&peer::iorun,this)));

        setIoThreadName();
    }

    ~peer() {

        DLOG("%04X PEER DESTRUCT address %s port %d\n", svid, addr.c_str(), port);

        try {
            if(!peer_io_service_.stopped()){
                peer_io_service_.stop();
            }

            if(iothp_ && iothp_->joinable()) {                
                iothp_->join();                
            } //try joining yourself error;
        }
        catch (std::exception& e) {
            std::cerr << e.what();
        }
    }

    void setIoThreadName()
    {                      
#ifdef LINUX
        if(iothp_){
            std::string thName = "pr_" + std::to_string(svid);
            pthread_setname_np(iothp_->native_handle(), thName.c_str());

        }
#endif
    }

    void iorun() {
        DLOG("%04X PEER IORUN START\n",svid);
        try {
            peer_io_service_.run();
        } //Now we know the server is down.
        catch (std::exception& e) {
//FIXME, stop peer after Broken pipe (now does not stop if peer ends with 'assert')
//FIXME, wipe out inactive peers (better solution)
            ELOG("%04X CATCH IORUN Service.Run error:%s\n",svid,e.what());
            leave();
        }        
    }    

    void tryAsyncConnect(boost::asio::ip::tcp::resolver::iterator& connIt, int timeout) {        
        m_netclient.asyncConnect(connIt, boost::bind(&peer::connect, this, boost::asio::placeholders::error), timeout);
    }

    PeerState getState()
    {
        return m_state;
    }

    void setState(PeerState state)
    {
        if(m_state == ST_AUTHENTCATING && state == PeerState::ST_SYNCD)
        {
            DLOG("%04X ADD avtive peer\n",svid);
            m_peerManager.addActivePeer(svid, shared_from_this());
        }

        if(m_state != ST_STOPED && state == PeerState::ST_STOPED)
        {
            DLOG("%04X LEAVE active peer\n",svid);
            m_peerManager.leevePeer(svid, addr, port);
        }

        DLOG("%04X PEER CHANGE STATE FROM %d TO %d\n",svid, m_state, state);
        m_state = state;
    }

    void leave() {        
        setState(ST_STOPED);
    }    

    void accept()
    {
        //only incoming connections
        assert(socket_.is_open());
        Helper::setSocketTimeout(socket_, NETPEER_SOCK_TIMEOUT, NETPEER_SOCK_IDLE, NETPEER_SOCK_MAXTRY);
        assert(incoming_);       
        addr = socket_.remote_endpoint().address().to_string();
        port = socket_.remote_endpoint().port();

        DLOG("%04X PEER CONNECT OK %s:%d\n",svid, addr.c_str() ,port);
        setState(ST_CONNECTED);
        asyncWaitForNewMessageHeader();
    }

    void connect(const boost::system::error_code& error)
    {
        //only outgoing connection
        if(error) {            
            DLOG("%04X PEER ACCEPT ERROR address %s: port%d\n",svid, addr.c_str(), port);
            leave();
            return;
        }

        DLOG("%04X PEER ACCEPT\n",svid);

        //killme=false; //connection established
        setState(ST_CONNECTED);
        assert(!incoming_);
        assert(addr == socket_.remote_endpoint().address().to_string());
        assert(port == socket_.remote_endpoint().port());

        //manager add
        DLOG("%04X PEER ACCEPT OK %d<->%s:%d\n",svid,socket_.local_endpoint().port(),
             socket_.remote_endpoint().address().to_string().c_str(),port);
        message_ptr msg=server_.write_handshake(0,sync_hs); // sets sync_hs
        msg->know_insert_(svid);
        msg->sent.insert(svid);
        msg->busy.insert(svid);
        msg->print(" HANDSHAKE");
        write_msgs_.push_back(msg);

        m_netclient.asyncWrite(msg->data, msg->len, boost::bind(&peer::handle_write,this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        asyncWaitForNewMessageHeader();        
    }

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    void print() {
        if(port) {
            DLOG("%04X Client on     %s:%d\n",svid,addr.c_str(),port);
        } //LESZEK
        else {
            DLOG("%04X Client available\n",svid);
        }
    }

    void update(message_ptr msg) {
        if(getState() == ST_STOPED) {
            DLOG("%04X update KILL detected ! (UPDATE), leaving\n",svid);
            return;
        }
        assert(msg->len>4+64);
        msg->print("; TRY UPDATE");
        if(do_sync) {
            msg->print("; NO SYNC");
            return;
        }
        if(msg->peer == svid) {
            msg->print("; NO ECHO");
            return;
        }
        if(!msg->know_emplace(svid)) {
            msg->print("; NO UPDATE");
            return;
        }
        message_ptr put_msg(new message()); // gone ??? !!!
        switch(msg->hashtype()) {
        case MSGTYPE_MSG:
            DLOG("%04X MESSAGE detected ! (UPDATE)\n", svid);
            put_msg->data[0]=MSGTYPE_PUT;
            put_msg->data[1]=msg->hashval(svid); //msg->data[4+(svid%64)]; // convert to peer-specific hash
            memcpy(put_msg->data+2,&msg->msid,4);
            memcpy(put_msg->data+6,&msg->svid,2);
            break;
        case MSGTYPE_CND:
            put_msg->data[0]=MSGTYPE_CNP;
            put_msg->data[1]=msg->hashval(svid); //msg->data[4+(svid%64)]; // convert to peer-specific hash
            memcpy(put_msg->data+2,&msg->msid,4);
            memcpy(put_msg->data+6,&msg->svid,2);
            break;
        case MSGTYPE_BLK:
            put_msg->data[0]=MSGTYPE_BLP;
            put_msg->data[1]=msg->hashval(svid); //msg->data[4+(svid%64)]; // convert to peer-specific hash
            memcpy(put_msg->data+2,&msg->msid,4);
            memcpy(put_msg->data+6,&msg->svid,2);
            break;
        case MSGTYPE_DBL:
            put_msg->data[0]=MSGTYPE_DBP;
            put_msg->data[1]=0;
            memcpy(put_msg->data+2,&msg->msid,4);
            memcpy(put_msg->data+6,&msg->svid,2);
            break;
        default:
            FLOG("%04X bad message type\n",svid);
            exit(-1);
        }
        put_msg->svid=msg->svid;
        put_msg->msid=msg->msid;
        put_msg->hash.num=put_msg->dohash(put_msg->data);
        put_msg->sent.insert(svid);
        put_msg->busy.insert(svid);
        DLOG("%04X HASH %016lX [%016lX] (update) %04X:%08X\n",svid,put_msg->hash.num,*((uint64_t*)put_msg->data),msg->svid,msg->msid);
        boost::lock_guard<boost::mutex> lock(pio_);
        if(BLOCK_MODE) {
            write_msgs_.push_back(put_msg);            
            DLOG("%04X HASH %016lX [%016lX] (update in block mode, waiting) %04X:%08X\n",svid,put_msg->hash.num,*((uint64_t*)put_msg->data),msg->svid,msg->msid); // could be bad allignment
            return;
        }
        bool no_write_in_progress = write_msgs_.empty();
        write_msgs_.push_back(put_msg);
        if (no_write_in_progress) {
            //int len=message_len(write_msgs_.front());
            int len=write_msgs_.front()->len;

            m_netclient.asyncWrite(write_msgs_.front()->data,len, boost::bind(&peer::handle_write,this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }        
    }

    /*int message_len(message_ptr msg) // shorten candidate vote messages if possible
    { if(do_sync){
        return(msg->len);}
      assert(msg->data!=NULL);
      //if(msg->data[0]==MSGTYPE_CNG && msg->len>4+64+10+sizeof(hash_t)){//shorten message if peer knows this hash
      if(msg->data[0]==MSGTYPE_CND && msg->len>4+64+10+sizeof(hash_t)){//shorten message if peer knows this hash
        //hash_s cand;
        //memcpy(cand.hash,msg->data+4+64+10,sizeof(hash_t));
        //candidate_ptr c_ptr=server_.known_candidate(cand,0);
        hash_s* cand=(hash_s*)(msg->data+4+64+10);
        candidate_ptr c_ptr=server_.known_candidate(*cand,0);
        if(c_ptr!=NULL && c_ptr->peers.find(svid)!=c_ptr->peers.end()){ // send only the hash, not the whole message
    //FIXME, is this ok ?
    DLOG("%04X WARNING, truncating cnd message\n",svid);
          return(4+64+10+sizeof(hash_t));}}
      //if(msg->data[0]==MSGTYPE_BLK && msg->len>4+64+10){
      if(msg->data[0]==MSGTYPE_BLG && msg->len>4+64+10){ // probably only during sync
        header_t* h=(header_t*)(msg->data+4+64+10);
        if(peer_hs.head.now==h->now && !memcmp(peer_hs.head.nowhash,h->nowhash,SHA256_DIGEST_LENGTH)){
    DLOG("%04X WARNING, truncating blk message\n",svid);
          return(4+64+10);}}
      return(msg->len);
    }*/

    void deliver(message_ptr msg)
    {
        DLOG("%04X (DELIVER), START TYPE\n",svid);
        extern candidate_ptr nullcnd;
        if(getState() == ST_STOPED) {
            DLOG("%04X KILL detected ! (DELIVER), leaving\n",svid);
            return;
        }
        if(do_sync) {
            return;
        }
        busy=time(NULL);//set peer to busy, TODO, set this flag in other methods too
        if(!msg->sent_emplace(svid)) {
            DLOG("%04X REJECTING download request for %0X4:%08X (late)\n",svid,msg->svid,msg->msid);
            return;
        }
        if(msg->len!=message::header_length && !msg->load(svid)) { // sets busy[svid] READONLY ok
            ELOG("%04X failed to load message %0X4:%08X\n",svid,msg->svid,msg->msid);
            return;
        }
        assert(msg->data!=NULL);
        //special handling of CND messages ... FIXME prevent slow search in candidates by each peer
        if(msg->data[0]==MSGTYPE_CND && msg->len>4+64+10+sizeof(hash_t)) {
            // this is very innefficient !!! server_.condidates_ should be a map of messages that have peers[] ...
            hash_s* cand=(hash_s*)(msg->data+4+64+10);
            candidate_ptr c_ptr=server_.known_candidate(*cand,0);
            if(c_ptr!=nullcnd && c_ptr->peers.find(svid)!=c_ptr->peers.end()) {
                // send only the hash, not the whole message
                WLOG("%04X WARNING, truncating cnd message\n",svid);
                message_ptr put_msg(new message(4+64+10+sizeof(hash_t),msg->data));
                memcpy(put_msg->data+1,&put_msg->len,3); //FIXME, make this a change_length function in message.hpp
                assert(msg->svid);
                put_msg->svid=msg->svid; //just for reporting
                put_msg->msid=msg->msid; //just for reporting
                put_msg->sent.insert(svid);
                put_msg->busy.insert(svid);
                msg=put_msg;
            } else {
                //FIXME, create new message and remove msids that are identical to our last_msid list
            }
        }
        boost::lock_guard<boost::mutex> lock(pio_);

        if(BLOCK_MODE) {
            write_msgs_.push_back(msg);            
            return;
        }
        if(msg->data[0]==MSGTYPE_STP) {
            DLOG("%04X SERV in block mode\n",svid);
            BLOCK_MODE=srvs_.now;
        }
        bool no_write_in_progress = write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (no_write_in_progress) {
            //int len=message_len(write_msgs_.front());
            int len=write_msgs_.front()->len;

            DLOG("%04X (DELIVER), START ACTION %d\n",svid, msg->data[0]);
            m_netclient.asyncWrite(write_msgs_.front()->data,len, boost::bind(&peer::handle_write,this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }        
    }

    void send_sync(message_ptr put_msg) {
        /*
        WAITING for headers 58B95160, maybe need more peers
        REQUEST more headers from peer 0001
        SENDING block header request for 58B95160
        main: peer.hpp:203: void peer::send_sync(message_ptr): Assertion `do_sync' failed.
        Aborted
        */
        //assert(do_sync); //FIXME, failed !!!
//FIXME, brakes after handle_read_headers() if there are more headers to load
        if(getState() == ST_STOPED)  {
            DLOG("%04X KILL detected ! (SEND SYNC), leaving\n",svid);
            return;
        }
        put_msg->load(svid);
        busy=time(NULL);//set peer to busy, TODO, set this flag in other methods too
        boost::unique_lock<boost::mutex> lock(pio_); //pio_.lock(); //most likely no lock needed
        std::size_t len = 0;
        try {                          
             len = m_netclient.writeSync(put_msg->data,put_msg->len, DEFAULT_NET_TIMEOUT);
        } catch (std::exception& e) {
            ELOG("%04X CATCH asio error (send_sync): %s\n",svid,e.what());
            throw std::runtime_error("ERROR in write_serv_del()\n");
        }
        if(len != put_msg->len ){
            ELOG("%04X ERROR in send_sync\n", svid);
            throw std::runtime_error("ERROR in write_serv_del()\n");
        }

        lock.unlock();
        if(put_msg->len!=message::header_length) {
//FIXME, do not unload everything ...
            put_msg->unload(svid);
        }        
    }

    void handle_write(const boost::system::error_code& error, size_t transfered)
    { //TODO change this later, dont send each message separately if possible
        std::ignore = transfered;

        if (!error) {
            boost::unique_lock<boost::mutex> lock(pio_);
            message* msg=&(*(write_msgs_.front()));
            assert(msg->data!=NULL);
            assert(!msg->not_busy(svid));
            bytes_out+=msg->len;
            files_out++;
            uint32_t dlen=msg->data_len();
            if(dlen!=msg->len) {
                DLOG("%04X DELIVERED MESSAGE %04X:%08X %02X (len:%d<>%d) [total %lub %uf] LENGTH DIFFERS!\n",svid,
                     msg->svid,msg->msid,msg->data[0],msg->len,dlen,bytes_out,files_out);
            } else {
                DLOG("%04X DELIVERED MESSAGE %04X:%08X %02X (len:%d) [total %lub %uf]\n",svid,
                     msg->svid,msg->msid,msg->data[0],msg->len,bytes_out,files_out);
            }
            if(msg->data[0]==MSGTYPE_STP) {
                DLOG("%04X SERV sent STOP\n",svid);
                write_msgs_.pop_front();
                lock.unlock();
                sync_start(true);
                return;
            }
            if(!server_.do_sync && !do_sync && (msg->data[0]==MSGTYPE_PUT || msg->data[0]==MSGTYPE_DBP)) {
//FIXME, do not add messages that are in last block
                if(svid_msid_new[msg->svid]<msg->msid) {
                    svid_msid_new[msg->svid]=msg->msid; // maybe a lock on svid_msid_new would help
                    DLOG("%04X UPDATE PEER SVID_MSID: %04X:%08X\n",svid,msg->svid,msg->msid);
                }
            }
            if(msg->len!=message::header_length) {
                msg->unload(svid);
            } else {
                msg->busy_erase(svid);
            }
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
                //FIXME, now load the message from db if needed !!! do not do this when inserting in write_msgs_, unless You do not worry about RAM but worry about speed
                //int len=message_len(write_msgs_.front());
                int len=write_msgs_.front()->len;

                m_netclient.asyncWrite(write_msgs_.front()->data,len, boost::bind(&peer::handle_write, this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
            }            
        } else {
            ELOG("%04X WRITE error %d %s\n",svid,error.value(),error.message().c_str());
            leave();
            return;
        }
    }

    void handle_read_header(const boost::system::error_code& error, size_t transfered)
    {
        last_received_msg_time = time(NULL);
        last_received_msg_type = read_msg_->data[0];

        std::ignore = transfered;

        extern message_ptr nullmsg;
        if(getState() == ST_STOPED)  {
            ELOG("%04X KILL detected ! (HANDLE READ HEADER), leaving\n",svid);            
            return;
        }
        if(error) {
            ELOG("%04X READ error %d %s (HEADER) transfered %lu\n",svid,error.value(),error.message().c_str(), transfered);
            leave();
            return;
        }
        if(!read_msg_->header(svid)) {
            ELOG("%04X READ header error\n",svid);
            leave();
            return;
        }
        if(svid==BANK_MAX && read_msg_->data[0]!=MSGTYPE_INI) { // READONLY ok
            ELOG("%04X READ illegal header during startup (%d)\n",svid,(int)read_msg_->data[0]);
            leave();
            return;
        }        

        busy=0; // any incomming data resets the busy flag indicating a responsive peer
        bytes_in+=read_msg_->len;
        files_in++;
        last_active=time(NULL); // protect from disconnect
        read_msg_->know_insert_(svid);

        if(read_msg_->data[0]==MSGTYPE_USR)
        { //len can be message::header_length in this case :-(
            //FIXME, accept only if needed !!
            DLOG("%04X READ bank %04X [len %08X]\n",svid,read_msg_->svid,read_msg_->len);

            m_netclient.asyncRead(read_msg_->data+message::header_length, read_msg_->len*sizeof(user_t),
                                  boost::bind(&peer::handle_read_bank,this,boost::asio::placeholders::error), REQ_USR_TIMEOUT);

            return;
        }
        if(read_msg_->len==message::header_length) { //short ping message
            if(srvs_.nodes.size()<=read_msg_->svid || server_.last_srvs_.nodes.size()<=read_msg_->svid) { // READONLY ok
                ELOG("%04X message from unknown server %04X:%08X\n",svid,read_msg_->svid,read_msg_->msid);
                leave();
                return;
            }
            if(read_msg_->data[0]==MSGTYPE_PUT || read_msg_->data[0]==MSGTYPE_CNP || read_msg_->data[0]==MSGTYPE_BLP || read_msg_->data[0]==MSGTYPE_DBP) {
                if(!read_msg_->svid) { // READONLY ok
                    ELOG("%04X illegal message from readonly server %04X:%08X\n",svid,read_msg_->svid,read_msg_->msid);
                    leave();
                    return;
                }
                DLOG("%04X HASH %016lX [%016lX]\n",svid,read_msg_->hash.num,*((uint64_t*)read_msg_->data)); // could be bad allignment
                if(read_msg_->data[0]==MSGTYPE_PUT) {
                    read_msg_->data[0]=MSGTYPE_GET;
                }
                if(read_msg_->data[0]==MSGTYPE_CNP) {
                    read_msg_->data[0]=MSGTYPE_CNG;
                }
                if(read_msg_->data[0]==MSGTYPE_BLP) {
                    read_msg_->data[0]=MSGTYPE_BLG;
                }
                if(read_msg_->data[0]==MSGTYPE_DBP) {
                    read_msg_->data[0]=MSGTYPE_DBG;
                }
                if(read_msg_->data[0]==MSGTYPE_GET || read_msg_->data[0]==MSGTYPE_DBG) {                    
                    boost::lock_guard<boost::mutex> lock(pio_);
                    if(svid_msid_new[read_msg_->svid]<read_msg_->msid) {
                        svid_msid_new[read_msg_->svid]=read_msg_->msid;
                        DLOG("%04X UPDATE PEER SVID_MSID: %04X:%08X\n",svid,read_msg_->svid,read_msg_->msid);
                    }                    
                }
                int ret=server_.message_insert(read_msg_);
                if(ret) { //NEW, make sure to insert in correct containers
                    if((ret<0) ||
                            (read_msg_->data[0]==MSGTYPE_CNG && server_.last_srvs_.now+BLOCKSEC==read_msg_->msid ) ||
                            (read_msg_->data[0]==MSGTYPE_BLG && server_.last_srvs_.now>=read_msg_->msid && (server_.last_srvs_.nodes[read_msg_->svid].status & SERVER_VIP)) ||
                            read_msg_->data[0]==MSGTYPE_DBG ||
                            (read_msg_->data[0]==MSGTYPE_GET && srvs_.nodes[read_msg_->svid].msid==read_msg_->msid-1 && server_.check_msgs_size()<MAX_CHECKQUE)) {
                        DLOG("%04X REQUESTING MESSAGE (%04X:%08X)\n",svid,read_msg_->svid,read_msg_->msid);
#ifdef DEBUG
                        if(!do_sync && !sync_hs.do_sync && !peer_hs.do_sync) {
                            boost::this_thread::sleep(boost::posix_time::milliseconds(rand()%1000));
                        }
#endif
                        deliver(read_msg_);
                    }
                }
            } // request message if not known (inserted)
            else if(read_msg_->data[0]==MSGTYPE_GET || read_msg_->data[0]==MSGTYPE_CNG || read_msg_->data[0]==MSGTYPE_BLG || read_msg_->data[0]==MSGTYPE_DBG) {
                if(!read_msg_->svid) { // READONLY ok
                    ELOG("%04X illegal message from readonly server %04X:%08X\n",svid,read_msg_->svid,read_msg_->msid);
                    leave();
                    return;
                }
                DLOG("%04X HASH %016lX [%016lX]\n",svid,read_msg_->hash.num,*((uint64_t*)read_msg_->data)); // could be bad allignment
                if(do_sync) {
                    read_msg_->path=peer_path;
                    if(read_msg_->load(svid)) { //READONLY ok
                        DLOG("%04X PROVIDING MESSAGE %04X:%08X %02X (len:%d) (in sync)\n",svid,read_msg_->svid,read_msg_->msid,read_msg_->hash.dat[1],read_msg_->len);
                        send_sync(read_msg_);
                    } //deliver will not work in do_sync mode
                    else {
                        DLOG("%04X FAILED answering request for %04X:%08X (not found in %08X)\n",svid,read_msg_->svid,read_msg_->msid,peer_path);
                    }
                } else {
                    message_ptr msg=server_.message_find(read_msg_,svid);
                    if(msg!=nullmsg) {
                        if(msg->len>message::header_length) {
                            if(msg->sent.find(svid)!=msg->sent.end()) {
                                //TODO, after reconnecting peer missed messages :-(
                                DLOG("%04X REJECTING download request for %04X:%08X\n",svid,msg->svid,msg->msid);
                            } else {
                                DLOG("%04X PROVIDING MESSAGE %04X:%08X %02X (len:%d)\n",svid,msg->svid,msg->msid,msg->hash.dat[1],msg->len);
#ifdef DEBUG
                                if(!do_sync && !sync_hs.do_sync && !peer_hs.do_sync) {
                                    boost::this_thread::sleep(boost::posix_time::milliseconds(rand()%1000));
                                }
#endif
                                deliver(msg);
                            }
                        } // must force deliver without checks
                        else { // no real message available
                            WLOG("%04X BAD get request\n",svid);
                        }
                    } else { // concider loading from db if available
                        DLOG("%04X MESSAGE %04X:%08X not found, concider loading from db\n\n\n",svid,
                             read_msg_->svid,read_msg_->msid);
                    }
                }
            } else if(read_msg_->data[0]==MSGTYPE_HEA) { // READONLY ok , accept svid==0 below
                write_headers();
            } else if(read_msg_->data[0]==MSGTYPE_NHR) {
                write_headers();
            } else if(read_msg_->data[0]==MSGTYPE_SER) { //servers request
                write_servers();
            } else if(read_msg_->data[0]==MSGTYPE_MSL) { //msg list request
                write_msglist();
            } else if(read_msg_->data[0]==MSGTYPE_USG) {
                write_bank();
            } else if(read_msg_->data[0]==MSGTYPE_PAT) { //set current sync block
                memcpy(&peer_path,read_msg_->data+1,4);
                DLOG("%04X DEBUG, got sync path %08X\n",svid,peer_path);
            } else if(read_msg_->data[0]==MSGTYPE_SOK) {
                uint32_t now;
                memcpy(&now,read_msg_->data+1,4);
                ELOG("%04X Authenticated, peer in sync at %08X\n",svid,now);
                update_sync();
                do_sync=0;
                setState(ST_SYNCD);
            } else {
                int n=read_msg_->data[0];
                ELOG("%04X ERROR message type %02X (%d) received (%016lX)\n",svid,n,n,*(uint64_t*)read_msg_->data);
                leave();
                return;
            }
            asyncWaitForNewMessageHeader();
        } else {
            if(read_msg_->data[0]==MSGTYPE_STP) {
                DLOG("%04X PEER in block mode\n",svid);
                //could enter a different (sync) read sequence here;
                //read_msg_->data=(uint8_t*)std::realloc(read_msg_->data,read_msg_->len);
                assert(read_msg_->len==1+SHA256_DIGEST_LENGTH);                
                m_netclient.asyncRead(read_msg_->data+message::header_length,read_msg_->len-message::header_length,
                                      boost::bind(&peer::handle_read_stop,this,boost::asio::placeholders::error));
                return;
            }
            if(read_msg_->data[0]==MSGTYPE_MSP) {
                DLOG("%04X READ msglist header\n",svid);                                
                m_netclient.asyncRead(read_msg_->data+message::header_length,read_msg_->len-message::header_length,
                                      boost::bind(&peer::handle_read_msglist,this,boost::asio::placeholders::error));
                return;
            }
            if(read_msg_->data[0]==MSGTYPE_BLK) {
                DLOG("%04X READ block header\n",svid);
                assert(read_msg_->len==4+64+10+sizeof(header_t) || read_msg_->len==4+64+10);

                m_netclient.asyncRead(read_msg_->data+message::header_length,read_msg_->len-message::header_length,
                                      boost::bind(&peer::handle_read_block,this,boost::asio::placeholders::error));
                return;
            }
            if(read_msg_->data[0]==MSGTYPE_NHD) {
                DLOG("%04X READ next header\n",svid);
                assert(read_msg_->len==8+SHA256_DIGEST_LENGTH+sizeof(headlink_t));

                m_netclient.asyncRead(read_msg_->data+message::header_length,read_msg_->len-message::header_length,
                                      boost::bind(&peer::handle_next_header,this,boost::asio::placeholders::error));
                return;
            }

            m_netclient.asyncRead(read_msg_->data+message::header_length,read_msg_->len-message::header_length,
                                  boost::bind(&peer::handle_read_body,this,boost::asio::placeholders::error));
        }
    }

    void update_sync(void) { // send current inventory (all msg and dbl messages)
        std::vector<uint64_t>txs;
        std::vector<uint64_t>dbl;
        std::vector<uint64_t>blk;
        server_.update_list(txs,dbl,blk,svid);
        std::string data;
        boost::unique_lock<boost::mutex> lock(pio_);
        for(auto it=txs.begin(); it!=txs.end(); it++) {
            svid_msid_new[*(uint16_t*)(((char*)&(*it))+6)]=*(uint32_t*)(((char*)&(*it))+2); //assume txs sorted
            data.append((const char*)&(*it),sizeof(uint64_t));
        }
        lock.unlock();
        for(auto it=dbl.begin(); it!=dbl.end(); it++) {
            data.append((const char*)&(*it),sizeof(uint64_t));
        }
        for(auto it=blk.begin(); it!=blk.end(); it++) {
            data.append((const char*)&(*it),sizeof(uint64_t));
        }
        message_ptr put_msg(new message(data.size()));
        memcpy(put_msg->data,data.c_str(),data.size());
        DLOG("%04X SENDING update with %d bytes\n",svid,(int)data.size());
        send_sync(put_msg);
    }

    void write_headers() {
        uint32_t from;
        memcpy(&from,read_msg_->data+1,4);
        uint32_t to=sync_hs.head.now;
        if(from>to) { // expect _NHR
            DLOG("%04X SENDING block header %08X\n",svid,from);
            message_ptr put_msg(new message(8+SHA256_DIGEST_LENGTH+sizeof(headlink_t)));
            headlink_t link;
            servers linkservers;
            linkservers.now=from;
            if(!linkservers.header_get()) {
                //FIXME, new block possibly not ready yet, repeat (sleep) until block ready
                ELOG("%04X ERROR, failed to provide header links\n",svid);
                uint32_t now=time(NULL);
                if(from < now-2*BLOCKSEC || from > now) {
                    leave();
                } // consider updating client
                return;
            }
            put_msg->data[0]=MSGTYPE_NHD;
            memcpy(put_msg->data+1,&from,4);
            memcpy((char*)put_msg->data+8,(const char*)linkservers.oldhash,SHA256_DIGEST_LENGTH);
            linkservers.filllink(link);
            memcpy((char*)put_msg->data+8+SHA256_DIGEST_LENGTH,(const char*)&link,sizeof(headlink_t));
            send_sync(put_msg);
            return;
        }
        uint32_t num=((to-from)/BLOCKSEC)-1;
        if(num<=0) {
            ELOG("%04X failed to provide correct request (from:%08X to:%08X)\n",svid,from,to);
            leave(); // consider updateing client
            return;
        }
        DLOG("%04X SENDING block headers starting after %08X and ending before %08X (num:%d)\n",svid,from,to,num);
        message_ptr put_msg(new message(SHA256_DIGEST_LENGTH+sizeof(headlink_t)*num));
        char* data=(char*)put_msg->data;
        for(uint32_t now=from+BLOCKSEC; now<to; now+=BLOCKSEC) {
            headlink_t link;
            servers linkservers;
            linkservers.now=now;
            if(!linkservers.header_get()) {
                ELOG("%04X failed to provide header links\n",svid);
                uint32_t now=time(NULL);
                if(from < now-2*BLOCKSEC || from > now) {
                    leave();
                } // consider updating client
                return;
            }
            if(now==from+BLOCKSEC) {
                memcpy(data,(const char*)linkservers.oldhash,SHA256_DIGEST_LENGTH);
                data+=SHA256_DIGEST_LENGTH;
            }
            linkservers.filllink(link);
            memcpy(data,(const char*)&link,sizeof(headlink_t));
            data+=sizeof(headlink_t);
        }
        assert(data==(char*)(put_msg->data+SHA256_DIGEST_LENGTH+sizeof(headlink_t)*num));
        send_sync(put_msg);
    }

    void request_next_headers(uint32_t now) { //WARNING, this requests a not validated header, FIXME !!!
        if(getState() == ST_STOPED) {
            return;
        }
        if(peer_hs.head.now>=now) { // this peer should send this header anytime soon
            return;
        }
        DLOG("%04X SENDING block header request for %08X\n",svid,now);
        message_ptr put_msg(new message());
        put_msg->data[0]=MSGTYPE_NHR;
        memcpy(put_msg->data+1,&now,4);
        send_sync(put_msg);
    }

//FIXME, this will fail because there is another read queued :-(
//must send a new message type (not _HEA)
    /*
        int len=SHA256_DIGEST_LENGTH+sizeof(headlink_t);
        read_msg_ = boost::make_shared<message>(len);
        boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_->data,len),
          boost::bind(&peer::handle_next_headers,this,boost::asio::placeholders::error,now,nowhash));
        //char* data=(char*)malloc(SHA256_DIGEST_LENGTH+sizeof(headlink_t));
        //ERROR, this is blocking !!!
        //int len=boost::asio::read(socket_,boost::asio::buffer(data,SHA256_DIGEST_LENGTH+sizeof(headlink_t)));
      }
    */

    //void handle_next_headers(const boost::system::error_code& error,uint32_t next_now,uint8_t* next_nowhash)
    void handle_next_header(const boost::system::error_code& error) {
        if(error) {
            ELOG("%04X reading next headers\n",svid);
            leave();
            return;
        }
        uint32_t from;
        memcpy(&from,read_msg_->data+1,4);
        char* data=(char*)read_msg_->data+8;
        DLOG("%04X PROCESSING next header\n",svid);
        servers peer_ls;
        headlink_t* link=(headlink_t*)(data+SHA256_DIGEST_LENGTH);
        peer_ls.loadlink(*link,from,data);
        server_.add_next_header(from,peer_ls);
        //server_.peer_.lock();
        //if((!server_.headers.size() &&
        //     !memcmp(peer_ls.oldhash,server_.last_srvs_.nowhash,SHA256_DIGEST_LENGTH)) ||
        //   (server_.headers.back().now==from-BLOCKSEC &&
        //     !memcmp(peer_ls.oldhash,server_.headers.back().nowhash,SHA256_DIGEST_LENGTH))){
        //  server_.headers.insert(server_.headers.end(),peer_ls);}
        //server_.peer_.unlock();
        asyncWaitForNewMessageHeader();
        return;
    }

    void handle_read_headers() {
        uint32_t to=peer_hs.head.now;
        DLOG("%04X READ HEADERS\n",svid);
        servers sync_ls; //FIXME, use only the header data not "servers"
        server_.get_last_header(sync_ls,sync_hs);
        //sync_ls.loadhead(sync_hs.head);
        //server_.peer_.lock();
        //if(server_.headers.size()){
        //  DLOG("%04X USE last header\n",svid);
        //  sync_ls=server_.headers.back();} // FIXME, use pointers/references maybe
        //server_.peer_.unlock();
        uint32_t from=sync_ls.now;
        /*if(opts_.fast){
          if(to!=from){
            DLOG("%04X HEADERS loaded ? %08X!=%08X\n",svid,to,from);
            return;}}
        else if(to<=from){ //FIXME, this failes sometimes
          DLOG("%04X HEADERS loaded ? %08X<=%08X\n",svid,to,from);
          return;}*/
        if(to<=from) { //FIXME, this failes sometimes
            DLOG("%04X HEADERS loaded (%08X<=%08X)\n",svid,to,from);
            return;
        }
        uint32_t num=(to-from)/BLOCKSEC;
        assert(num>0);
        std::vector<servers> headers(num); //TODO, consider changing this to a list
        if(num>1) {
            //if(server_.slow_sync(false,headers)<0){
            //  return;}
            DLOG("%04X SENDING block headers request\n",svid);
            message_ptr put_msg(new message());
            put_msg->data[0]=MSGTYPE_HEA;
            memcpy(put_msg->data+1,&from,4);
            send_sync(put_msg);
            DLOG("%04X READING block headers starting after %08X and ending before %08X (num:%d)\n",svid,from,to,num-1);
            char* data=(char*)malloc(SHA256_DIGEST_LENGTH+sizeof(headlink_t)*(num-1));
            assert(data!=NULL);
            int len=0;
            try {
                len=m_netclient.readSync(data,SHA256_DIGEST_LENGTH+sizeof(headlink_t)*(num-1), DEFAULT_NET_TIMEOUT);
            } catch (std::exception& e) {
                ELOG("%04X CATCH asio error (handle_read_headers): %s\n",svid,e.what());
                free(data);
                throw std::runtime_error("CATCH asio error (handle_read_headers)");
            }
            if(len!=(int)(SHA256_DIGEST_LENGTH+sizeof(headlink_t)*(num-1))) {
                ELOG("%04X READ headers error\n",svid);
                free(data);
                throw std::runtime_error("CATCH asio error (handle_read_headers)");
            }
            char* d=data+SHA256_DIGEST_LENGTH;
            //reed hashes and compare
            for(uint32_t i=0,now=from+BLOCKSEC; now<to; now+=BLOCKSEC,i++) {
                headlink_t* link=(headlink_t*)d;
                d+=sizeof(headlink_t);
                if(!i) {
                    headers[i].loadlink(*link,now,data);
                } else {
                    headers[i].loadlink(*link,now,(char*)headers[i-1].nowhash);
                }
                headers[i].header_print();
            } //assert(i==num-1);
            free(data);
            if(memcmp(headers[num-2].nowhash,peer_hs.head.oldhash,SHA256_DIGEST_LENGTH)) {
                ELOG("%04X ERROR, hashing header chain :-(\n",svid);
                char hash[2*SHA256_DIGEST_LENGTH];
                ed25519_key2text(hash,headers[num-2].nowhash,SHA256_DIGEST_LENGTH);
                ELOG("%04X NOWHASH nowhash %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
                ed25519_key2text(hash,peer_hs.head.oldhash,SHA256_DIGEST_LENGTH);
                ELOG("%04X NOWHASH oldhash %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
                leave();
                return;
            }
        }
        headers[num-1].loadhead(peer_hs.head);
        if(memcmp(headers[0].oldhash,sync_ls.nowhash,SHA256_DIGEST_LENGTH)) {
            ELOG("%04X ERROR, initial oldhash mismatch :-(\n",svid);
            char hash[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(hash,headers[0].oldhash,SHA256_DIGEST_LENGTH);
            ELOG("%04X NOWHASH got  %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
            ed25519_key2text(hash,sync_ls.nowhash,SHA256_DIGEST_LENGTH);
            ELOG("%04X NOWHASH have %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
            ELOG("%04X Maybe start syncing from an older block (peer will disconnect)\n\n",svid);
            leave();
            return;
        }
        DLOG("%04X HASHES loaded\n",svid);
        //server_.slow_sync(true,headers);
        server_.add_headers(headers);
        // send current sync path
        //server_.peer_.lock();
        if(srvs_.now && server_.do_sync) {
            message_ptr put_msg(new message());
            put_msg->data[0]=MSGTYPE_PAT;
            memcpy(put_msg->data+1,&srvs_.now,4);
            send_sync(put_msg);
        }
        //server_.peer_.unlock();
        return;
    }

    void write_msglist()
    {
        servers header;
        assert(read_msg_->data!=NULL);
        memcpy(&header.now,read_msg_->data+1,4);
        if(!header.header_get()){
            WLOG("%04X FAILED to read header %08X for svid: %04X\n",svid,header.now,svid); //TODO, send error
            return;
        }
        uint8_t *sigdata=NULL;
        uint32_t nok=0;
        header.get_signatures(header.now,sigdata,nok);
        header.vok=(uint16_t)nok;
        int len=12+header.vok*sizeof(svsi_t)+SHA256_DIGEST_LENGTH+header.msg*(2+4+SHA256_DIGEST_LENGTH);
        message_ptr put_msg(new message(len));
        put_msg->data[0]=MSGTYPE_MSP;
        memcpy(put_msg->data+1,&len,3); //bigendian
        if(sigdata!=NULL){
            memcpy(put_msg->data+4,sigdata,8+header.vok*sizeof(svsi_t));
            free(sigdata);
        }
        else{
            memcpy(put_msg->data+4,&header.now,4);
            memcpy(put_msg->data+8,&nok,4);
        }
        if(header.msgl_get((char*)(put_msg->data+12+header.vok*sizeof(svsi_t)))!=(int)(SHA256_DIGEST_LENGTH+header.msg*(2+4+SHA256_DIGEST_LENGTH))){
            WLOG("%04X FAILED to read msglist %08X with %d signatures\n",svid,header.now,header.vok); //TODO, send error
            return;
        }
        DLOG("%04X SENDING block msglist %08X with %d signatures\n",svid,header.now,header.vok); //TODO, send error
        send_sync(put_msg);
    }

    void handle_read_msglist(const boost::system::error_code& error)
    {
        if(error){
            ELOG("%04X ERROR reading msglist\n",svid);
            leave();
            return;
        }
        servers header;
        assert(read_msg_->data!=NULL);
        memcpy(&header.now,read_msg_->data+4,4);

        if(server_.get_msglist!=header.now){
            ELOG("%04X ERROR got wrong msglist id\n",svid); // consider updating server
            leave();
            return;
        }

        header.header_get();
        uint32_t nok;
        memcpy(&nok,read_msg_->data+8,4);
        header.vok=(uint16_t)nok;
        if(read_msg_->len!=(12+header.vok*sizeof(svsi_t)+SHA256_DIGEST_LENGTH+header.msg*(2+4+SHA256_DIGEST_LENGTH)))
        {
            ELOG("%04X ERROR got wrong msglist length\n",svid); // consider updating server
            leave();
            return;
        }
        if(memcmp(read_msg_->data+12+header.vok*sizeof(svsi_t),header.msghash,SHA256_DIGEST_LENGTH))
        {
            ELOG("%04X ERROR got wrong msglist msghash\n",svid); // consider updating server
            char hash[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(hash,read_msg_->data+12+header.vok*sizeof(svsi_t),SHA256_DIGEST_LENGTH);
            ELOG("%04X MSGHASH got  %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
            ed25519_key2text(hash,header.msghash,SHA256_DIGEST_LENGTH);
            ELOG("%04X MSGHASH have %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
            leave();
            return;
        }
        if(header.vok*2<server_.last_srvs_.vtot && header.vok<opts_.mins){
            ELOG("%04X ERROR not enough signatures (%d<(%d/2&%d))\n",svid,header.vok,server_.last_srvs_.vtot,opts_.mins);
            leave();
            return;
        }
        header_t head;
        header.vno=0;
        header.header(head);
        server_.last_srvs_.check_signatures(head,(svsi_t*)(read_msg_->data+12),true);

        if(head.vok*2<server_.last_srvs_.vtot && head.vok<opts_.mins){
            ELOG("%04X ERROR not enough signatures after validation (%d<(%d/2&%d))\n",svid,head.vok,server_.last_srvs_.vtot,opts_.mins);
            leave();
            return;
        }

        header.header_put();
        server_.msgl_process(header,read_msg_->data+12+header.vok*sizeof(svsi_t));
      ////FIXME, process check and save in one function
      //std::map<uint64_t,message_ptr> map;
      //header.msgl_map((char*)(read_msg_->data+8),map,opts_.svid);
      //header.msgl_hash(map);
      ////if(!header.msg_check(map)){
      //if(memcmp(read_msg_->data+8,header.msghash,SHA256_DIGEST_LENGTH)){ //check again after hash calculation
      //  ELOG("%04X ERROR msghash check failed\n",svid); // consider updating server
      //  read_msg_ = boost::make_shared<message>();
      //  boost::asio::async_read(socket_,
      //    boost::asio::buffer(read_msg_->data,message::header_length),
      //    boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      //  return;}
      //server_.put_msglist(header.now,map, add message hashtree );

        asyncWaitForNewMessageHeader();
    }

    void write_bank() {                
        uint32_t path=read_msg_->msid;
        uint16_t bank=read_msg_->svid;

//FIXME !!!
//FIXME !!! check if the first undo directory is needed !!!
//FIXME !!! if peer starts at 'path' and receives messages in 'path' then it has to start from state before path
//FIXME !!! then the path/und files are needed to start in correct position
//FIXME !!!

        if(1+(srvs_.now-path)/BLOCKSEC>MAX_UNDO) {
            ELOG("%04X ERROR, too old sync point\n",svid);
            leave();
            return;
        }
        //check the numer of users at block time
        servers s;
        s.get(path);
        if(bank>=s.nodes.size()) {
            ELOG("%04X ERROR, bad bank at sync point\n",svid);
            leave();
            return;
        }
        std::vector<int> ud;
        uint32_t users=s.nodes[bank].users;
        //TODO, consider checking that the final hash is correct
        char filename[64];
        sprintf(filename,"usr/%04X.dat",bank);
        int fd=open(filename,O_RDONLY);
        int ld=0;
        if(fd<0) {
            ELOG("%04X ERROR, failed to open bank, weird !!!\n",svid);
            leave();
            return;
        }
        for(uint32_t block=path+BLOCKSEC; block<=srvs_.now; block++) {
            Helper::FileName::getUndo(filename, block, bank);
            int uf = open(filename, O_RDONLY);
            if(uf<0) {
                continue;
            }
            DLOG("%04X USING bank %04X block %08X undo %s\n",svid,bank,path,filename);
            ud.push_back(uf);
        }
        if(ud.size()) {
            ld=ud.back();
        }
        int msid=0;
        int64_t weight=0;
        //SHA256_CTX sha256;
        //SHA256_Init(&sha256);
        uint64_t csum[4]= {0,0,0,0};
        for(uint32_t user=0; user<users; msid++) {
            uint32_t end=user+MESSAGE_CHUNK;
            if(end>users) {
                end=users;
            }
            int len=end-user;
            message_ptr put_msg(new message(8+len*sizeof(user_t))); //6Mb working space
            put_msg->data[0]=MSGTYPE_USR;
            memcpy(put_msg->data+1,&len,3); // this is number of users (max 0x10000)
            memcpy(put_msg->data+4,&msid,2); // this is the chunk id
            memcpy(put_msg->data+6,&bank,2); // this is the bank
            user_t* u=(user_t*)(put_msg->data+8);
            for(; user<end; user++,u++) {
                u->msid=0;
                int junk=0;
                for(auto it=ud.begin(); it!=ud.end(); it++,junk++) {
                    read(*it,(char*)u,sizeof(user_t));
                    if(u->msid) {
                        DLOG("%04X USING bank %04X undo user %08X file %d\n",svid,bank,user,junk);
                        lseek(fd,sizeof(user_t),SEEK_CUR);
                        for(it++; it!=ud.end(); it++) {
                            lseek(*it,sizeof(user_t),SEEK_CUR);
                        }
                        goto NEXTUSER;
                    }
                }
                read(fd,u,sizeof(user_t));
                if(ld) { //confirm again that the undo file has not changed
                    user_t v;
                    v.msid=0;
                    lseek(ld,-sizeof(user_t),SEEK_CUR);
                    read(ld,&v,sizeof(user_t));
                    if(v.msid) { //overwrite user info
                        memcpy((char*)u,&v,sizeof(user_t));
                    }
                }
NEXTUSER:
                ;
                //print user
                DLOG("%04X USER:%04X m:%04X t:%08X s:%04X b:%04X u:%04X l:%08X r:%08X v:%016lX h:%08X\n",svid,
                     user,u->msid,u->time,u->stat,u->node,u->user,u->lpath,u->rpath,u->weight,*((uint32_t*)(u->csum)));
                weight+=u->weight;
                //SHA256_Update(&sha256,u,sizeof(user_t));
                //FIXME, debug only !!!
#ifndef NDEBUG
                {
                    user_t n;
                    memcpy(&n,u,sizeof(user_t));
                    server_.last_srvs_.user_csum(n,bank,user);
                    if(memcmp(n.csum,u->csum,32)) {
                        ELOG("%04X ERROR !!!, checksum mismatch for user %08X [%08X<>%08X]\n",svid,user,
                             *((uint32_t*)(n.csum)),*((uint32_t*)(u->csum)));
                        //exit(-1);
                        leave();
                        return;
                    }
                }
#endif
                server_.last_srvs_.xor4(csum,u->csum);
            }
            DLOG("%04X SENDING bank %04X block %08X chunk %X muser %X sum %016lX hash %08X head %016lX\n",svid,bank,path,msid,user,s.nodes[bank].weight,*((uint32_t*)csum),*(uint64_t*)put_msg->data);
            send_sync(put_msg); // send even if we have errors

            DLOG("%04X FINISH SENDING bank %04X block %08X chunk %X muser %X sum %016lX hash %08X head %016lX\n",svid,bank,path,msid,user,s.nodes[bank].weight,*((uint32_t*)csum),*(uint64_t*)put_msg->data);
            if(user==users) {
                //uint8_t hash[32];
                //SHA256_Final(hash,&sha256);
//FIXME, should compare with previous hash !!!
                if(s.nodes[bank].weight!=weight) {
                    //unlink(filename); //TODO, enable this later
                    ELOG("%04X ERROR sending bank %04X bad sum %016lX<>%016lX\n",svid,bank,s.nodes[bank].weight,weight);
                    leave();
                    break;
                }
                if(memcmp(s.nodes[bank].hash,csum,32)) {
                    //unlink(filename); //TODO, enable this later
                    ELOG("%04X ERROR sending bank %04X (bad hash)\n",svid,bank);
                    leave();
                    break;
                }
            }
        }
        close(fd);
        for(auto fu : ud) {
            close(fu);
        }
    }

    void asyncWaitForNewMessageHeader()
    {               
        read_msg_ = boost::make_shared<message>();

        m_netclient.asyncRead(read_msg_->data, message::header_length,
                              boost::bind(&peer::handle_read_header, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred), (BLOCKSEC));
    }

    void handle_read_bank(const boost::system::error_code& error)
    {
        int fd;
        if(error) {
            ELOG("%04X ERROR reading message\n",svid);
            leave();
            return;
        }
        if(!server_.do_sync) {
            DLOG("%04X DEBUG ignore usr message\n",svid);
            asyncWaitForNewMessageHeader();
            return;
        }
        uint16_t bank=read_msg_->svid;
        if(!bank || bank>=server_.last_srvs_.nodes.size()) {
            ELOG("%04X ERROR reading bank %04X (bad num)\n",svid,bank);
            leave();
            return;
        }
        if(read_msg_->len+0x10000*read_msg_->msid>server_.last_srvs_.nodes[bank].users) {
            ELOG("%04X ERROR reading bank %04X (too many users)\n",svid,bank);
            leave();
            return;
        }
        uint64_t hnum=server_.need_bank(bank);
        if(!hnum) {
            DLOG("%04X NO NEED for bank %04X\n",svid,bank);
            asyncWaitForNewMessageHeader();
            return;
        }
        ILOG("%04X PROCESSING bank %04X\n",svid,bank);
        char filename[64];
        sprintf(filename,"usr/%04X.dat.%04X",bank,svid);
        if(!read_msg_->msid) {
            hrb_last_bank=bank;
            hrb_last_msid=0;
            hrb_last_weight=0;
            bzero(hrb_last_csum,4*sizeof(uint64_t));
            //SHA256_Init(&sha256);
            fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
        } else {
            if(hrb_last_bank!=bank||hrb_last_msid!=read_msg_->msid-1) {
                unlink(filename);
                ELOG("%04X ERROR reading bank %04X (incorrect message order)\n",svid,bank);
                leave();
                return;
            }
            hrb_last_msid=read_msg_->msid;
            fd=open(filename,O_WRONLY|O_APPEND,0644);
        }
        if(fd<0) { //trow or something :-)
            ELOG("%04X ERROR creating bank %04X file\n",svid,bank);
            exit(-1);
        }
        //if sending user_t without csum, must split this into individual writes
        if((int)(read_msg_->len*sizeof(user_t))!=write(fd,read_msg_->data+8,read_msg_->len*sizeof(user_t))) {
            close(fd);
            unlink(filename);
            ELOG("%04X ERROR writing bank %04X file\n",svid,bank);
            exit(-1);
        }
        close(fd);
        user_t* u=(user_t*)(read_msg_->data+8);
        uint32_t uid=0x10000*read_msg_->msid;
        for(uint32_t i=0; i<read_msg_->len; i++,u++,uid++) {
            //DLOG("%04X USER:%08X m:%08X t:%08X s:%04X b:%04X u:%08X l:%08X r:%08X v:%016lX\n",svid,
            //  uid,u->msid,u->time,u->stat,u->node,u->user,u->lpath,u->rpath,u->weight);
            DLOG("%04X USER:%04X m:%04X t:%08X s:%04X b:%04X u:%04X l:%08X r:%08X v:%016lX h:%08X\n",svid,
                 uid,u->msid,u->time,u->stat,u->node,u->user,u->lpath,u->rpath,u->weight,*((uint32_t*)(u->csum)));
            hrb_last_weight+=u->weight;
            //SHA256_Update(&sha256,u,sizeof(user_t));
            server_.last_srvs_.user_csum(*u,bank,uid); //overwrite u.csum (TODO consider not sending over network!!!)
            server_.last_srvs_.xor4(hrb_last_csum,u->csum);
        }
        if(read_msg_->len+0x10000*read_msg_->msid<server_.last_srvs_.nodes[bank].users) {
            asyncWaitForNewMessageHeader();
            return;
        }
        ILOG("%04X GOT bank %04X users %08X sum %016lX hash %08X\n",svid,bank,uid,hrb_last_weight,(uint32_t)(hrb_last_csum[0]&0xFFFFFFFF));
        //uint8_t hash[32];
        //SHA256_Final(hash,&sha256);
        //if(memcmp(server_.last_srvs_.nodes[bank].hash,hash,32))
        if(server_.last_srvs_.nodes[bank].weight!=hrb_last_weight) {
            //unlink(filename); //TODO, enable this later
            ELOG("%04X ERROR reading bank %04X (bad sum)\n",svid,bank);
            leave();
            return;
        }
        if(memcmp(server_.last_srvs_.nodes[bank].hash,hrb_last_csum,4*sizeof(uint64_t))) {
            //unlink(filename); //TODO, enable this later
            uint32_t* h=(uint32_t*)server_.last_srvs_.nodes[bank].hash;
            ELOG("%04X ERROR reading bank %04X (bad hash) [%08X<>%08X]\n",svid,bank,*h,(uint32_t)(hrb_last_csum[0]&0xFFFFFFFF));
            leave();
            return;
        }
        char new_name[64];
        sprintf(new_name,"usr/%04X.dat",bank);
        rename(filename,new_name);
        DLOG("%04X PROCESSED bank %04X\n",svid,bank);
        server_.have_bank(hnum);
        DLOG("%04X CONTINUE after bank processing\n",svid);
        asyncWaitForNewMessageHeader();
    }

    void write_servers() {
        uint32_t now;
        assert(read_msg_->data!=NULL);
        memcpy(&now,read_msg_->data+1,4);
        DLOG("%04X SENDING block servers for block %08X\n",svid,now);
        if(server_.last_srvs_.now!=now) {
            //FIXME, try getting data from repository
            ELOG("%04X ERROR, bad time %08X<>%08X\n",svid,server_.last_srvs_.now,now);
            leave();
            return;
        }
        message_ptr put_msg(new message(server_.last_srvs_.nod*sizeof(node_t)));
        if(!server_.last_srvs_.copy_nodes((node_t*)put_msg->data,server_.last_srvs_.nod)) {
            ELOG("%04X ERROR, failed to copy nodes :-(\n",svid);
            leave();
            return;
        }
        send_sync(put_msg);
    }

    void handle_read_servers() {
        if(server_.fast_sync(false,peer_hs.head,peer_nods,peer_svsi)<0) {
            return;
        }
        //send
        DLOG("%04X SENDING block servers request\n",svid);
        message_ptr put_msg(new message());
        put_msg->data[0]=MSGTYPE_SER;
        memcpy(put_msg->data+1,&peer_hs.head.now,4);
        send_sync(put_msg);
        peer_nods=(node_t*)malloc(peer_hs.head.nod*sizeof(node_t));
        assert(peer_nods!=NULL);
        int len=0;
        try {
            len=m_netclient.readSync(peer_nods,peer_hs.head.nod*sizeof(node_t), DEFAULT_NET_TIMEOUT);
            //len=boost::asio::read(socket_,boost::asio::buffer(peer_nods,peer_hs.head.nod*sizeof(node_t)));
        } catch (std::exception& e) {
            ELOG("%04X CATCH asio error (handle_read_servers): %s\n",svid,e.what());
            free(peer_svsi);
            free(peer_nods);
            throw std::runtime_error("CATCH asio error (handle_read_servers)\n");
        }
        if(len!=(int)(peer_hs.head.nod*sizeof(node_t))) {
            ELOG("%04X READ servers error\n",svid);
            free(peer_svsi);
            free(peer_nods);
            throw std::runtime_error("CATCH asio error (handle_read_servers)\n");
        }
        if(server_.last_srvs_.check_nodes(peer_nods,peer_hs.head.nod,peer_hs.head.nodhash)) {
            ELOG("%04X SERVERS incompatible with hash\n",svid);
            char hash[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(hash,sync_hs.head.nodhash,SHA256_DIGEST_LENGTH);
            ELOG("%04X NODHASH peer %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
            free(peer_svsi);
            free(peer_nods);
            throw std::runtime_error("CATCH asio error (handle_read_servers)\n");
        }
        DLOG("%04X FINISH SYNC\n",svid);
        server_.fast_sync(true,peer_hs.head,peer_nods,peer_svsi); // should use last_srvs_ instead of sync_...
        free(peer_svsi);
        free(peer_nods);
    }

    int authenticate() { //FIXME, don't send last block because signatures are missing; send the one before last
        uint32_t now=time(NULL);
        uint32_t blocknow=now-(now%BLOCKSEC);
        msid=read_msg_->msid;
        svid=read_msg_->svid;

        //set thread name.
        setIoThreadName();

        setState(ST_AUTHENTCATING);

        DLOG("%04X PEER HEADER %04X:\n",svid,msid);
        assert(read_msg_->data!=NULL);
        memcpy(&peer_hs,read_msg_->data+4+64+10,sizeof(handshake_t));
        srvs_.header_print(peer_hs.head);

        /*if(opts_.fast && server_.do_fast)
        {
            char hash_text[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(hash_text, peer_hs.head.viphash, SHA256_DIGEST_LENGTH);
            ELOG("Fast sync VIPHASH: %.*s\n", 2*SHA256_DIGEST_LENGTH, hash_text);
            if(strncmp(hash_text, opts_.viphash.c_str(), 2*SHA256_DIGEST_LENGTH)) {
                ELOG("Synced to invalid VIPHASH, expected %.*s\n", 2*SHA256_DIGEST_LENGTH, opts_.viphash.c_str());
                return 0;
            }
        }*/

        //memcpy(&sync_head,&peer_hs.head,sizeof(header_t));
        if(read_msg_->svid==opts_.svid) {
            ELOG("%04X ERROR: connecting to myself\n",svid);
            return(0);
        }
        if(m_peerManager.checkDuplicate(this->svid)) {
            ELOG("%04X ERROR: server already connected\n",svid);
            return(0);
        }
        if(peer_hs.head.nod>srvs_.nodes.size() && incoming_) {
            ELOG("%04X ERROR: too high number of servers for incoming connection\n",svid);
            return(0);
        }
        if(read_msg_->now>now+2 || read_msg_->now<now-2) {
            ELOG("%04X ERROR: bad time %08X<>%08X\n",svid,read_msg_->now,now);
            return(0);
        }
        if(peer_hs.msid!=0xFFFFFFFF && peer_hs.msid>server_.msid_) { //FIXME, we will check this later, maybe not needed here
            ELOG("%04X WARNING, possible message loss (my msid peer:%08X>me:%08X)\n",svid,peer_hs.msid,server_.msid_);
        }
        if(peer_hs.msid && peer_hs.msid==server_.msid_ && server_.msid_==srvs_.nodes[opts_.svid].msid &&
                memcmp(peer_hs.msha,srvs_.nodes[opts_.svid].msha,SHA256_DIGEST_LENGTH)) { //FIXME, we should check this later, maybe not needed
            char hash[2*SHA256_DIGEST_LENGTH];
            ELOG("%04X WARNING, last message hash mismatch, should run full resync\n",svid);
            ed25519_key2text(hash,srvs_.nodes[opts_.svid].msha,SHA256_DIGEST_LENGTH);
            ELOG("%04X HASH have %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
            ed25519_key2text(hash,peer_hs.msha,SHA256_DIGEST_LENGTH);
            ELOG("%04X HASH got  %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
        }
        if(incoming_) {
            DLOG("%04X INCOMING HANDSHAKE \n",svid);
            message_ptr sync_msg=server_.write_handshake(svid,sync_hs); // sets sync_hs, READONLY ok
            sync_msg->print("; send welcome");
            send_sync(sync_msg);
        }
        //if(peer_hs.head.now==sync_hs.head.now && peer_hs.msid!=0xFFFFFFFF && sync_hs.msid!=0xFFFFFFFF){
        if(peer_hs.head.now==sync_hs.head.now) {
            if(memcmp(peer_hs.head.oldhash,sync_hs.head.oldhash,SHA256_DIGEST_LENGTH)) {
                char hash[2*SHA256_DIGEST_LENGTH];
                ELOG("%04X ERROR oldhash mismatch, FIXME, move back more blocks to sync\n",svid);
                ed25519_key2text(hash,sync_hs.head.oldhash,SHA256_DIGEST_LENGTH);
                ELOG("%04X HASH have %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
                ed25519_key2text(hash,peer_hs.head.oldhash,SHA256_DIGEST_LENGTH);
                ELOG("%04X HASH got  %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
                return(0);
            }
            if(memcmp(peer_hs.head.nowhash,sync_hs.head.nowhash,SHA256_DIGEST_LENGTH)) {
                char hash[2*SHA256_DIGEST_LENGTH];
                ELOG("%04X WARNING nowhash mismatch, not tested :-( move back one block to sync\n",svid);
                ed25519_key2text(hash,sync_hs.head.nowhash,SHA256_DIGEST_LENGTH);
                ELOG("%04X HASH have %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
                ed25519_key2text(hash,peer_hs.head.nowhash,SHA256_DIGEST_LENGTH);
                ELOG("%04X HASH got  %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
                return(0);
            }
        }
        //if(!server_.do_sync){ //we are in sync
        if(!sync_hs.do_sync) { //we are in sync
            if(peer_hs.head.now>sync_hs.head.now) {
                ELOG("%04X ERROR not ready to connect with this server\n",svid);
                return(0);
            }
            //if(peer_hs.head.now<sync_hs.head.now){
            if(peer_hs.do_sync) {
                ELOG("%04X Authenticated, provide sync data\n",svid);
                //write_sync(); // peer will disconnect if peer does not want the data
                int vok=sync_hs.head.vok;
                int vno=sync_hs.head.vno;
                int size=sizeof(svsi_t)*(vok+vno);
                message_ptr put_msg(new message(size));
                server_.last_srvs_.get_signatures(sync_hs.head.now,put_msg->data,vok,vno);//TODO, check if server_.last_srvs_ has public keys
                DLOG("%04X SENDING last block signatures ok:%d+no:%d (%d bytes)\n",svid,vok,vno,size);
                send_sync(put_msg);
                setState(ST_SYNCD);
                return(1);
            } else {
                ELOG("%04X Authenticated, peer in sync\n",svid);
                update_sync();
                last_active=now; // protect from disconnect
                do_sync=0;
            }
            setState(ST_SYNCD);
            return(1);
        }
        // try syncing from this server
        //if(peer_hs.head.now!=srvs_.now-BLOCKSEC)
        if(peer_hs.head.now!=blocknow-BLOCKSEC || peer_hs.do_sync) {
            ELOG("%04X PEER not in sync\n",svid);
            return(0);
        }
        //if(peer_hs.head.vok<server_.vip_max/2 && (!opts_.mins || peer_hs.head.vok<opts_.mins))
        if(peer_hs.head.vok*2<server_.last_srvs_.vtot && peer_hs.head.vok<opts_.mins) {
            ELOG("%04X PEER not enough signatures (%d:%d && %d:%d) for %08X\n",svid,
                 peer_hs.head.vok,server_.last_srvs_.vtot,peer_hs.head.vok,opts_.mins,server_.last_srvs_.now);
            return(0);
        }
        DLOG("%04X Authenticated, expecting sync data (%u bytes)\n",svid,
             (uint32_t)((peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t)));
        peer_svsi=(svsi_t*)malloc((peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t)); //FIXME, send only vok
        int len=0;
        try {
            len=m_netclient.readSync(peer_svsi,(peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t), DEFAULT_NET_TIMEOUT);
        } catch (std::exception& e) {
            DLOG("%04X CATCH asio error (authenticate): %s\n",svid,e.what());            
            free(peer_svsi);
            throw std::runtime_error("READ block signatures error\n");
            return(0);
        }
        if(len!=(int)((peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t))) {
            ELOG("%04X READ block signatures error\n",svid);
            free(peer_svsi);
            throw std::runtime_error("ERROR in authenticate:read vok vno\n");
            return(0);
        }
        DLOG("%04X BLOCK signatures recieved ok:%d no:%d\n",svid,peer_hs.head.vok,peer_hs.head.vno);
        if(!(do_sync && opts_.fast && opts_.svid == 0))
        {
          server_.last_srvs_.check_signatures(peer_hs.head,peer_svsi,true);//TODO, check if server_.last_srvs_ has public keys
          //if(peer_hs.head.vok<server_.vip_max/2 && (!opts_.mins || peer_hs.head.vok<opts_.mins)){
          if(peer_hs.head.vok*2<server_.last_srvs_.vtot && peer_hs.head.vok<opts_.mins)
          {
            ELOG("%04X READ not enough signatures after validaiton\n",svid);
            free(peer_svsi);
            return(0);
          }
        }
        //now decide if You want to sync to last stage first ; or load missing blocks and messages first, You can decide based on size of databases and time to next block
        //the decision should be in fact made at the beginning by the server
        server_.add_electors(peer_hs.head,peer_svsi);
        if(opts_.fast) {
            handle_read_servers();
            server_.last_srvs_.header(sync_hs.head);
        } // set new starting point for headers synchronisation
        handle_read_headers();
        //FIXME, brakes assert in send_sync() !!!
        last_active=now; // protect from disconnect
        do_sync=0; // set peer in sync, we are not in sync (server_.do_sync==1)

        ELOG("%04X ADD active peer !!!!!!!!!!!\n",svid);

        setState(ST_SYNCD);
        return(1);
    }

    void handle_read_body(const boost::system::error_code& error) {
        extern message_ptr nullmsg;
        assert(read_msg_->data[0]!=MSGTYPE_BLK);
        if(error) {
            ELOG("%04X ERROR reading message\n",svid);
            leave();
            return;
        }
        read_msg_->read_head();
        if(!read_msg_->svid && svid==BANK_MAX) { // READONLY ok
            if(!authenticate()) {
                ELOG("%04X NOT authenticated\n",svid);
                leave();
                return;
            }

            busy=0; // make peer available for download traffic
            ELOG("%04X CONTINUE after authentication1\n",svid);
            asyncWaitForNewMessageHeader();
            return;
        }

        if(opts_.mins || svid!=BANK_MAX || !server_.do_sync) {
            if(!read_msg_->svid || read_msg_->svid>=srvs_.nodes.size()) { // READONLY ok
                if(srvs_.nodes[0].status & SERVER_FST) {
                  //srvs_.init_fast(read_msg_->svid, srvs_.nodes[opts_.svid].pk);
                } else {
                    ELOG("%04X ERROR reading head (bad svid %04X)\n",svid,read_msg_->svid);
                    leave();
                    return;
                }
            }
            uint8_t* msha=srvs_.nodes[read_msg_->svid].msha;
            if(read_msg_->data[0]==MSGTYPE_MSG) {
                if(server_.last_srvs_.nodes[read_msg_->svid].msid>=read_msg_->msid) {
                    DLOG("%04X IGNORE message with too old msid %04X:%08X<=%08X\n",svid,
                         read_msg_->svid,read_msg_->msid,server_.last_srvs_.nodes[read_msg_->svid].msid);
                    asyncWaitForNewMessageHeader();
                    return;
                }
                if(srvs_.nodes[read_msg_->svid].msid!=read_msg_->msid-1) {
                    message_ptr prev=server_.message_svidmsid(read_msg_->svid,read_msg_->msid-1);
                    if(prev!=nullmsg && (prev->status & (MSGSTAT_DAT|MSGSTAT_VAL))) {
                        msha=prev->sigh;
                    } else if(server_.last_srvs_.nodes[read_msg_->svid].msid==read_msg_->msid-1) {
                        msha=server_.last_srvs_.nodes[read_msg_->svid].msha;
                    } else {
                        DLOG("%04X ERROR LOADING mistimed future message %04X:%08X!=%08X+1\n",svid,
                             read_msg_->svid,read_msg_->msid,srvs_.nodes[read_msg_->svid].msid);
                        asyncWaitForNewMessageHeader();
                        return;
                    }
                    DLOG("%04X LOADING mistimed message %04X:%08X!=%08X+1\n",svid,
                         read_msg_->svid,read_msg_->msid,srvs_.nodes[read_msg_->svid].msid);
                }
                if(!read_msg_->hash_tree()) {
                    DLOG("%04X ERROR calculating hash tree for message %04X:%08X\n",svid,read_msg_->svid,read_msg_->msid);
                    asyncWaitForNewMessageHeader();
                    return;
                }
            } else if(read_msg_->data[0]==MSGTYPE_INI || read_msg_->data[0]==MSGTYPE_CND) {
                read_msg_->hash_signature();
            }
            if(!(srvs_.nodes[0].status & SERVER_FST) && read_msg_->check_signature(srvs_.nodes[read_msg_->svid].pk,opts_.svid,msha)) {
                //FIXME, this can be also a double spend, do not loose it
                ELOG("%04X BAD signature %04X:%08X (last msid:%08X) %016lX!!!\n\n",svid,read_msg_->svid,read_msg_->msid,
                     srvs_.nodes[read_msg_->svid].msid,read_msg_->hash.num);
                if(read_msg_->data[0]!=MSGTYPE_MSG) {
                    leave();
                    return;
                }
                if(read_msg_->msid==server_.last_srvs_.nodes[read_msg_->svid].msid+1) {
                    leave();
                    return;
                }
                DLOG("%04X store message %04X:%08X with bad signature (dbl?)\n",svid,read_msg_->svid,read_msg_->msid);
                read_msg_->status|=MSGSTAT_SIG;
                server_.bad_insert(read_msg_);
                asyncWaitForNewMessageHeader();
                return;
            }
        }

        if(svid==BANK_MAX) { // READONLY ok
            if(!authenticate()) {
                ELOG("%04X NOT authenticated\n",svid);
                leave();
                return;
            }

            busy=0; // make peer available for download traffic
            ELOG("%04X CONTINUE after authentication2\n",svid);
            asyncWaitForNewMessageHeader();
            return;
        }
        //if this is a candidate vote
        if(read_msg_->hash.dat[1]==MSGTYPE_CND) {
//FIXME, move this to handle_read_candidate
            if(!parse_vote()) {
                ELOG("%04X PARSE vote FAILED\n",svid);
                leave();
                return;
            }
        }
        //TODO, check if correct server message number ! and update the server
        server_.message_insert(read_msg_);
        asyncWaitForNewMessageHeader();
    }

    void handle_read_block(const boost::system::error_code& error) {
        DLOG("%04X BLOCK, got BLOCK\n",svid);
        if(error) { // READONLY ok
            ELOG("%04X READ error %d %s (BLOCK)\n",svid,error.value(),error.message().c_str());
            leave();
            return;
        }
        if(read_msg_->len==4+64+10) { //adding missing data
            if(server_.last_srvs_.now!=read_msg_->msid) {
                DLOG("%04X BLOCK READ problem, ignoring short BLK message due to msid mismatch, msid:%08X block:%08X\n",svid,read_msg_->msid,server_.last_srvs_.now);
                asyncWaitForNewMessageHeader();
                return;
            }
            header_t* h=(header_t*)(read_msg_->data+4+64+10);
            read_msg_->len+=sizeof(header_t);
            read_msg_->data=(uint8_t*)realloc(read_msg_->data,read_msg_->len); // throw if no RAM ???
            server_.last_srvs_.header(*h);
        }
        read_msg_->hash_signature();
        read_msg_->read_head();
        if(!read_msg_->svid || read_msg_->svid>=srvs_.nodes.size()) {
            ELOG("%04X ERROR reading head\n",svid);
            leave();
            return;
        }
        if(read_msg_->check_signature(srvs_.nodes[read_msg_->svid].pk,opts_.svid,srvs_.nodes[read_msg_->svid].msha)) {
            ELOG("%04X BLOCK signature error\n",svid);
            leave();
            return;
        }
        if((read_msg_->msid!=server_.last_srvs_.now && read_msg_->msid!=srvs_.now) || read_msg_->now<read_msg_->msid || read_msg_->now>=read_msg_->msid+2*BLOCKSEC) {
            ELOG("%04X BLOCK TIME error now:%08X msid:%08X block[-1].now:%08X block[].now:%08X \n",svid,read_msg_->now,read_msg_->msid,server_.last_srvs_.now,srvs_.now);
            leave();
            return;
        }
        if(read_msg_->svid==svid) {
            DLOG("%04X BLOCK\n",svid);
            assert(read_msg_->data!=NULL);
            memcpy(&peer_hs.head,read_msg_->data+4+64+10,sizeof(header_t));
#ifdef DEBUG
            char hash[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(hash,peer_hs.head.nowhash,SHA256_DIGEST_LENGTH);
            DLOG("%04X BLOCK %08X %.*s from PEER\n",svid,peer_hs.head.now,2*SHA256_DIGEST_LENGTH,hash);
#endif
        }
        DLOG("%04X INSERT BLOCK message\n",svid);
        server_.message_insert(read_msg_);
        asyncWaitForNewMessageHeader();
    }

    void message_phash(uint8_t* mhash,std::map<uint64_t,hash_s>& map) {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        for(auto it=map.begin(); it!=map.end(); ++it) {
#ifndef NDEBUG
            char sigh[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(sigh,it->second.hash,SHA256_DIGEST_LENGTH);
            DLOG("%04X HASH add %04X:%08X %.*s\n",svid,*(uint16_t*)(((char*)&it->first)+6),*(uint32_t*)(((char*)&it->first)+2),2*SHA256_DIGEST_LENGTH,sigh);
#endif
            SHA256_Update(&sha256,it->second.hash,sizeof(hash_t));
        }
        SHA256_Final(mhash, &sha256);
    }

    void handle_read_stop(const boost::system::error_code& error) {
        DLOG("%04X PEER got STOP\n",svid);
        if(error) {
            ELOG("%04X READ error %d %s (STOP)\n",svid,error.value(),error.message().c_str());
            leave();
            return;
        }
        assert(read_msg_->data!=NULL);
        memcpy(last_message_hash,read_msg_->data+1,SHA256_DIGEST_LENGTH);
        char hash[2*SHA256_DIGEST_LENGTH];
        hash[2*SHA256_DIGEST_LENGTH-1]='?';
        ed25519_key2text(hash,last_message_hash,SHA256_DIGEST_LENGTH);
        DLOG("%04X LAST HASH got %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash);
        sync_start(false);
    }

    void sync_start(bool server) {
        DLOG("sync_start %d\n", server);
        boost::unique_lock<boost::mutex> lock(pio_);
        if(server) {
            BLOCK_SERV=true;
        } else {
            BLOCK_PEER=true;
        }
        DLOG("sync_start2 %d\n", server);
        if(!BLOCK_SERV || !BLOCK_PEER) {            
            DLOG("%04X PEER sync_start wait\n",svid);
            return;
        }
        lock.unlock(); // start synchronous communication
        DLOG("%04X PEER sync_start run (no UPDATE PEER SVID_MSID after this)\n",svid);
        bool serv_error;
        bool peer_error;

        try {
            write_serv_del();
            read_peer_del();
            write_serv_add(false);
            serv_error=read_peer_add(false);
            write_serv_check(serv_error);
            peer_error=read_peer_check();
            if(peer_error) {
                ELOG("%04X PEER SYNC problem\n",svid);
                write_serv_add(true);
            }
            if(serv_error) {
                ELOG("%04X SERV SYNC problem\n",svid);
                serv_error=read_peer_add(true);
                write_serv_check(serv_error);
            }
            if(peer_error) {
                peer_error=read_peer_check();
            }
            if(serv_error || peer_error) {
                ELOG("%04X SYNC failed with %s %s\n",svid,(serv_error?"server error":""),(peer_error?"peer error":""));
                leave();
                return;
            }
            sync_finish();
        } catch (std::exception& e) {
            leave();
            ELOG("%04X CATCH error (sync_start): %s\n",svid,e.what());
        }
    }

    void write_serv_del() { // send only info about missing messages first
        std::string data;
        DLOG("%04X write_serv_del\n",svid);
        uint16_t num_del=0; // this can be uint16_t;
        for(auto it=svid_msid_new.begin(); it!=svid_msid_new.end(); ++it) {
            auto pt=server_.LAST_block_svid_msgs.find(it->first); // could use svidmsid
            if(pt==server_.LAST_block_svid_msgs.end()) {
                const uint32_t msid=0;
                num_del++;
                data.append((const char*)&it->first,2);
                data.append((const char*)&msid,4);
                DLOG("%04X SERV del %04X:00000000\n",svid,it->first);
                continue;
            }
            if(it->second>pt->second->msid) {
                uint32_t msid=pt->second->msid;
                num_del++;
                data.append((const char*)&it->first,2);
                data.append((const char*)&msid,4);
                DLOG("%04X SERV del %04X:%08X\n",svid,it->first,msid);
            }
        }
        DLOG("%04X SERV del %d\n",svid,num_del);
        assert(data.size()==6*num_del);
        message_ptr put_msg(new message(2+data.size()));
        memcpy(put_msg->data,&num_del,2);
        if(data.size()) {
            memcpy(put_msg->data+2,data.c_str(),data.size());
        }
        try {            
            m_netclient.writeSync(put_msg->data,put_msg->len, DEFAULT_NET_TIMEOUT);
        } catch (std::exception& e) {
            ELOG("%04X CATCH asio error (write_serv_del): %s\n",svid,e.what());
            throw std::runtime_error("ERROR in write_serv_del()");
        }        
    }

    void read_peer_del() {
        int len=0;
        try {
            len=m_netclient.readSync(read_msg_->data,2, DEFAULT_NET_TIMEOUT);
        } catch (std::exception& e) {
            ELOG("%04X CATCH asio error (read_peer_del 1): %s\n",svid, e.what());
            throw std::runtime_error("ERROR in read_peer_del\n");
        }
        if(2!=len) {
            ELOG("%04X READ read_peer_del error\n",svid);            
            throw std::runtime_error("ERROR in read_peer_del\n");
        }
        assert(read_msg_->data!=NULL);
        memcpy(&peer_del,read_msg_->data,2);
        PEER_block_del_start.clear();
        if(!peer_del || peer_del==0xffff) {
            DLOG("%04X PEER del %s\n",svid,(peer_del?"all":"none"));
        } else {
            DLOG("%04X PEER del %d\n",svid,peer_del);
            free(read_msg_->data);
            read_msg_->data=(uint8_t*)malloc(6*peer_del);
            read_msg_->len=6*peer_del;
            try {
                len=m_netclient.readSync(read_msg_->data,read_msg_->len, DEFAULT_NET_TIMEOUT);
            } catch (std::exception& e) {
                len=0;
                DLOG("%04X CATCH asio error (read_peer_del 2): %s\n",svid,e.what());
                throw std::runtime_error("ERROR in read_peer_del\n");
            }
            if((int)read_msg_->len!=len) {
                ELOG("%04X READ read_peer_del error\n",svid);                
                throw std::runtime_error("ERROR in read_peer_del\n");
            }
            DLOG("%04X PEER read peer del\n",svid);
            for(int i=0; i<peer_del; i++) {
                uint16_t dsvid;
                uint32_t dmsid;
                memcpy(&dsvid,read_msg_->data+6*i,2);
                memcpy(&dmsid,read_msg_->data+6*i+2,4);
                PEER_block_del_start[dsvid]=dmsid;
                if(dsvid>=srvs_.nodes.size()) {
                    ELOG("%04X ERROR read_peer_del peersvid %04X\n",svid,dsvid);                    
                    throw std::runtime_error("ERROR read_peer_del peersvid\n");
                }
                DLOG("%04X PEER del %04X:%08X\n",svid,dsvid,dmsid);
            }
        }
        return;
    }

    void write_serv_add(bool peer_error) {
        DLOG("%04X write_serv_add run\n",svid);
        uint32_t num_add=0;
        uint16_t lastsvid=0;
        uint32_t maxmsid=0;
        std::string data;
        for(auto me=server_.LAST_block_all_msgs.begin(); me!=server_.LAST_block_all_msgs.end(); me++) {
            if(peer_del!=0xffff && !peer_error && me->second->svid!=lastsvid) {
                lastsvid=me->second->svid;
                auto pd=PEER_block_del_start.find(lastsvid);
                if(pd!=PEER_block_del_start.end()) {
                    maxmsid=pd->second;
                } else {
                    auto pt=svid_msid_new.find(lastsvid);
                    if(pt!=svid_msid_new.end()) {
                        maxmsid=pt->second;
                    } else {
                        maxmsid=0;
                    }
                }
            }
            if(peer_del==0xffff || peer_error || me->second->msid>maxmsid) {
                msidsvidhash_t mssvha;
                mssvha.msid=me->second->msid;
                mssvha.svid=me->second->svid;
                memcpy(mssvha.sigh,me->second->sigh,sizeof(hash_t));
                data.append((const char*)&mssvha,sizeof(msidsvidhash_t));
                char sigh[2*SHA256_DIGEST_LENGTH];
                ed25519_key2text(sigh,mssvha.sigh,SHA256_DIGEST_LENGTH);
                DLOG("%04X SERV add %04X:%08X %.*s\n",svid,mssvha.svid,mssvha.msid,2*SHA256_DIGEST_LENGTH,sigh);
                num_add++;
            }
        }
        DLOG("%04X SERV add %d\n",svid,num_add);
        assert(data.size()==sizeof(msidsvidhash_t)*num_add);
        message_ptr put_msg(new message(4+data.size()));
        memcpy(put_msg->data,&num_add,4);
        if(data.size()) {
            memcpy(put_msg->data+4,data.c_str(),data.size());
        }

        try {
            m_netclient.writeSync(put_msg->data,put_msg->len, DEFAULT_NET_TIMEOUT);
        }
        catch (std::exception& e) {
            ELOG("%04X CATCH asio error (write_serv_add): %s\n",svid,e.what());
            throw std::runtime_error("ERROR in write_serv_add()\n");
        }        
    }

    bool read_peer_add(bool error) {
        int len=0;
        try {
            len=m_netclient.readSync(read_msg_->data, 4, DEFAULT_NET_TIMEOUT);
        } catch (std::exception& e) {
            ELOG("%04X CATCH asio error (read_peer_add 1): %s\n",svid,e.what());
            throw std::runtime_error("ERROR in read_peer_add\n");
        }
        if(4!=len) {
            ELOG("%04X READ read_peer_add error\n",svid);            
            throw std::runtime_error("ERROR in read_peer_add\n");
        }
        memcpy(&peer_add,read_msg_->data,4);
        if(!peer_add) {
            DLOG("%04X PEER add none\n",svid);
        } else {
            DLOG("%04X PEER add %d\n",svid,peer_add);
            free(read_msg_->data);
            read_msg_->data=(uint8_t*)malloc(sizeof(msidsvidhash_t)*peer_add);
            read_msg_->len=sizeof(msidsvidhash_t)*peer_add;
            try {
                len=m_netclient.readSync(read_msg_->data, read_msg_->len, DEFAULT_NET_TIMEOUT);
            } catch (std::exception& e) {
                len=0;
                ELOG("%04X CATCH asio error (read_peer_add 2): %s\n",svid,e.what());
                throw std::runtime_error("ERROR in read_peer_add\n");
            }
            if((int)read_msg_->len!=len) {
                ELOG("%04X READ read_peer_add error\n",svid);                
                throw std::runtime_error("ERROR in read_peer_add\n");
            }
        }

        DLOG("%04X PEER read peer add\n",svid);
        PEER_block_del.clear();
        PEER_block_all.clear();
        if(!error && peer_del!=0xffff) {
            uint16_t lastsvid=0;
            uint32_t maxmsid=0;
            for(auto it=server_.LAST_block_all_msgs.begin(); it!=server_.LAST_block_all_msgs.end(); it++) {
                if(it->second->svid!=lastsvid) {
                    lastsvid=it->second->svid;
                    auto pd=PEER_block_del_start.find(lastsvid);
                    if(pd!=PEER_block_del_start.end()) {
                        maxmsid=pd->second;
                    } else {
                        auto pt=svid_msid_new.find(lastsvid);
                        if(pt!=svid_msid_new.end()) {
                            maxmsid=pt->second;
                        } else {
                            maxmsid=0;
                        }
                    }
                }
                if(it->second->msid>maxmsid) {
                    PEER_block_del.insert(it->first);
                } else {
                    //PEER_block_all[it->first]=*(hash_s*)it->second->sigh;
                    hash_s* hash_p=(hash_s*)it->second->sigh;
                    PEER_block_all[it->first]=*hash_p;
                }
            }
        } else {
            for(auto it=server_.LAST_block_all_msgs.begin(); it!=server_.LAST_block_all_msgs.end(); it++) {
                PEER_block_del.insert(it->first);
            }
        }
        PEER_block_add.clear();
        for(uint32_t i=0; i<peer_add; i++) {
            uint64_t key=0;
            memcpy(((char*)&key)+2,read_msg_->data+sizeof(msidsvidhash_t)*i,6);
            if(*(uint16_t*)(((char*)&key)+6)>=srvs_.nodes.size()) {
                ELOG("%04X ERROR read_peer_missing_messages peersvid %04X\n",svid,*(uint16_t*)(((char*)&key)+6));
                //leave();
                //throw("ERROR in read_peer_add\n");
                throw std::runtime_error("ERROR read_peer_missing_messages\n");
            }
            PEER_block_del.erase(key);
            PEER_block_add[key]=*(hash_s*)(read_msg_->data+sizeof(msidsvidhash_t)*i+6);
            PEER_block_all[key]=*(hash_s*)(read_msg_->data+sizeof(msidsvidhash_t)*i+6);
            char sigh[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(sigh,read_msg_->data+sizeof(msidsvidhash_t)*i+6,SHA256_DIGEST_LENGTH);
            DLOG("%04X PEER add %04X:%08X %.*s\n",svid,*(uint16_t*)(((char*)&key)+6),*(uint32_t*)(((char*)&key)+2),2*SHA256_DIGEST_LENGTH,sigh);
        }

        // check data
        hash_t hash= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        message_phash(hash,PEER_block_all);
#ifndef NDEBUG
        char hash1[2*SHA256_DIGEST_LENGTH];
        char hash2[2*SHA256_DIGEST_LENGTH];
        ed25519_key2text(hash1,last_message_hash,SHA256_DIGEST_LENGTH);
        ed25519_key2text(hash2,hash,SHA256_DIGEST_LENGTH);
        DLOG("%04X HASH check:\n  %.*s vs\n  %.*s\n",svid,2*SHA256_DIGEST_LENGTH,hash1,2*SHA256_DIGEST_LENGTH,hash2);
#endif
        error=memcmp(last_message_hash,hash,SHA256_DIGEST_LENGTH);
        return(error);
    }

    void write_serv_check(bool error)
    {
        DLOG("%04X write_serv_check run\n",svid);
        uint8_t buf[1];
        buf[0]=error;

        try {
            m_netclient.writeSync(buf,1, DEFAULT_NET_TIMEOUT);
        }
        catch (std::exception& e) {
            DLOG("%04X CATCH asio error (write_serv_check): %s\n",svid,e.what());
            //leave();
            throw std::runtime_error("ERROR in write_serv_check()\n");
        }
    }

    bool read_peer_check() {
        uint8_t buf[1];
        int len=0;
        try {
            len=m_netclient.readSync(buf, 1, DEFAULT_NET_TIMEOUT);
        } catch (std::exception& e) {
            ELOG("%04X CATCH asio error (read_peer_check): %s\n",svid,e.what());
            throw std::runtime_error("ERROR in read_peer_check\n");
        }
        if(1!=len) {
            ELOG("%04X READ read_peer_check error\n",svid);            
            throw std::runtime_error("ERROR in read_peer_check\n");
        }
        DLOG("%04X PEER read peer check\n",svid);
        bool error=buf[0];
        return(error);
    }

    void sync_finish() {
        ELOG("%04X SYNC OK\n",svid);
        hash_s* hash_p=(hash_s*)last_message_hash;
        server_.save_candidate(BLOCK_MODE,*hash_p,PEER_block_add,PEER_block_del,svid);
        boost::lock_guard<boost::mutex> lock(pio_);
        if(!write_msgs_.empty()) {
            //int len=message_len(write_msgs_.front());
            int len=write_msgs_.front()->len;
            DLOG("%04X send after sync [%016lX len:%d]\n",svid,*(uint64_t*)write_msgs_.front()->data,len);
            m_netclient.asyncWrite(write_msgs_.front()->data,len, boost::bind(&peer::handle_write,this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }
        asyncWaitForNewMessageHeader();
        //save last synced block to protect peer from disconnect
        last_active=BLOCK_MODE;
        BLOCK_MODE=0;
        BLOCK_SERV=false;
        BLOCK_PEER=false;        
    }

    int parse_vote() { //TODO, make this function similar to handle_read_block (make this handle_read_candidate)
        extern candidate_ptr nullcnd;
        //if((read_msg_->msid!=server_.last_srvs_.now && read_msg_->msid!=srvs_.now) || read_msg_->now<read_msg_->msid || read_msg_->now>=read_msg_->msid+2*BLOCKSEC)
        if(read_msg_->msid<server_.LAST_block || read_msg_->msid>server_.LAST_block+BLOCKSEC || read_msg_->now<read_msg_->msid || read_msg_->now>=read_msg_->msid+2*BLOCKSEC) {
            ELOG("%04X BLOCK TIME error now:%08X msid:%08X block[-1].now:%08X block[].now:%08X \n",svid,read_msg_->now,read_msg_->msid,server_.last_srvs_.now,srvs_.now);
            return(0);
        }
//FIXME, are we ready ???
        while(read_msg_->msid>server_.LAST_block) {
            DLOG("%04X waiting for server to prepare new block data \n",svid);
            boost::this_thread::sleep(boost::posix_time::seconds(1));
        }
        hash_s cand;
        assert(read_msg_->data!=NULL);
        memcpy(cand.hash,read_msg_->data+message::data_offset,sizeof(hash_t));
        candidate_ptr c_ptr=server_.known_candidate(cand,svid);
        char hash[2*SHA256_DIGEST_LENGTH];
        ed25519_key2text(hash,cand.hash,SHA256_DIGEST_LENGTH);
        DLOG("%04X CAND %04X %.*s (len:%d)\n",svid,read_msg_->svid,2*SHA256_DIGEST_LENGTH,hash,read_msg_->len);
        if(c_ptr==nullcnd) {
            DLOG("%04X PARSE --NEW-- vote for --NEW-- candidate\n",svid);
            if(read_msg_->len<4+64+10+sizeof(hash_t)+4+2) {
                ELOG("%04X PARSE vote short message read FATAL\n",svid);
                return(0);
            }
            uint32_t num_del;
            uint32_t num_add;
            memcpy(&num_del,read_msg_->data+4+64+10+sizeof(hash_t),4);
            memcpy(&num_add,read_msg_->data+4+64+10+sizeof(hash_t)+4,4);
            if(!num_add && !num_del) {
                ELOG("%04X PARSE vote empty change list FATAL\n",svid);
                return(0);
            }
            if(read_msg_->len<4+64+10+sizeof(hash_t)+4+4+num_add*sizeof(msidsvidhash_t)+num_del*6) {
                ELOG("%04X PARSE vote bad message length read FATAL (len:%d)\n",svid,(int)(4+64+10+sizeof(hash_t)+4+4+num_add*sizeof(msidsvidhash_t)+num_del*6));
                return(0);
            }
            std::set<uint64_t> peer_block_del(PEER_block_del);
            std::map<uint64_t,hash_s> peer_block_add(PEER_block_add);
            std::map<uint64_t,hash_s> peer_block_all(PEER_block_all);
            uint8_t *d=read_msg_->data+4+64+10+sizeof(hash_t)+4+4;
            for(uint32_t i=0; i<num_del; i++,d+=6) {
                uint64_t key=0;
                memcpy(((char*)&key)+2,d,6);
                peer_block_add.erase(key);
                peer_block_all.erase(key);
                peer_block_del.insert(key);
            }
            for(uint32_t i=0; i<num_add; i++,d+=sizeof(msidsvidhash_t)) {
                uint64_t key=0;
                memcpy(((char*)&key)+2,d,6);
                peer_block_del.erase(key);
                peer_block_all[key]=*(hash_s*)(d+6);
                peer_block_add[key]=*(hash_s*)(d+6);
            }
            hash_t tmp_hash;
            message_phash(tmp_hash,peer_block_all);
            if(memcmp(cand.hash,tmp_hash,SHA256_DIGEST_LENGTH)) {
                ELOG("%04X ERROR parsing hash from peer\n",svid);
                return(0);
            }
            DLOG("%04X OK candidate from peer\n",svid);
            c_ptr=server_.save_candidate(read_msg_->msid,cand,peer_block_add,peer_block_del,svid);
        } else {
            DLOG("%04X PARSE vote for known candidate\n",svid);
        }

        //modify tail from message
        uint32_t oldlen=read_msg_->len;
        if(!c_ptr->msg_add.empty() || !c_ptr->msg_del.empty()) {
            uint32_t num_add=c_ptr->msg_add.size();
            uint32_t num_del=c_ptr->msg_del.size();
            read_msg_->len=message::data_offset+sizeof(hash_t)+4+4+num_add*sizeof(msidsvidhash_t)+num_del*6;
            read_msg_->data=(uint8_t*)realloc(read_msg_->data,read_msg_->len); // throw if no RAM ???
            memcpy(read_msg_->data+1,&read_msg_->len,3); // remember length
            memcpy(read_msg_->data+message::data_offset+sizeof(hash_t),&num_del,4);
            memcpy(read_msg_->data+message::data_offset+sizeof(hash_t)+4,&num_add,4);
            uint8_t* d=read_msg_->data+message::data_offset+sizeof(hash_t)+4+4;
            for(auto it=c_ptr->msg_del.begin(); it!=c_ptr->msg_del.end(); it++,d+=6) { //miss second
                memcpy(d,((char*)&(*it))+2,6);
            }
            for(auto it=c_ptr->msg_add.begin(); it!=c_ptr->msg_add.end(); it++,d+=sizeof(msidsvidhash_t)) { //have first!
                memcpy(d,((char*)&it->first)+2,6);
                memcpy(d+6,&it->second.hash,sizeof(hash_t));
            }
            DLOG("%04X CHANGE CAND LENGTH! [len:%d->%d]\n",svid,oldlen,read_msg_->len);
            assert((uint32_t)(d-read_msg_->data)==read_msg_->len);
        } else {
            read_msg_->len=message::data_offset+sizeof(hash_t); // remember length
            read_msg_->data=(uint8_t*)realloc(read_msg_->data,read_msg_->len);
            memcpy(read_msg_->data+1,&read_msg_->len,3);
        }
        if(oldlen!=read_msg_->len) {
            DLOG("%04X STORE DIFFERENT CAND LENGTH [len:%d->%d]\n",svid,oldlen,read_msg_->len);
            memcpy(read_msg_->data+1,&read_msg_->len,3);
        }
        return(1);
    }    

    uint32_t    getSvid()
    {
        return svid;
    }

    friend class PeerConnectManager;

  private:
    uint32_t    last_received_msg_time=0; //time when msg was last received from peer
    uint32_t    last_received_msg_type=0; //type of last received msg from peer
    uint32_t    svid; // svid of peer
    int         do_sync; // needed by server::get_more_headers , FIXME, remove this, user peer_hs.do_sync    
    uint32_t    busy; // waiting for response (used during sync load balancing) set to last request time
    uint32_t    last_active; // updated with every block sync, protects from disconnect, could be read only (private)

    // data from peer
    std::string addr;
    uint32_t    port; // not needed    

    boost::asio::io_service peer_io_service_;	//TH
    boost::asio::io_service::work work_;		//TH
    boost::asio::ip::tcp::socket socket_;    

    PeerClient          m_netclient;
    PeerConnectManager& m_peerManager;
    PeerState           m_state;

    std::unique_ptr<boost::thread> iothp_;			//TH
    server& server_;
    bool incoming_;
    servers& srvs_; //FIXME ==server_.srvs_
    options& opts_; //FIXME ==server_.opts_


    //handle read bank
    uint16_t    hrb_last_bank{0};
    uint16_t    hrb_last_msid{0};
    int64_t     hrb_last_weight{0};
    uint64_t    hrb_last_csum[4] = {0,0,0,0};

    uint32_t msid;
    uint32_t peer_path; //used to load data when syncing

    handshake_t sync_hs;
    handshake_t peer_hs;
    svsi_t* peer_svsi;
    node_t* peer_nods;

    message_ptr read_msg_;
    message_queue write_msgs_;
    // statistics
    uint32_t files_out;
    uint32_t files_in;
    uint64_t bytes_out;
    uint64_t bytes_in;
    // block hash of messages
    uint8_t last_message_hash[SHA256_DIGEST_LENGTH]; //used in block building
    std::map<uint16_t,uint32_t> svid_msid_new; // highest msid for each svid known to peer or server
    uint32_t peer_add;
    uint16_t peer_del;
    uint32_t BLOCK_MODE;
    bool BLOCK_SERV;
    bool BLOCK_PEER;
    std::map<uint16_t,uint32_t> PEER_block_del_start;
    std::set<uint64_t> PEER_block_del;
    std::map<uint64_t,hash_s> PEER_block_add;
    std::map<uint64_t,hash_s> PEER_block_all;
    //mutexes
    boost::mutex pio_; //LOCK: mtx_
};

#endif // PEER_HPP
