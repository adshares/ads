//#define _GNU_SOURCE
#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
//#include <boost/archive/text_iarchive.hpp>
//#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
//#include <boost/serialization/list.hpp>
//#include <boost/serialization/vector.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
//#include <boost/container/flat_set.hpp>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <dirent.h>
#include <fcntl.h>
#include <forward_list>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <list>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <set>
#include <stack>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include "default.hpp"
#include "ed25519/ed25519.h"
#include "hash.hpp"
#include "helper/hlog.h"
#include "user.hpp"
#include "options.hpp"
#include "message.hpp"
#include "servers.hpp"
#include "candidate.hpp"
#include "server.hpp"
#include "peer.hpp"
#include "office.hpp"
#include "client.hpp"

bool finish=false;
boost::mutex flog;
boost::mutex siglock;
FILE* stdlog=NULL;
candidate_ptr nullcnd;
message_ptr nullmsg;
std::promise<int> prom;

void signal_handler(int signal) {
    DLOG("\nSIGNAL: %d\n\n",signal);
    if(signal==SIGSEGV || signal==SIGABRT) {
        if(stdout!=NULL) {
            fflush(stdout);
        }
        if(stderr!=NULL) {
            fflush(stderr);
        }
        if(stdlog!=NULL) {
            fflush(stdlog);
        }
        std::signal(SIGABRT,SIG_DFL);
        abort();
    }
    if(signal==SIGINT || signal==SIGQUIT || signal==SIGTERM || signal==SIGUSR1) {
        if(!finish) {
            prom.set_value(signal);
            finish=true;
        }
    }
}

int main(int argc, char* argv[]) {

#ifndef NDEBUG
    std::setbuf(stdout,NULL);
#endif

    std::signal(SIGINT,signal_handler);
    std::signal(SIGQUIT,signal_handler);
    std::signal(SIGABRT,signal_handler);
    std::signal(SIGTERM,signal_handler);
    std::signal(SIGSEGV,signal_handler);
    //std::signal(SIGHUP,signal_handler); //maybe used by asio
    //std::signal(SIGILL,signal_handler);
    //std::signal(SIGFPE,signal_handler);
    std::signal(SIGUSR1,signal_handler);
    //std::signal(SIGUSR2,signal_handler);
    stdlog=fopen("log.txt","w");
    options opt;
    opt.get(argc,argv);
    FILE *lock=fopen(".lock","a");
    assert(lock!=NULL);
    fprintf(lock,"%s:%d/%d\n",opt.addr.c_str(),opt.port,opt.svid);
    fclose(lock);
    try {
        int signal=SIGUSR1;
        while(signal==SIGUSR1) {
            finish=false;
            std::future<int> fut=prom.get_future();
            server s(opt);
            office o(opt,s);
            s.ofip=&o;
            s.start();
            opt.back=0;
            signal=fut.get();
            if(signal==SIGUSR1) {
                opt.fast=false;
                opt.init=false;
                ELOG("\n\nRESTARTING\n\n\n");
            } else {
                ELOG("Shutting down\n");
            }
            o.stop();
            s.stop();
            prom=std::promise<int>();
        }
    } catch (std::exception& e) {
        ELOG("MAIN Exception: %s\n",e.what());
    }
    fclose(stdlog);
    unlink(".lock");
    return(0);
}

// office <-> client
void office::start_accept() {
    if(!run) {
        return;
    }

    client_ptr c(new client(*io_services_[next_io_service_++],*this));
#ifdef DEBUG
    DLOG("OFFICE online %d\n",next_io_service_);
#endif
    if(next_io_service_ >= CLIENT_POOL) {
        next_io_service_=0;
    }

    acceptor_.async_accept(c->socket(),boost::bind(&office::handle_accept, this, c, boost::asio::placeholders::error));
}
void office::handle_accept(client_ptr c, const boost::system::error_code& error) {
    //uint32_t now=time(NULL);
    if(!run) {
        return;
    }
#ifdef DEBUG
    DLOG("OFFICE new ticket (total open:%ld)\n",clients_.size());
#endif
    if(clients_.size()>=MAXCLIENTS || srv_.do_sync || message.length()>MESSAGE_TOO_LONG) {
        ELOG("OFFICE busy, delaying connection\n");
    }

    while(clients_.size()>=MAXCLIENTS || srv_.do_sync || message.length()>MESSAGE_TOO_LONG) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }

    if (!error) {
        try {
            c->start();
            join(c);
        } catch (std::exception& e) {
            DLOG("Client exception: %s\n",e.what());
        }
    }
    start_accept(); //FIXME, change this to a non blocking office
}

