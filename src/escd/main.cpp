#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <future>
#include <sys/types.h>
#include <unistd.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/mutex.hpp>
#include "default.hpp"
#include "options.hpp"
#include "candidate.hpp"
#include "server.hpp"
#include "office.hpp"

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

    auto workdir = settings::get_workdir(argc, argv, true);
    if(workdir != ".") {
        settings::change_working_dir(workdir);
    }

    options opt;
    opt.get(argc,argv);

    stdlog=fopen("log.txt","w");
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

