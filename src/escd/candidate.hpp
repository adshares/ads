#ifndef CANDIDATE_HPP
#define CANDIDATE_HPP

class candidate :
  public boost::enable_shared_from_this<candidate>
{
public:
  //hash_t shash; // hash of signature hashes, this is the key to this container
  uint32_t now;
  uint16_t peer; // source of , probably not used
  uint64_t score; // 
  bool failed; // candidate will not be accepted during voting
  std::map<uint64_t,hash_s> msg_add; // add to list
  std::set<uint64_t> msg_del; // remove from list
  std::set<uint64_t> msg_mis; // still waiting
//bool failed_peer; // candidate will not be accepted
//std::map<uint16_t,msidhash_t> svid_have; // hashes missed by peer (processed first)
//std::map<uint16_t,msidhash_t> svid_miss; // hashes missed by server (processed second)
//std::set<uint16_t> waiting_server; //hashes still missing by server during block building
  std::set<message_ptr> votes; // messages with votes, probably not used, but could be used for reporting
  std::set<uint16_t> peers; // used by save_candidate

  candidate() :
	now(0),
	peer(0),
	score(0),
	failed(false)
  {
  }

  candidate(uint32_t blk,std::map<uint64_t,hash_s>& add,std::set<uint64_t>& del,std::set<uint64_t>& mis,uint16_t svid,bool myfailed) :
	now(blk),
	peer(svid),
	score(0),
	failed(myfailed),
	msg_add(add),
	msg_del(del),
	msg_mis(mis)
  { 
  }

  bool accept()
  { if(!failed && msg_mis.size()==0){
      return true;}
    return false;
  }

  bool elected_accept()
  { if(msg_mis.size()==0){
      return true;}
    return false;
  }

  void update(message_ptr msg)
  { msg_.lock(); //just in case
    auto mis=msg_mis.find(msg->hash.num & 0xFFFFFFFFFFFF0000L);
    if(mis!=msg_mis.end()){
      auto add=msg_add.find(msg->hash.num & 0xFFFFFFFFFFFF0000L);
      assert(add!=msg_add.end());
      if(!memcmp(add->second.hash,msg->sigh,32)){
        msg_mis.erase(mis);}}
    msg_.unlock();
  }

  const char* print_missing(servers* srvs) //TODO, consider having local lock
  { static std::string line;
    if(failed){
      line="FAILED MISSING: ";}
    else{
      line="MISSING: ";}
    msg_.lock();
    for(auto key : msg_mis){
      char miss[64];
      uint32_t msid=(key>>16) & 0xFFFFFFFFL;
      uint16_t svid=(key>>48);
      sprintf(miss," %04X:%08X",svid,msid);
      line+=miss;}
    msg_.unlock();
    return(line.c_str());
  }

  void get_missing(std::vector<uint64_t>& missing)
  { msg_.lock();
    for(auto key : msg_mis){
      missing.push_back(key);}
    msg_.unlock();
  }

  void del_missing(uint64_t mis)
  { msg_.lock();
    msg_mis.erase(mis);
    msg_.unlock();
  }

private:
  boost::mutex msg_;

};
typedef boost::shared_ptr<candidate> candidate_ptr;

#endif // CANDIDATE_HPP