void office::join(client_ptr c) {
    std::lock_guard<std::mutex> lock(mx_client_);
    clients_.insert(c);
}

void office::leave(client_ptr c) {
    std::lock_guard<std::mutex> lock(mx_client_);
    clients_.erase(c);
}

// server <-> office
void server::ofip_update_block(uint32_t period_start,uint32_t now,message_map& commit_msgs,uint32_t newdiv) {
    ofip->update_block(period_start,now,commit_msgs,newdiv);
}
void server::ofip_process_log(uint32_t now) {
    ofip->process_log(now);
}
void server::ofip_init(uint32_t myusers) {
    ofip->init(myusers);
}
void server::ofip_start() {
    ofip->start();
}
bool server::ofip_get_msg(uint32_t msid,std::string& line) {
    return(ofip->get_msg(msid,line));
}
void server::ofip_del_msg(uint32_t msid) {
    ofip->del_msg(msid);
}
void server::ofip_gup_push(gup_t& g) {
    ofip->gup.push(g);
}
void server::ofip_add_remote_deposit(uint32_t user,int64_t weight) {
    ofip->add_remote_deposit(user,weight);
}
void server::ofip_add_remote_user(uint16_t abank,uint32_t auser,uint8_t* pkey) {
    ofip->add_remote_user(abank,auser,pkey);
}
void server::ofip_delete_user(uint32_t auser) {
    ofip->delete_user(auser);
}
void server::ofip_change_pkey(uint8_t* pkey) {
    memcpy(ofip->pkey,pkey,32);
}
void server::ofip_readwrite() {
    ofip->readonly=false;
}
void server::ofip_readonly() {
    ofip->readonly=true;
}
bool server::ofip_isreadonly() {
    return(ofip->readonly);
}

