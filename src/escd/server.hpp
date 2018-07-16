#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <fcntl.h>
#include <algorithm>
#include "servers.hpp"
#include "options.hpp"
#include "candidate.hpp"
#include "helper/hlog.h"
#include "helper/blocks.h"
#include "network/peerclientmanager.h"

class office;
class peer;

typedef std::shared_ptr<peer> peer_ptr;

class server {
  public:
    server(options& opts);
    ~server();
    void start();
    void stop();
    void recyclemsid(uint32_t lastpath);
    void del_msglog(uint32_t now,uint16_t svid,uint32_t msid);
    void put_msglog(uint32_t now,uint16_t svid,uint32_t msid,std::map<uint64_t,log_t>& log); //message log, by server

    //update_nodehash is similar
    int undo_bank(bool commit); //will undo database changes and check if the database is consistant
    void load_banks();
    uint64_t need_bank(uint16_t bank); //FIXME, return 0 if not at this stage
    void have_bank(uint64_t hnum);
    void load_chain();
    void msgl_process(servers& header,uint8_t* data);
    void get_last_header(servers& sync_ls,handshake_t& sync_hs);
    void add_next_header(uint32_t from,servers& peer_ls);
    void add_headers(std::vector<servers>& peer_headers);
    int fast_sync(bool done,header_t& head,node_t* nods,svsi_t* svsi);
    uint32_t readmsid();

    //FIXME, move this to servers.hpp
    void writemsid();
    void clean_last_svid_msgs(std::map<uint16_t,message_ptr>& map);// remove !!!
    void message_shash(uint8_t* mhash,message_map& map);
    void LAST_block_msgs();
    void LAST_block_final(hash_s& cand);
    void signlater();
    void count_votes(uint32_t now,hash_s& cand);
    void add_electors(header_t& head,svsi_t* peer_svsi);

    //FIXME, using blk_msgs_ as elector set is unsafe, VIP nodes should be used
    void prepare_poll();
    message_ptr message_svidmsid(uint16_t svid,uint32_t msid);
    std::string print_missing_verbose();
    message_ptr message_find(message_ptr msg,uint16_t svid);
    void double_spend(message_ptr msg);
    void create_double_spend_proof(message_ptr msg1,message_ptr msg2);
    bool known_dbl(uint16_t svid);
    int check_dbl(boost::mutex& mlock_,message_map& msgs,message_map::iterator it);
    void bad_insert(message_ptr msg);
    void bad_recover(message_ptr msg);
    int message_insert(message_ptr msg);
    int dbl_insert(message_ptr msg); // WARNING !!! it deletes old message data if len==message::header_length
    int cnd_insert(message_ptr msg); // WARNING !!! it deletes old message data if len==message::header_length
    int blk_insert(message_ptr msg); // WARNING !!! it deletes old message data if len==message::header_length
    int txs_insert(message_ptr msg); // WARNING !!! it deletes old message data if len==message::header_length
    void cnd_validate(message_ptr msg); //FIXME, check timing !!!
    void blk_validate(message_ptr msg); // WARNING, this is executed by peer io_service
    void missing_sent_remove(uint16_t svid); //TODO change name to missing_know_send_remove()
    void validator(void);
    uint64_t make_ppi(uint16_t tmpos, uint32_t omsid,uint32_t amsid,uint16_t abank,uint16_t bbank);
    uint16_t ppi_abank(const uint64_t& ppi);
    uint16_t ppi_bbank(const uint64_t& ppi);
    uint64_t ppi_txid(const uint64_t& ppi);
    bool undo_message(message_ptr msg);//FIXME, this is single threaded, remove locks
    void log_broadcast(uint32_t path,char* p,int len,uint8_t* hash,uint8_t* pkey,uint32_t msid,uint32_t mpos);
    bool process_message(message_ptr msg);
    int open_bank(uint16_t svid);
    uint8_t bitcount(std::vector<uint8_t>& bitvotes,uint8_t min);
    void commit_block(std::set<uint16_t>& update);//assume single thread
    int64_t dividend(user_t& u);
    int64_t dividend(user_t& u,int64_t& fee);
    void commit_dividends(std::set<uint16_t>& update, uint64_t &myput_fee); //assume single thread, TODO change later
    void commit_deposit(std::set<uint16_t>& update, uint64_t &myput_fee); //assume single thread, TODO change later !!!
    void commit_bankfee(uint64_t myput_fee);
    bool accept_message();
    void update_list(std::vector<uint64_t>& txs, std::vector<uint64_t>& dbl, std::vector<uint64_t>& blk, uint16_t peer_svid);
    void finish_block();
    message_ptr write_handshake(uint16_t peer,handshake_t& hs);
#ifdef DOUBLE_SPEND
    uint32_t write_dblspend(std::string line);
#endif
    uint32_t write_message(std::string line); // assume single threaded
    void write_candidate(const hash_s& cand_hash);
    candidate_ptr save_candidate(uint32_t blk,const hash_s& h,std::map<uint64_t,hash_s>& add,std::set<uint64_t>& del,uint16_t peer);
    candidate_ptr known_candidate(const hash_s& h,uint16_t peer);
    void update_candidates(message_ptr msg);
    bool known_elector(uint16_t svid);
    void write_header();
    void clock();
    bool break_silence(uint32_t now,std::string& message,uint32_t& tnum);// will be obsolete if we start tolerating empty blocks
    void missing_msgs_erase(message_ptr& msg);
    void missing_msgs_insert(message_ptr& msg);
    int check_msgs_size();
    uint32_t srvs_now();
    servers& getBlockInPorgress();
    servers& getLastClosedBlock();
    uint16_t getRandomNodeIndx();
    uint16_t getMaxNodeIndx();
    bool getNode(uint16_t nodeId, node& nodeInfo);
    void ofip_gup_push(gup_t& g);
    void ofip_add_remote_deposit(uint32_t user,int64_t weight);
    void ofip_init(uint32_t myusers);
    void ofip_start();
    bool ofip_get_msg(uint32_t msid,std::string& line);
    void ofip_del_msg(uint32_t msid);
    void ofip_update_block(uint32_t period_start,uint32_t now,message_map& commit_msgs,uint32_t newdiv);
    void ofip_process_log(uint32_t now);
    uint32_t ofip_add_remote_user(uint16_t abank,uint32_t auser,uint8_t* pkey);
    void ofip_delete_user(uint32_t user);
    void ofip_change_pkey(uint8_t* user);
    void ofip_readwrite();
    void ofip_readonly();
    bool ofip_isreadonly();

