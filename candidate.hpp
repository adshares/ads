#ifndef CANDIDATE_HPP
#define CANDIDATE_HPP

class candidate :
  public boost::enable_shared_from_this<candidate>
{
public:
  //hash_t shash; // hash of signature hashes, this is the key to this container
  uint16_t peer; // source of , probably not used
  //std::set<uint16_t> missed_peer; // hashes missed by peer
  std::map<uint16_t,msidhash_t> svid_miss; // hashes missed by server
  std::map<uint16_t,msidhash_t> svid_have; // hashes missed by peer
  uint32_t score; // 
  std::set<message_ptr> votes; // messages with votes, probably not used, but could be used for reporting
  std::set<uint16_t> peers; // used by save_candidate
  std::set<uint16_t> waiting_server; //hashes still missing by server during block building
  bool failed_peer; // candidate will not be accepted i

  candidate() :
	peer(0),
	score(0),
	failed_peer(false)
  {
  }

  candidate(std::map<uint16_t,msidhash_t>& miss,std::map<uint16_t,msidhash_t>& have,uint16_t svid,bool failed) :
	peer(svid),
	score(0),
	failed_peer(failed)
  {
    svid_miss=miss;
    svid_have=have;
    for(auto it=svid_miss.begin();it!=svid_miss.end();it++){
      waiting_server.insert(it->first);}
  }

  bool accept()
  { if(!failed_peer && waiting_server.size()==0){
      return true;}
    return false;
  }

private:
};
typedef boost::shared_ptr<candidate> candidate_ptr;

#endif // CANDIDATE_HPP