// server <-> peer
/*void server::peer_set(std::set<peer_ptr>& peers) {
    peer_.lock();
    peers=peers_;
    peer_.unlock();
}*/
/*void server::peer_clean() { //LOCK: peer_ missing_ mtx_
    std::set<peer_ptr> remove;
    std::set<uint16_t> svids;
    static uint32_t last_block=0;
    peer_.lock();
    if (!do_sync && last_block< srvs_.now && time(NULL)>srvs_.now+BLOCKSEC*0.75) {
        for (auto pi=peers_.begin();pi!=peers_.end();pi++) {
            if(!(*pi)->do_sync && (*pi)->last_active<srvs_.now) {
                DLOG("DISCONNECT IDLE PEER %04X\n",(*pi)->svid);
                (*pi)->killme=true;
            }
        }
        last_block=srvs_.now;
    }
    for(auto pi=peers_.begin(); pi!=peers_.end(); pi++) {
        if((*pi)->killme) {
            remove.insert(*pi);
            //peers_.erase(*pi);
            remove.insert(*pi);
            peers_.erase(*pi);
        } else {
            svids.insert((*pi)->svid);
        }
    }
    peer_.unlock();
    for(auto pi=remove.begin(); pi!=remove.end(); pi++) {
        DLOG("KILL PEER %04X\n",(*pi)->svid);
        if(svids.find((*pi)->svid)==svids.end()) {
            missing_sent_remove((*pi)->svid);
        }
        (*pi)->stop();
    }
    if(svids.empty() && !opts_.init && !do_sync) {
        ELOG("ERROR: no peers, panic\n");
        panic=true;
        if(!ofip_isreadonly()) {
            ELOG("ERROR: no peers, set office readonly\n");
            ofip_readonly();
        }
    }
}*/
/*void server::peer_killall() {
    std::set<peer_ptr> peers;
    peer_set(peers);
    DLOG("KILL ALL PEERS\n");
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        (*pi)->stop();
    }

    //m_peerManager.stop();
}*/
/*void server::disconnect(uint16_t svid) {
    std::set<peer_ptr> peers;
    peer_set(peers);
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        if((*pi)->svid==svid) {
            DLOG("DISCONNECT PEER %04X\n",(*pi)->svid);
            (*pi)->killme=true;
        }
    }
}*/
/*const char* server::peers_list() {
    std::set<peer_ptr> peers;
    static std::string list;
    list="";
    peer_set(peers);
    if(!peers.size()) {
        return("");
    }
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        list+=",";
        if((*pi)->svid!=BANK_MAX) {
            list+=std::to_string((*pi)->svid);
        } else {
            list+="-";
        }
        if((*pi)->killme) {
            list+="*";
        }
    }
    return(list.c_str()+1);
}*/
/*void server::peers_known(std::set<uint16_t>& list) {
    std::set<peer_ptr> peers;
    peer_set(peers);
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        if((*pi)->svid && (*pi)->svid<BANK_MAX){
            list.insert((*pi)->svid);
        }
    }
}*/
/*void server::connected(std::vector<uint16_t>& list) {
    std::set<peer_ptr> peers;
    peer_set(peers);
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        if((*pi)->svid && !((*pi)->killme) && !((*pi)->do_sync)) {
            list.push_back((*pi)->svid);
        }
    }
}*/
/*int server::duplicate(peer_ptr p) {
    std::set<peer_ptr> peers;
    peer_set(peers);
    for(auto r : peers) {
        if(r!=p && r->svid==p->svid) {
            return 1;
        }
    }
    return 0;
}*/
void server::get_more_headers(uint32_t now) {
    /*std::set<peer_ptr> peers;
    peer_set(peers);
    auto pi=peers.begin();
    if(!peers.size()) {
        return;
    }
    if(peers.size()>1) {
        int64_t num=((uint64_t)random())%peers.size();
        advance(pi,num);
    }
    if(!(*pi)->do_sync) {
        DLOG("REQUEST more headers from peer %04X\n",(*pi)->svid);
        (*pi)->request_next_headers(now);
    } //LOCK: peer::sio_*/

    m_peerManager.getMoreHeaders(now);
}
void server::fillknown(message_ptr msg) {
    /*static uint32_t r=0;
    std::vector<uint16_t> peers;
    connected(peers);
    uint32_t n=peers.size();
    for(uint32_t i=0; i<n; i++) {
        msg->know_insert(peers[(r+i)%n]);
    }
    r++;*/

    m_peerManager.fillknown(msg);
}
void server::peerready(std::set<uint16_t>& ready) { //get list of peers available for data download
    /*std::set<peer_ptr> peers;
    peer_set(peers);
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        if((*pi)->svid && !((*pi)->killme) && !((*pi)->do_sync) && !((*pi)->busy)) {
            ready.insert((*pi)->svid);
        }
    }*/

    m_peerManager.getReadyPeers(ready);
}
int server::deliver(message_ptr msg,uint16_t svid) {
    /*std::set<peer_ptr> peers;
    peer_set(peers);
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        if((*pi)->svid==svid) {
            (*pi)->deliver(msg);
            return(1);
        }
    }*/

    m_peerManager.deliver(msg, svid);
    //msg->sent_erase(svid);
    return(1);
}
void server::deliver(message_ptr msg) {
    m_peerManager.deliverToAll(msg);
    /*std::set<peer_ptr> peers;
    peer_set(peers);
    std::for_each(peers.begin(),peers.end(),boost::bind(&peer::deliver, _1, msg));*/
}
/*int server::update(message_ptr msg,uint16_t svid) {
    std::set<peer_ptr> peers;
    peer_set(peers);
    for(auto pi=peers.begin(); pi!=peers.end(); pi++) {
        if((*pi)->svid==svid) {
            (*pi)->update(msg);
            return(1);
        }
    }
    msg->sent_erase(svid);
    return(0);
}*/
void server::update(message_ptr msg) {
    //std::set<peer_ptr> peers;
    //peer_set(peers);
    //std::for_each(peers.begin(),peers.end(),boost::bind(&peer::update, _1, msg));
    m_peerManager.updateAll(msg);
}
void server::start_accept() {
    //peer_ptr new_peer(new peer(*this,true,srvs_,opts_));
   //acceptor_.async_accept(new_peer->socket(),boost::bind(&server::peer_accept,this,new_peer,boost::asio::placeholders::error));
    m_peerManager.startAccept();

}
/*void server::peer_accept(peer_ptr new_peer,const boost::system::error_code& error) {
    uint32_t now=time(NULL);
    if (now>=srvs_.now+BLOCKSEC) {
        DLOG("WARNING: dropping peer connection while creating new block\n");
        new_peer->stop();
    } else {m_peers
        if (!error) {
            peer_.lock();
            peers_.insert(new_peer);
            peer_.unlock();
            new_peer->accept();
        } else {
            new_peer->stop();
        }
    }
    new_peer.reset();
    start_accept();
}*/
/*void server::connect(boost::asio::ip::tcp::resolver::iterator& iterator) {
    try {
        DLOG("TRY connecting to address %s:%d\n",
             iterator->endpoint().address().to_string().c_str(),iterator->endpoint().port());
        peer_ptr new_peer(new peer(*this,false,srvs_,opts_, m_peerManager));
        peer_.lock();
        peers_.insert(new_peer);
        peer_.unlock();
        //new_peer->killme=true; // leave little time for the connection
        //boost::asio::async_connect(new_peer->socket(),iterator,
        //                           boost::bind(&peer::connect,new_peer,boost::asio::placeholders::error));
        new_peer->tryAsyncConnect(iterator, 15);
    } catch (std::exception& e) {
        DLOG("Connection: %s\n",e.what());
    }
}*/
/*void server::connect(std::string peer_address) {
    try {
        DLOG("TRY connecting to address %s\n",peer_address.c_str());
        const char* port=SERVER_PORT;
        char* p=strchr((char*)peer_address.c_str(),'/'); //separate peer id
        if(p!=NULL) {
            peer_address[p-peer_address.c_str()]='\0';
        }
        p=strchr((char*)peer_address.c_str(),':'); //detect port
        if(p!=NULL) {
            peer_address[p-peer_address.c_str()]='\0';
            port=p+1;
        }
        boost::asio::ip::tcp::resolver resolver(io_service_);
        boost::asio::ip::tcp::resolver::query query(peer_address.c_str(),port);
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
        peer_ptr new_peer(new peer(*this,false,srvs_,opts_, m_peerManager));
        peer_.lock();
        peers_.insert(new_peer);
        peer_.unlock();
        //new_peer->killme=true; // leave little time for the connection
        //boost::asio::async_connect(new_peer->socket(),iterator,
        //                           boost::bind(&peer::connect,new_peer,boost::asio::placeholders::error));
        new_peer->tryAsyncConnect(iterator, 15);
    } catch (std::exception& e) {
        DLOG("Connection: %s\n",e.what());
    }
}*/
/*void server::connect(uint16_t svid) {
    try {
        DLOG("TRY connecting to peer %04X\n",svid);
        //char ipv4t[32];
        //sprintf(ipv4t,"%s",inet_ntoa(srvs_.nodes[svid].ipv4));
        struct in_addr addr;
        addr.s_addr=srvs_.nodes[svid].ipv4;
        char portt[32];
        sprintf(portt,"%u",srvs_.nodes[svid].port);
        boost::asio::ip::tcp::resolver resolver(io_service_);
        boost::asio::ip::tcp::resolver::query query(inet_ntoa(addr),portt);
        //boost::asio::ip::tcp::endpoint              connectpoint{ boost::asio::ip::address::from_string("127.0.0.1"), 8291};
        //boost::asio::ip::tcp::resolver::query query(boost::asio::ip::address::from_string("127.0.0.1"), 8291);
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
        peer_ptr new_peer(new peer(*this,false,srvs_,opts_, m_peerManager));
        peer_.lock();
        peers_.insert(new_peer);
        peer_.unlock();
        //new_peer->killme=true; // leave little time for the connection
        //boost::asio::async_connect(new_peer->socket(),iterator,
        //                           boost::bind(&peer::connect,new_peer,boost::asio::placeholders::error));

        new_peer->tryAsyncConnect(iterator, 15);

    } catch (std::exception& e) {
        DLOG("Connection: %s\n",e.what());
    }
}*/