    //FIXME, move this to servers.hpp
    //std::set<uint16_t> last_svid_dbl; //list of double spend servers in last block
    std::map<uint16_t,message_ptr> LAST_block_svid_msgs;
    message_map LAST_block_final_msgs;
    message_map LAST_block_all_msgs;
    uint32_t LAST_block;
    //std::map<uint16_t,message_ptr> svid_msgs_; //last validated txs message or dbl message from server
    //boost::mutex svid_;
    //FIXME, use serv_.now instead
    servers last_srvs_;
    //message_ptr block; // my block message, now data in last_srvs_
    int do_sync;
    int do_fast;
    int do_check;
    //int get_headers; //TODO :-( reduce the number of flags
    uint32_t msid_; // change name to msid :-)
    //int vip_max; // user last_srvs_.vtot;
    std::list<servers> headers; //FIXME, make this private
    uint32_t get_msglist; //block id of the requested msglist of messages
    office* ofip;
    // uint8_t *pkey; //used by office/client to create BKY transaction
    hash_t pkey; // can not be a pointer to servers.nodes[] because add_node runs push_back() on a vector
    uint32_t start_path;
    uint32_t start_msid;
  private:
    hash_t skey;
    //enum { max_connections = 4 }; //unused
    //enum { max_recent_msgs = 1024 }; // this is the block chain :->

    servers srvs_;
    options& opts_;

    boost::thread_group threadpool;

    PeerConnectManager m_peerManager; //responsible for managing peers
    boost::thread* clock_thread;

    //uint8_t lasthash[SHA256_DIGEST_LENGTH]; // hash of last block, this should go to path/servers.txt
    //uint8_t prevhash[SHA256_DIGEST_LENGTH]; // hash of previous block, this should go to path/servers.txt
    int do_validate; // keep validation threads running
    //uint32_t maxnow; // do not process messages if time >= maxnow
    candidate_ptr winner; // elected candidate
    std::set<message_ptr> busy_msgs_; // messages in validation
    std::set<peer_ptr> peers_;
    std::map<hash_s,candidate_ptr,hash_cmp> candidates_; // list of candidates, TODO should be map of message_ptr
    message_queue wait_msgs_;
    message_queue check_msgs_;
    message_queue sign_msgs_;
    std::map<hash_s,message_ptr,hash_cmp> bad_msgs_;
    message_map missing_msgs_; //TODO, start using this, these are messages we still wait for
    message_map txs_msgs_; //_TXS messages (transactions)
    message_map ldc_msgs_; //_TXS messages (transactions in sync mode)
    message_map cnd_msgs_; //_CND messages (block candidates)
    message_map blk_msgs_; //_BLK messages (blocks) or messages in a block when syncing
    message_map dbl_msgs_; //_DBL messages (double spend)

    std::map<uint16_t,nodekey_t> nkeys;
    std::map<uint16_t,uint64_t> electors;
    uint64_t votes_max;
    int do_vote;
    int do_block;
    std::map<uint64_t,int64_t> deposit;
    std::map<uint64_t,uint16_t> blk_bky; //create new bank
    std::map<uint64_t,uint32_t> blk_bnk; //create new bank
    std::map<uint64_t,get_t> blk_get; //set lock / withdraw
    std::map<uint64_t,usr_t> blk_usr; //remote account request
    std::map<uint64_t,uok_t> blk_uok; //remote account accept
    std::map<uint64_t,uint32_t> blk_sbs; //set node status bits
    std::map<uint64_t,uint32_t> blk_ubs; //clear node status bits
    std::vector<int64_t> bank_fee;
    int64_t mydiv_fee; // just to record the local TXS_DIV_FEE bank income
    int64_t myusr_fee; // just to record the local TXS_DIV_FEE bank income
    int64_t myget_fee; // just to record the local TXS_DIV_FEE bank income
    uint32_t period_start; //start time of this period
    hash_t msha_;
    std::set<uint16_t> dbl_srvs_; //list of detected double servers
    bool iamvip;
    bool block_only;
    bool panic;

    boost::mutex cand_;
    boost::mutex wait_;
    boost::mutex check_;
    boost::mutex bad_;
    boost::mutex missing_;
    boost::mutex txs_;
    boost::mutex ldc_;
    boost::mutex cnd_;
    boost::mutex blk_;
    boost::mutex dbls_;
    boost::mutex dbl_;
    boost::mutex deposit_;
    boost::mutex headers_;
    boost::mutex peer_;
};

#endif // SERVER_HPP
