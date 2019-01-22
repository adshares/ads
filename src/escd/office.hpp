#ifndef OFFICE_HPP
#define OFFICE_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>
#include <stack>
#include <deque>
#include <map>
#include <set>
#include <boost/make_shared.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "helper/blocks.h"
#include "message.hpp"


class client;
class server;
class options;
typedef boost::shared_ptr<client> client_ptr;

//before optimising remember that the required througput here is much lower than the throughput of the server
//consider running the processing in a single thread

class office {
  public:
    office(options& opts, server& srv);
    ~office();
    void iorun_client(int i);
    void iorun();
    void stop();
    void init(uint32_t myusers);
    void start();
    void update_div(uint32_t now,uint32_t newdiv);
    void process_div(uint32_t path);
    void update_block(uint32_t period_start,uint32_t now,message_map& commit_msgs,uint32_t newdiv);
    void process_log(uint32_t now);
    void resolve_hostnames();
    void clock();
    void process_gup(uint32_t now);
    void process_dep(uint32_t now);
    bool get_user_global(user_t& u,uint16_t cbank,uint32_t cuser);
    bool get_user(user_t& u,uint16_t cbank,uint32_t cuser);
    uint32_t add_remote_user(uint16_t bbank,uint32_t buser,uint8_t* pkey);
    uint32_t add_user(uint16_t abank, uint8_t* pk, uint32_t when);
    void set_user(uint32_t user, user_t& nu, int64_t deduct);
    void delete_user(uint32_t user);
    void add_remote_deposit(uint32_t buser,int64_t tmass);
    void add_deposit(uint32_t buser,int64_t tmass);
    void add_deposit(usertxs& utxs);
    bool try_account(hash_s* key);
    void add_account(hash_s* key,uint32_t user);
    bool get_msg(uint32_t msid,std::string& line);
    void del_msg(uint32_t msid);
    bool set_account_status(uint32_t buser, uint16_t status);
    bool unset_account_status(uint32_t buser, uint16_t status);
    bool add_msg(IBlockCommand& utxs, uint32_t& msid, uint32_t& mpos);
    bool add_msg(uint8_t* msg, uint32_t len, uint32_t& msid, uint32_t& mpos);
    bool add_msg(uint8_t* msg, usertxs& utxs, uint32_t& msid, uint32_t& mpos);
    bool lock_user(uint32_t cuser);
    void unlock_user(uint32_t cuser);
    void start_accept();
    void handle_accept(client_ptr c,const boost::system::error_code& error);
    void join(client_ptr c);
    void leave(client_ptr c);
    bool check_user(uint16_t peer,uint32_t uid);
    bool find_key(uint8_t* pkey,uint8_t* skey);
    uint32_t last_path();
    uint32_t last_nodes();
    uint32_t last_users(uint32_t bank);
    void mklogdir(uint16_t svid);
#ifdef FALLOC_FL_COLLAPSE_RANGE
    int purge_log(int fd,uint32_t /*user*/);
#else
    int purge_log(int fd,uint32_t user);
#endif

#ifdef NOLOG
    void mklogfile(uint16_t svid,uint32_t user);
    void put_log(uint32_t user,log_t& log);
    void put_log(std::map<uint64_t,log_t>& log,uint32_t ntime);
#else
    void mklogfile(uint16_t svid,uint32_t user);
    void put_log(uint32_t user,log_t& log);
    void put_log(std::map<uint64_t,log_t>& log,uint32_t ntime);
#endif

    void put_ulog(uint32_t user,log_t& log);
    void put_ulog(std::map<uint64_t,log_t>& log);
    bool fix_log(uint16_t svid,uint32_t user);
    bool get_log(uint16_t svid,uint32_t user,uint32_t from,std::string& slog);
    uint8_t* node_pkey(uint16_t node);

    int get_tickets();
    uint16_t svid;
    hash_t pkey; // local copy for managing updates
    std::stack<gup_t> gup; // GET results
    bool readonly;
    std::vector<boost::asio::ip::address> allow_from;
    std::vector<boost::asio::ip::tcp::endpoint> redirect_read;
    std::vector<boost::asio::ip::tcp::endpoint> redirect_write;
    std::vector<boost::asio::ip::address> redirect_read_exclude;
    std::vector<boost::asio::ip::address> redirect_write_exclude;
  private:
    bool run;
    char ofifilename[64];
    const user_t user_tmp= {};
    uint32_t div_ready;
    uint32_t block_ready;
    uint32_t users; //number of users of the bank
    std::string message;
    boost::asio::ip::tcp::endpoint endpoint_;
    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    boost::asio::ip::tcp::acceptor acceptor_;
    server& srv_;
    options& opts_;
    std::set<client_ptr> clients_;
    int offifd_; // user file descriptor
    uint32_t message_sent;
    uint32_t message_tnum; // last transaction number
    std::map<hash_s,uint32_t,hash_cmp> accounts_; // list of candidates
    //io_service_pool
    typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
    typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;
    io_service_ptr        io_services_[CLIENT_POOL];
    work_ptr              io_works_[CLIENT_POOL];
    boost::thread_group   threadpool;
    int next_io_service_;
    std::stack<dep_t> rdep; // users with remote deposits
    std::deque<uint64_t> mque; // list of message to process log
    std::deque<uint32_t> deleted_users; // list of accounts to reuse ... this list could be limited (swapped)
    boost::thread ioth_;
    boost::thread clock_thread;
    std::set<uint32_t> users_lock;
    boost::mutex file_; //LOCK: server::
    boost::mutex log_; //FIXME, maybe no need for this lock, lock too long
    std::mutex  users_; //LOCK: file_, log_
    std::mutex  mx_client_;
    std::mutex  mx_account_;
};

#endif // OFFICE_HPP
