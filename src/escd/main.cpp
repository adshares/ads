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
#include <openssl/rand.h>
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
boost::recursive_mutex flog;
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

    //init rando seed for rand functions
    srand (time(NULL));

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
                opt.comm=true;
                opt.genesis="";
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
            join(c);
            c->start();            
        } catch (std::exception& e) {            
            DLOG("Client exception: %s\n",e.what());
            leave(c);
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
uint32_t server::ofip_add_remote_user(uint16_t abank,uint32_t auser,uint8_t* pkey) {
    return ofip->add_remote_user(abank,auser,pkey);
}
void server::ofip_delete_user(uint32_t auser) {
    ofip->delete_user(auser);
}
void server::ofip_change_pkey(uint8_t* pkey) {
    memcpy(ofip->pkey,pkey,32);
}
void server::ofip_readwrite() {
    DLOG("OFFICE SET READWRITE\n");
    std::cout << "OFFICE SET READWRITE\n";
    ofip->readonly=false;
}
void server::ofip_readonly() {
    DLOG("OFFICE SET READONLY\n");
    ofip->readonly=true;
}
bool server::ofip_isreadonly() {
    return(ofip->readonly);
}

