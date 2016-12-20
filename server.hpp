#ifndef SERVER_HPP
#define SERVER_HPP

#include "main.hpp"
 
class peer;
typedef boost::shared_ptr<peer> peer_ptr;

class server
{
public:
  server(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint,options& opts) :
    io_service_(io_service),
    acceptor_(io_service, endpoint),
    opts_(opts),
    votes_max(0.0),
    do_validate(0),
    do_vote(0),
    do_block(0),
    do_fast(1),
    do_sync(1)
  { uint32_t path=readmsid(); // reads msid_ and path
    last_srvs_.get(path);
    if(last_srvs_.nodes.size()<=opts_.svid){ 
      std::cerr << "ERROR: reading servers\n";
      exit(-1);} 
    if(memcmp(last_srvs_.nodes[opts_.svid].pk,opts_.pk,32)){ // move this to get servers
      char pktext[2*32+1]; pktext[2*32]='\0';
      ed25519_key2text(pktext,last_srvs_.nodes[opts_.svid].pk,32);
      std::cerr << "ERROR: server_public_key mismatch: " << pktext << "\n";
      exit(-1);}
    //TODO, check vok and vno, if bad, inform user and suggest an older starting point

    uint32_t now=time(NULL);
    now-=now%BLOCKSEC;
    if(opts_.init){
      last_srvs_.init(now-BLOCKSEC);}
    srvs_=last_srvs_;
    srvs_.now+=BLOCKSEC;
    srvs_.blockdir();
    vip_max=srvs_.update_vip(); //based on initial weights at start time

    if(last_srvs_.now==now-BLOCKSEC){ // maybe we should run sync anyway (unless we are the only vip)
      //assume database correct, allow very fast start, consider adding a startpoint option
      do_sync=0;}
    else{
      if(last_srvs_.now<now-MAXLOSS && !opts_.fast){
        std::cerr<<"WARNING, possibly missing too much history for full resync\n";}
      do_sync=1;}

    //FIXME, move this to a separate thread that will keep a minimum number of connections
    for(std::string addr : opts_.peer){
      //std::cerr<<"CONNECT :"<<addr<<"\n";
      connect(addr);
      boost::this_thread::sleep(boost::posix_time::seconds(2));} //wait some time before connecting to more peers

    if(do_sync){
      if(opts_.fast){ //FIXME, do slow sync after fast sync
        while(do_fast){ // fast_sync changes the status, FIXME use future/promis
          boost::this_thread::sleep(boost::posix_time::seconds(1));}}
      else{
        do_fast=0;}
      std::cerr<<"START syncing headers\n";
      load_chain();} // sets do_sync=0;

    clock_thread = new boost::thread(boost::bind(&server::clock, this));
    start_accept(); // should consider sending a 'hallo world' message
  }
  ~server()
  { 
    //threadpool.interrupt_all();
    do_validate=0;
    threadpool.join_all();

    clock_thread->interrupt();
    clock_thread->join();
  }

  void load_chain()
  { char pathname[16];
    uint32_t now=time(NULL);
    now-=now%BLOCKSEC;
    do_validate=1;
    threadpool.create_thread(boost::bind(&server::validator, this));
    threadpool.create_thread(boost::bind(&server::validator, this));
//FIXME, must start with a matching nowhash and load serv_
    for(auto block=headers.begin();srvs_.now<now;){
      if(block==headers.end()){ // wait for peers to load more blocks
        std::cerr<<"WAITING for headers "<<block->now<<"\n";
        boost::this_thread::sleep(boost::posix_time::seconds(2));
        continue;}
      std::cerr<<"START syncing header "<<block->now<<"\n";
      if(srvs_.now!=block->now){
        std::cerr<<"ERROR, got strange block numbers "<<srvs_.now<<"<>"<<block->now<<"\n";
        exit(-1);} //FIXME, prevent this
      //srvs_.blockdir();
      //block->load_signatures(); //TODO should go through signatures and update vok, vno
      block->header_put(); //FIXME will loose relation to signatures, change signature filename to fix this
      if(!block->load_txslist(missing_msgs_,opts_.svid)){
        //request list of transactions from peers
        peer_.lock(); // consider changing this to missing_lock
        get_txslist=srvs_.now;
        peer_.unlock();
        //prepare txslist request message
        message_ptr put_msg(new message());
        put_msg->data[0]=MSGTYPE_TXL;
        memcpy(put_msg->data+1,&block->now,4);
        put_msg->got=0; // do first request emidiately
        while(get_txslist){ // consider using future/promise
          uint32_t now=time(NULL);
          if(put_msg->got<now-MAX_MSGWAIT){
            fillknown(put_msg); // do this again in case we have a new peer, FIXME, let the peer do this
            uint16_t svid=put_msg->request();
            if(svid){
              std::cerr << "REQUESTING TXL from "<<svid<<"\n";
              deliver(put_msg,svid);}}
          boost::this_thread::sleep(boost::posix_time::seconds(1));}
        srvs_.txs=block->txs; //check
        srvs_.txs_put(missing_msgs_);}
      //inform peers about current sync block
      message_ptr put_msg(new message());
      put_msg->data[0]=MSGTYPE_PAT;
      memcpy(put_msg->data+1,&srvs_.now,4);
      deliver(put_msg);
      //request missing messages from peers
      txs_.lock();
      txs_msgs_.clear();
      txs_.unlock();
      dbl_.lock();
      dbl_msgs_.clear();
      dbl_.unlock();
      blk_.lock();
      blk_msgs_.clear();
      blk_.unlock();
      missing_.lock();
      for(auto it=missing_msgs_.begin();it!=missing_msgs_.end();){
        missing_.unlock();
	auto jt=it++;
        blk_.lock();
        blk_msgs_[jt->first]=jt->second; //overload the use of blk_msgs_ , during sync store here the list of messages to be validated (meybe we should use a different container for ths later)
        blk_.unlock();
	if(jt->second->hash.dat[0]==MSGTYPE_DBL){
          dbl_.lock();
          dbl_msgs_[jt->first]=jt->second;
          dbl_.unlock();}
        else{
          txs_.lock();
          txs_msgs_[jt->first]=jt->second;
          txs_.unlock();}
	if(jt->second->load() && !jt->second->sigh_check()){
          check_.lock();
          check_msgs_.push_back(jt->second); // send to validator
          check_.unlock();
          missing_msgs_erase(jt->second);
          continue;} // assume no lock needed
	fillknown(jt->second);
	jt->second->request(); //FIXME, maybe request only if this is the next needed message, need to have serv_ ... ready for this check :-/
        missing_.lock();}
      missing_.unlock();
      //wait for all messages to be processed by the validators
      blk_.lock();
      while(blk_msgs_.size()){
        blk_.unlock();
        boost::this_thread::sleep(boost::posix_time::seconds(1)); //yes, yes, use futur/promise instead
	blk_.lock();}
      blk_.unlock();
      //finish block
      srvs_.finish(); //FIXME, add locking
      if(memcmp(srvs_.nowhash,block->nowhash,SHA256_DIGEST_LENGTH)){
        std::cerr<<"ERROR, failed to arrive at correct hash at block "<<srvs_.now<<", fatal\n";
        exit(-1);}
      last_srvs_=srvs_; // consider not making copies of nodes
      srvs_.now+=BLOCKSEC;
      srvs_.blockdir();
      now=time(NULL);
      now-=now%BLOCKSEC;
      block++;}
    //TODO, add nodes if needed
    vip_max=srvs_.update_vip();
    txs_.lock();
    txs_msgs_.clear();
    txs_.unlock();
    dbl_.lock();
    dbl_msgs_.clear();
    dbl_.unlock();
    //FIXME, inform peers about sync status
    peer_.lock();
    do_sync=0;
    headers.clear();
    peer_.unlock();
    message_ptr put_msg(new message());
    put_msg->data[0]=MSGTYPE_SOK;
    memcpy(put_msg->data+1,&srvs_.now,4);
    deliver(put_msg);
  }

  void put_txslist(uint32_t now,std::map<uint64_t,message_ptr>& map)
  { missing_.lock(); // consider changing this to missing_lock
    if(get_txslist!=now){
      missing_.unlock();
      return;}
    missing_msgs_.swap(map);
    get_txslist=0;
    missing_.unlock();
    return;
  }

  void add_headers(std::vector<servers>& peer_headers)
  { if(!do_sync){
      return;}
    peer_.lock();
    if(!headers.size()){
      headers.insert(headers.end(),peer_headers.begin(),peer_headers.end());
      peer_.unlock();
      return;}
    auto it=peer_headers.begin();
    for(;it!=peer_headers.end() && it->now<=headers.back().now;it++){}
    if(headers.back().now!=peer_headers.begin()->now-BLOCKSEC){
      std::cerr<<"ERROR, headers misaligned\n"; //should never happen
      peer_.unlock();
      return;}
    headers.insert(headers.end(),it,peer_headers.end());
    peer_.unlock();
  }

  int fast_sync(bool done,header_t& head,node_t* nods,svsi_t* svsi)
  { static uint32_t last=0;
    for(;;){
      uint32_t now=time(NULL);
      peer_.lock();
      if(!do_fast){
        peer_.unlock();
        return(-1);}
      if(done){ // peer should now overwrite servers with current data
	std::cerr<<"SYNC overwrite\n";
        last_srvs_.overwrite(head,nods);
	std::cerr<<"SYNC mkdir\n";
        last_srvs_.blockdir();
	std::cerr<<"SYNC put\n";
	last_srvs_.put();
	std::cerr<<"SYNC put signatures\n";
	last_srvs_.put_signatures(head,svsi);
	std::cerr<<"SYNC copy\n";
        srvs_=last_srvs_; //FIXME, create a copy function
        srvs_.now+=BLOCKSEC;
	std::cerr<<"SYNC blockdir\n";
        srvs_.blockdir();
	std::cerr<<"SYNC update vip\n";
        vip_max=srvs_.update_vip(); //based on final weights
        do_fast=0;
        peer_.unlock();
        return(1);}
      if(last<now-SYNC_WAIT){
        last=now;
        peer_.unlock();
        return(1);}
      peer_.unlock();
      boost::this_thread::sleep(boost::posix_time::seconds(1));}
  }

  void join(peer_ptr participant);
  void leave(peer_ptr participant);
  int duplicate(peer_ptr participant);
  void deliver(message_ptr msg);
  void deliver(message_ptr msg,uint16_t svid);
  void update(message_ptr msg);
  void svid_msid_rollback(message_ptr msg);
  void start_accept();
  void handle_accept(peer_ptr new_peer,const boost::system::error_code& error);
  void connect(std::string peer_address);
  void fillknown(message_ptr msg);

  //FIXME, move this to servers.hpp
  uint32_t readmsid()
  { if(opts_.init){
      msid_=0;
      return(0);}
    std::ifstream myfile("msid.txt");
    if(!myfile.is_open()){
      msid_=0;
      return(0);}
    std::string line1;
    std::string line2;
    getline (myfile,line1);
    getline (myfile,line2);
    myfile.close();
    msid_=atoi(line1.c_str());
    return(atoi(line2.c_str()));
  }

  //FIXME, move this to servers.hpp
  void writemsid()
  { std::ofstream myfile("msid.txt");
    if(!myfile.is_open()){
      throw("FATAL ERROR: failed to write to msid.txt \n");}
    myfile << std::to_string(msid_) << "\n" << std::to_string(last_srvs_.now) << "\n";
    myfile.close();
  }

  //move this to message
  void message_msha(std::map<uint16_t,message_ptr>& map)
  { svid_msha.clear();
    for(std::map<uint16_t,message_ptr>::iterator it=map.begin();it!=map.end();++it){
      msidhash_t msha;
      msha.msid=it->second->msid;
      memcpy(msha.sigh,it->second->sigh,sizeof(hash_t));
      svid_msha[it->first]=msha;}
  }

  //move this to message
  void message_shash(uint8_t* mhash,std::map<uint16_t,message_ptr>& map)
  { SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for(std::map<uint16_t,message_ptr>::iterator it=map.begin();it!=map.end();++it){
      char sigh[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(sigh,it->second->sigh,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"SHASH: %d:%d %.*s\n",(int)(it->first),(int)(it->second->msid),2*SHA256_DIGEST_LENGTH,sigh);
      // do not hash messages from ds_server
      SHA256_Update(&sha256,it->second->sigh,4*sizeof(uint64_t));}
    SHA256_Final(mhash, &sha256);							std::cerr << "message_shash\n";
  }

  void count_votes(uint32_t now,hash_s& cand) // cand_.locked()
  { candidate_ptr num1=NULL;
    candidate_ptr num2=NULL;
    float votes_counted=0;
    hash_s best;
    cand_.lock();
    for(auto it=candidates_.begin();it!=candidates_.end();it++){ // cand_ is locked
      if(num1==NULL || it->second->score>num1->score){
        num2=num1;
        memcpy(&best,&it->first,sizeof(hash_s));
        num1=it->second;}
      else if(num2==NULL || it->second->score>num2->score){
        num2=it->second;}
      votes_counted+=it->second->score;}
    cand_.unlock();
    if(num1==NULL){
      if(do_vote && now>srvs_.now+BLOCKSEC+(do_vote-1)*VOTE_DELAY){
        std::cerr << "CANDIDATE proposing\n";
        write_candidate(cand);}
      return;}
    if(do_block<2 && num1->score>(num2!=NULL?num2->score:0)+(votes_max-votes_counted)){
      float x=(num2!=NULL?num2->score:0);
      std::cerr << "BLOCK elected: " << num1->score << " second:" << x << " max:" << votes_max << " counted:" << votes_counted << "\n";
      do_block=2;
      winner=num1;
      if(winner->failed_peer){
        std::cerr << "BAD BLOCK elected :-( must resync :-( \n"; // FIXME, do not exit, initiate sync
        exit(-1);}}
    if(do_block==2 && winner->accept()){
      std::cerr << "CANDIDATE winner accepted\n";
      do_block=3;
      if(do_vote){
        write_candidate(best);}
      return;}
    if(do_vote && num1->accept() && num1->peers.size()>1){
      std::cerr << "CANDIDATE proposal accepted\n";
      write_candidate(best);
      return;}
    if(do_vote && now>srvs_.now+BLOCKSEC+(do_vote-1)*VOTE_DELAY){
      std::cerr << "CANDIDATE proposing\n";
      write_candidate(cand);}
  }

  void prepare_poll() // select CANDIDATE_MAX candidates and VOTES_MAX electors
  { 
    cand_.lock();
    votes_max=0.0;
    do_vote=0;
    //FIXME, this should be moved to servers.hpp
    std::vector<uint16_t> svid_rank;
    for(auto it=last_svid_msgs.begin();it!=last_svid_msgs.end();++it){
      if(srvs_.nodes[it->second->svid].status & SERVER_DBL){
        continue;}
      std::cerr << "ELECTOR accepted : " << it->second->svid << "(" << it->second->msid << ")\n";
      svid_rank.push_back(it->second->svid);}
    if(!svid_rank.size()){
      std::cerr << "ERROR, no valid server for this block :-(, TODO create own block\n";}
    else{
      std::cerr << "SORT \n";
      std::sort(svid_rank.begin(),svid_rank.end(),[this](const uint16_t& i,const uint16_t& j){return(this->srvs_.nodes[i].weight>this->srvs_.nodes[j].weight);});} //fuck, lambda :-/
    //TODO, save this list
    for(int j=0;j<VOTES_MAX && j<svid_rank.size();j++){
      if(svid_rank[j]==opts_.svid){
        do_vote=1+j;}
      std::cerr << "ELECTOR[" << svid_rank[j] << "]=" << srvs_.nodes[svid_rank[j]].weight << "\n";
      electors[svid_rank[j]]=srvs_.nodes[svid_rank[j]].weight;
      votes_max+=srvs_.nodes[svid_rank[j]].weight;}
    winner=NULL;
    std::cerr << "ELECTOR max:" << votes_max << "\n";
    cand_.unlock();
  }

  message_ptr message_svidmsid(uint16_t svid,uint32_t msid)
  { 
    union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0; // hash
    h.dat[1]=0; // message type
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    fprintf(stderr,"HASH find:%0.16llX (%04X:%08X) %d:%d\n",h.num,svid,msid,svid,msid);
    txs_.lock();
    auto mi=txs_msgs_.lower_bound(h.num);
    while(mi!=txs_msgs_.end() && mi->second->svid==svid && mi->second->msid==msid){
      if(mi->second->status==MSGSTAT_VAL){
        txs_.unlock();
        return mi->second;}
      mi++;}
    txs_.unlock();
    return NULL;
  }

  message_ptr message_find(message_ptr msg,uint16_t svid)
  { fprintf(stderr,"HASH find:%0.16llX (%04X%08X) %d:%d\n",msg->hash.num,msg->svid,msg->msid,msg->svid,msg->msid);
    if(msg->data[0]==MSGTYPE_GET){
      txs_.lock();
      std::map<uint64_t,message_ptr>::iterator it=txs_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=txs_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          txs_.unlock();
          return it->second;}
        it++;}
      txs_.unlock();
      return NULL;}
    if(msg->data[0]==MSGTYPE_CNG){
      cnd_.lock();
      std::map<uint64_t,message_ptr>::iterator it=cnd_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=cnd_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          cnd_.unlock();
          return it->second;}
        it++;}
      cnd_.unlock();
fprintf(stderr,"HASH find failed, CND db:\n");
for(auto me=cnd_msgs_.begin();me!=cnd_msgs_.end();me++){ fprintf(stderr,"HASH have: %0.16llX (%02X)\n",me->first,me->second->hashval(svid));} //data[4+(svid%64)]
      return NULL;}
    if(msg->data[0]==MSGTYPE_BLG){
      blk_.lock();
      std::map<uint64_t,message_ptr>::iterator it=blk_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=blk_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          blk_.unlock();
          return it->second;}
        it++;}
      blk_.unlock();
      return NULL;}
    if(msg->data[0]==MSGTYPE_DBG){
      dbl_.lock();
      std::map<uint64_t,message_ptr>::iterator it=dbl_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=dbl_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64){
          dbl_.unlock();
          return it->second;}
        it++;}
      dbl_.unlock();
      return NULL;}
    fprintf(stderr,"UNKNOWN hashtype:%d %02X\n",(uint32_t)msg->data[0],(uint32_t)msg->data[0]);
    return NULL;
  }

  void double_spend(message_ptr msg)
  {
    std::cerr << "WARNING, double spend maybe not yet fully implemented\n";
    svid_.lock();
    srvs_.nodes[msg->svid].status|=SERVER_DBL;
    srvs_.nodes[msg->svid].msid=msg->msid; //FIXME, this should be maybe(!) msid from last block + 1
    svid_msgs_[msg->svid]=msg;
    svid_.unlock();
    if(!do_sync){
      update(msg);}
    // undo transactions later
  }

  void create_double_spend_proof(message_ptr msg1,message_ptr msg2)
  { try{
      assert(!do_sync); // should never happen, should never get same msid from same server in a txs_list
      uint32_t len=4+msg1->len+msg2->len;
      assert(msg1->svid==msg2->svid);
      assert(msg1->msid==msg2->msid);
      if(msg1->svid==opts_.svid){
        std::cerr << "FATAL, created own double spend !!!\n";
        exit(-1);}
      message_ptr dbl_msg(new message(len));
      if(msg1->data==NULL){
        msg1->load();}
      if(msg2->data==NULL){
        msg2->load();}
      dbl_msg->data[0]=MSGTYPE_DBL;
      memcpy(dbl_msg->data+1,&len,3);
//FIXME, include previous hash in this double spend proof
// preh !!!
      memcpy(dbl_msg->data+4,msg1->data,msg1->len);
      memcpy(dbl_msg->data+4+msg1->len,msg2->data,msg2->len);
      dbl_msg->msid=0xffffffff;
      dbl_msg->svid=msg1->svid;
      dbl_msg->now=time(NULL);
      dbl_msg->peer=opts_.svid;
      dbl_msg->hash.num=dbl_msg->dohash();
      dbl_msg->hash_signature(NULL); //FIXME, set this to last hash from last block
      dbl_.lock();
      dbl_msgs_[dbl_msg->hash.num]=dbl_msg;
      dbl_.unlock();
      double_spend(dbl_msg);}
    catch(std::out_of_range& e){
      return;}
  }

  int message_insert(message_ptr msg)
  { if(msg->hash.dat[1]==MSGTYPE_TXS){
      return(txs_insert(msg));}
    if(msg->hash.dat[1]==MSGTYPE_CND){
      return(cnd_insert(msg));}
    if(msg->hash.dat[1]==MSGTYPE_BLK){
      return(blk_insert(msg));}
    if(msg->hash.dat[1]==MSGTYPE_DBL){
      return(dbl_insert(msg));}
    std::cerr << "ERROR, getting unexpected message type :-( \n";
    return(-1);
  }

  int dbl_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  {
    assert(msg->hash.dat[1]==MSGTYPE_DBL);
    dbl_.lock(); // maybe no lock needed
    fprintf(stderr,"HASH insert:%0.16llX (DBL)\n",msg->hash.num);
    auto it=dbl_msgs_.find(msg->hash.num);
    if(it!=dbl_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
        if(do_sync && memcmp(osg->sigh,msg->sigh,SHA256_DIGEST_LENGTH)){
          dbl_.unlock();
          std::cerr << "ERROR, getting message with wrong signature hash\n";
          return(0);}
        osg->update(msg);
        osg->path=srvs_.now;
        dbl_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){
          std::cerr << "ERROR, message save failed, abort server\n";
          exit(-1);}
        double_spend(osg);
        return(1);}
      else{ // update info about peer inventory
        dbl_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      dbl_msgs_[msg->hash.num]=msg;
      dbl_.unlock();
      missing_msgs_insert(msg);
      return(1);}
    if(msg->svid==opts_.svid){ // own message
      dbl_msgs_[msg->hash.num]=msg;
      dbl_.unlock();
      assert(msg->peer==msg->svid);
      std::cerr << "DEBUG, storing own dbl message :-( [???]\n";
      msg->save();
      return(1);}
    dbl_.unlock();
    std::cerr << "ERROR, getting unexpected dbl message\n";
    return(-1);
  }

  int cnd_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_CND);
    cnd_.lock();
    fprintf(stderr,"HASH insert:%0.16llX (CND)\n",msg->hash.num);
    std::map<uint64_t,message_ptr>::iterator it=cnd_msgs_.find(msg->hash.num);
    if(it!=cnd_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
	message_ptr pre=NULL,nxt=NULL;
        osg->update(msg);
        osg->path=osg->msid; // this is the block time!!!
        cnd_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){ //FIXME, do not exit !!! return fail
          std::cerr << "ERROR, message save failed, abort server\n";
          return(-1);}
        if(it!=cnd_msgs_.begin()){
          pre=(--it)->second;
          it++;}
        if((++it)!=cnd_msgs_.end()){
          nxt=it->second;}
        if(pre!=NULL && pre->len>message::header_length && (pre->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(pre,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        if(nxt!=NULL && nxt->len>message::header_length && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(nxt,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        std::cerr << "DEBUG, storing cnd message\n";
        cnd_validate(osg);
        return(1);}
      else{ // update info about peer inventory
        cnd_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      cnd_msgs_[msg->hash.num]=msg;
      cnd_.unlock();
      missing_msgs_insert(msg);
      return(1);}
    if(msg->svid==opts_.svid){ // own message
      cnd_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      cnd_.unlock();
      assert(msg->peer==msg->svid);
      std::cerr << "DEBUG, storing own cnd message\n";
      msg->save();
      cnd_validate(msg);
      return(1);}
    cnd_.unlock();
    std::cerr << "ERROR, getting unexpected cnd message\n";
    return(-1);
  }

  int blk_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_BLK);
    blk_.lock();
    fprintf(stderr,"HASH insert:%0.16llX (BLK)\n",msg->hash.num);
    std::map<uint64_t,message_ptr>::iterator it=blk_msgs_.find(msg->hash.num);
    if(it!=blk_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
	message_ptr pre=NULL,nxt=NULL;
        osg->update(msg);
        osg->path=osg->msid; // this is the block time!!!
        blk_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){ //FIXME, save where ???, check legal time !!!
          std::cerr << "ERROR, message save failed, abort server\n";
          return(-1);}
        if(it!=blk_msgs_.begin()){
          pre=(--it)->second;
          it++;}
        if((++it)!=blk_msgs_.end()){
          nxt=it->second;}
        if(pre!=NULL && pre->len>message::header_length && (pre->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(pre,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        if(nxt!=NULL && nxt->len>message::header_length && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(nxt,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        std::cerr << "DEBUG, storing blk message\n";
        blk_validate(osg);
        return(1);}
      else{ // update info about peer inventory
        blk_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      blk_msgs_[msg->hash.num]=msg; //insert block message
      blk_.unlock();
      missing_msgs_insert(msg);
      return(1);}
    if(msg->svid==opts_.svid){ // own message
      blk_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      blk_.unlock();
      assert(msg->peer==msg->svid);
      std::cerr << "DEBUG, storing own blk message\n";
      msg->save(); //FIXME, time !!!
      blk_validate(msg);
      return(1);}
    if(msg->svid==msg->peer){ // peers message
      blk_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      blk_.unlock();
      std::cerr << "DEBUG, storing peer's blk message\n";
      msg->save(); //FIXME, time !!!
      blk_validate(msg);
      return(1);}
    blk_.unlock();
    std::cerr << "ERROR, getting unexpected blk message\n";
    return(-1);
  }

  int txs_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_TXS);
    txs_.lock(); // maybe no lock needed
    fprintf(stderr,"HASH insert:%0.16llX (TXS)\n",msg->hash.num);
    std::map<uint64_t,message_ptr>::iterator it=txs_msgs_.find(msg->hash.num);
    if(it!=txs_msgs_.end()){
      message_ptr osg=it->second;
      // overwrite message status with the current one
      // not needed any more msg->status=osg->status; // for peer.hpp to check if message is already validated
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
        if(do_sync && memcmp(osg->sigh,msg->sigh,SHA256_DIGEST_LENGTH)){
          txs_.unlock();
          std::cerr << "ERROR, getting message with wrong signature hash\n";
          return(0);}
        osg->update(msg);
        osg->path=srvs_.now;
        txs_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){ //FIXME, change path
          std::cerr << "ERROR, message save failed, abort server\n";
          exit(-1);}
        // process double spend
        //if(osg->hash.dat[1]==MSGTYPE_DBL){ // double spend proof
        //  double_spend(osg);
        //  return(1);}
        // check for double spend
        if(!do_sync){
          message_ptr pre=NULL,nxt=NULL; //probably not needed when syncing
          if(it!=txs_msgs_.begin()){
            pre=(--it)->second;
            it++;}
          if((++it)!=txs_msgs_.end()){
            nxt=it->second;}
          if(pre!=NULL && pre->len>message::header_length && (pre->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
            create_double_spend_proof(pre,osg); // should copy messages from this server to ds_msgs_
            return(1);}
          if(nxt!=NULL && nxt->len>message::header_length && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
            create_double_spend_proof(nxt,osg); // should copy messages from this server to ds_msgs_
            return(1);}
          if(osg->now>=srvs_.now+BLOCKSEC){
            wait_.lock();
            wait_msgs_.push_back(osg);
            wait_.unlock();
            return(1);}}//FIXME, process wait messages later
        // process ordinary messages
        check_.lock();
        check_msgs_.push_back(osg);
        check_.unlock();
        return(1);}
      else{ // update info about peer inventory
        txs_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    else{
      if(msg->len==message::header_length){
        txs_msgs_[msg->hash.num]=msg;
        txs_.unlock();
        missing_msgs_insert(msg);
        return(1);}
      if(msg->svid==opts_.svid){ // own message
        txs_msgs_[msg->hash.num]=msg;
        msg->path=srvs_.now;
        txs_.unlock();
        msg->save();
        check_.lock();
        check_msgs_.push_back(msg); // running though validator
        check_.unlock();
	assert(msg->peer==msg->svid);
        std::cerr << "DEBUG, storing own message\n";
        return(1);}
      txs_.unlock();
      std::cerr << "ERROR, getting unexpected message\n";
      return(-1);}
  }

  void cnd_validate(message_ptr msg)
  {
    std::cerr << "CANDIDATE test\n";
    msg->status=MSGSTAT_VAL; // all can vote
    cand_.lock();
    if(electors.find(msg->svid)==electors.end()){ //FIXME, electors should have assigned rank when building poll
      std::cerr << "BAD ELECTOR "<< msg->svid <<" :-( \n";
      cand_.unlock();
      return;}
    if(!electors[msg->svid]){
      cand_.unlock();
      return;}
    hash_s cand; //TODO
    memcpy(cand.hash,msg->data+message::data_offset,sizeof(hash_t));
    auto it=candidates_.find(cand);
    if(it==candidates_.end()){
      std::cerr << "BAD CANDIDATE :-( \n";
      cand_.unlock();
      return;}
    it->second->score+=electors[msg->svid]; // update sum of weighted votes
    std::cerr << "CANDIDATE score: "<< it->second->score <<"(+"<< electors[msg->svid] <<")\n";
    electors[msg->svid]=0;
    cand_.unlock();
  }

  void blk_validate(message_ptr msg)
  {

//FIXME, check what block are we dealing with !!!

    std::cerr << "BLOCK test\n";
    if(msg->msid!=last_srvs_.now){
      fprintf(stderr,"BLOCK bad msid:%08x block:%08x\n",msg->msid,last_srvs_.now);
      return;}
    msg->status=MSGSTAT_VAL; // all can vote
    uint32_t vip=last_srvs_.nodes[msg->svid].status & SERVER_VIP;
    if(!vip){
      fprintf(stderr,"BLOCK ignore non-vip vote msid:%08x svid:%04x\n",msg->msid,(uint32_t)msg->svid);
      return;}
    header_t* h=(header_t*)(msg->data+4+64+10); 
    bool no=memcmp(h->nowhash,last_srvs_.nowhash,sizeof(hash_t));
    blk_.lock();
    last_srvs_.save_signature(msg->svid,msg->data+4,!no);
    blk_.unlock();
    fprintf(stderr,"BLOCK: yes:%d no:%d max:%d\n",last_srvs_.vok,last_srvs_.vno,vip_max);
    update(msg); // update others if this is a VIP message, my message was sent already, but second check will not harm
    if(last_srvs_.vno>vip_max/2){
      std::cerr << "BAD BLOCK consensus :-( must resync :-( \n"; // FIXME, do not exit, initiate sync
      exit(-1);}
  }

  void validator(void)
  {
    while(do_validate){
      check_.lock(); //TODO this should be a lock for check_msgs_ only maybe
      if(check_msgs_.empty()){
        check_.unlock();
        //look for missing messages
        //std::cerr << "VALIDATOR housekeeping\n";
        uint32_t now=time(NULL);
	message_queue tmp_msgs_;
	missing_.lock();
	for(auto mi=missing_msgs_.begin();mi!=missing_msgs_.end();mi++){
          //FIXME, request _BLK messages if BLOCK ready 
          if(mi->second->hash.dat[1]==MSGTYPE_BLK){ //FIXME, consider checking mi->first
            if(now>mi->second->got+MAX_MSGWAIT && mi->second->msid<=last_srvs_.now){
              tmp_msgs_.push_back(mi->second);}}
          else{
            if(now>mi->second->got+MAX_MSGWAIT && srvs_.nodes[mi->second->svid].msid==mi->second->msid-1){
              tmp_msgs_.push_back(mi->second);}}}
	missing_.unlock();
        for(auto re=tmp_msgs_.begin();re!=tmp_msgs_.end();re++){
	  uint16_t svid=(*re)->request();
          if(svid){
            fprintf(stderr,"HASH request:%0.16llX [%0.16llX] (%0.4X) %d:%d\n",(*re)->hash.num,*((uint64_t*)(*re)->data),svid,(*re)->svid,(*re)->msid); // could be bad allignment
            std::cerr << "REQUESTING MESSAGE from "<<svid<<" ("<<(*re)->svid<<":"<<(*re)->msid<<")\n";
            deliver((*re),svid);}}
        //checking waiting messages
        tmp_msgs_.clear();
        wait_.lock();
	for(auto wa=wait_msgs_.begin();wa!=wait_msgs_.end();){
          if((*wa)->now<srvs_.now+BLOCKSEC && srvs_.nodes[(*wa)->svid].msid==(*wa)->msid-1){
            std::cerr << "QUEUING MESSAGE "<<(*wa)->svid<<":"<<(*wa)->msid<<"\n";
            tmp_msgs_.push_back(*wa);
            wa=wait_msgs_.erase(wa);}
          else{
            wa++;}}
        wait_.unlock();
	check_.lock();
        check_msgs_.insert(check_msgs_.end(),tmp_msgs_.begin(),tmp_msgs_.end());
        check_.unlock();
        //TODO, check if there are no forgotten messeges in the missing_msgs_ queue
	boost::this_thread::sleep(boost::posix_time::milliseconds(500));} // adds latency but works
      else{
        message_ptr msg=check_msgs_.front();
	check_msgs_.pop_front();
        check_.unlock();
        if(srvs_.nodes[msg->svid].status & SERVER_DBL){ // ignore messages from DBL server
          continue;} // no update
        //if(msg->status==MSGSTAT_VAL || srvs_.nodes[msg->svid].msid>=msg->msid)
        if(srvs_.nodes[msg->svid].msid>=msg->msid){
          std::cerr <<"WARNING ignoring validation of old message "<<msg->svid<<":"<<msg->msid<<"<="<<srvs_.nodes[msg->svid].msid<<"\n";
          continue;}
	if(srvs_.nodes[msg->svid].msid!=msg->msid-1){
          std::cerr <<"WARNING postponing validation of future message "<<msg->svid<<":"<<msg->msid<<"!="<<srvs_.nodes[msg->svid].msid<<"+1\n";
          wait_.lock();
          wait_msgs_.push_back(msg);
          wait_.unlock();
          continue;}
        //if not valid, continue
        boost::this_thread::sleep(boost::posix_time::seconds(10.0*((float)random()/(float)RAND_MAX))); //FIXME, this is the validation :-D
        msg->print_text(";VALID");
        msg->status=MSGSTAT_VAL;
        svid_.lock();
        srvs_.nodes[msg->svid].msid=msg->msid; 
        svid_msgs_[msg->svid]=msg;
        svid_.unlock();
        if(!do_sync){
          update_candidates(msg);
          update(msg);}
        else{
          blk_.lock();
          blk_msgs_.erase(msg->hash.num);
          blk_.unlock();}}}
  }

  void update_list(std::vector<uint64_t>& txs,std::vector<uint64_t>& dbl,uint16_t peer_svid)
  { txs_.lock();
    for(auto me=txs_msgs_.begin();me!=txs_msgs_.end();me++){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      if(me->second->status==MSGSTAT_VAL){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_PUT;
        h.dat[1]=me->second->hashval(peer_svid); //data[4+(peer_svid%64)]
        txs.push_back(h.num);}}
    txs_.unlock();
    dbl_.lock();
    for(auto me=dbl_msgs_.begin();me!=dbl_msgs_.end();me++){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      if(me->second->status==MSGSTAT_VAL){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_DBP;
        h.dat[1]=0;//me->second->data[4+(peer_svid%64)];
        dbl.push_back(h.num);}}
    dbl_.unlock();
  }

  void finish_block()
  { 
    txs_.lock();
    std::map<uint16_t,message_ptr> last_block_svid_msgs=last_svid_msgs; // last block latest validated message from server, should change this to now_svid_msgs
    for(auto it=winner->svid_have.begin();it!=winner->svid_have.end();it++){
      //last_block_svid_msgs.erase(*it);
      uint16_t svid=it->first;
      uint32_t msid=it->second.msid;
      if(!msid){
        fprintf(stderr,"WARNING msid==0 for svid:%04X\n",(uint32_t)svid);
        continue;}
      uint8_t* sigh=it->second.sigh;
      union {uint64_t num; uint8_t dat[8];} h;
      h.dat[0]=0; // hash
      h.dat[1]=0; // message type
      memcpy(h.dat+2,&msid,4);
      memcpy(h.dat+6,&svid,2);
//FIXME, include dblspend message search
      auto mi=txs_msgs_.lower_bound(h.num);
      while(1){
        if((mi==txs_msgs_.end()) || ((mi->first & 0xFFFFFFFFFFFF0000L)!=(h.num))){
          fprintf(stderr,"FATAL, could not find svid_have message :-( %0.16llX\n",h.num);
          exit(-1);}
        if(!memcmp(sigh,mi->second->sigh,sizeof(hash_t))){
          last_block_svid_msgs[it->first]=mi->second;
          break;}
        mi++;}}
    for(auto it=winner->svid_miss.begin();it!=winner->svid_miss.end();it++){
      uint16_t svid=it->first;
      uint32_t msid=it->second.msid;
      if(!msid){
        fprintf(stderr,"WARNING msid==0 for svid:%04X\n",(uint32_t)svid);
        continue;}
      uint8_t* sigh=it->second.sigh;
      union {uint64_t num; uint8_t dat[8];} h;
      h.dat[0]=0; // hash
      h.dat[1]=0; // message type
      memcpy(h.dat+2,&msid,4);
      memcpy(h.dat+6,&svid,2);
//FIXME, include dblspend message search
      auto mi=txs_msgs_.lower_bound(h.num);
      while(1){
        if((mi==txs_msgs_.end()) || ((mi->first & 0xFFFFFFFFFFFF0000L)!=(h.num))){
          fprintf(stderr,"FATAL, could not find svid_miss message :-( %0.16llX\n",h.num);
          exit(-1);}
        if(!memcmp(sigh,mi->second->sigh,sizeof(hash_t))){
          last_block_svid_msgs[it->first]=mi->second;
          break;}
        mi++;}}
    txs_.unlock();

    // save list of final message hashes
    char filename[64];
    sprintf(filename,"%08X/delta.txt",srvs_.now); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    FILE *fp=fopen(filename,"w");
    char* hash=(char*)malloc(2*sizeof(hash_t));
    for(auto it=last_block_svid_msgs.begin();it!=last_block_svid_msgs.end();it++){
      node* nod=&srvs_.nodes[it->first];
      //node* nn=&last_srvs_.nodes[it->first];
      //nn->status=no->status;
      //nn->ipv4=no->ipv4;
      //nn->port=no->port;
      nod->msid=it->second->msid;
      nod->mtim=it->second->now;
      memcpy(nod->msha,it->second->sigh,sizeof(hash_t));
      ed25519_key2text(hash,it->second->sigh,sizeof(hash_t));
      fprintf(stderr,"DELTA: %d %d %.*s\n",it->first,it->second->msid,2*sizeof(hash_t),hash);
      fprintf(fp,"%d %d %.*s\n",it->first,it->second->msid,2*sizeof(hash_t),hash);}
    // confirm hash;
    hash_s last_block_message;
    message_shash(last_block_message.hash,last_block_svid_msgs);
    cand_.lock();
    auto ca=candidates_.find(last_block_message);
    if(ca==candidates_.end() || memcmp(last_block_message.hash,ca->first.hash,sizeof(hash_t))){
      std::cerr << "FATAL, failed to confirm block hash\n";
      cand_.unlock();
      exit(-1);}
    cand_.unlock();
    ed25519_key2text(hash,last_block_message.hash,sizeof(hash_t));
    fprintf(stderr,"DELTA: 0 0 %.*s\n",2*sizeof(hash_t),hash);
    fprintf(fp,"0 0 %.*s\n",2*sizeof(hash_t),hash);
    fclose(fp); //TODO consider updating peers about correct block msgs hash

    std::stack<message_ptr> remove_msgs;
    std::stack<message_ptr> invalidate_msgs;
    message_queue commit_msgs; //std::forward_list<message_ptr> commit_msgs;
    std::map<uint64_t,message_ptr> last_block_all_msgs; // last block all validated message from server, should change this to now_svid_msgs
    sprintf(filename,"%08X/block.txt",srvs_.now); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    fp=fopen(filename,"w");
    uint32_t txcount=0;
    uint16_t nsvid=0; // current svid processed
    //uint32_t nmsid=0; // current msid to be processed
    //uint32_t emsid=0; // not needed !
    uint32_t minmsid=0; // first msid to be processed
    uint32_t maxmsid=0; // last msid to be proceesed
    bool remove=false;
    txs_.lock();
    for(auto mj=txs_msgs_.begin();mj!=txs_msgs_.end();){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      auto mi=mj++;
      if(mi->second->path>0 && mi->second->path<srvs_.now){
        std::cerr << "ERASE message " << mi->second->svid << ":" << mi->second->msid << "\n";
        txs_msgs_.erase(mi); // forget old messages
        continue;}
      if(nsvid!=mi->second->svid){
        remove=0;
        nsvid=mi->second->svid;
        //emsid=mi->second->msid;
        if(last_srvs_.nodes.size()<nsvid){
          minmsid=1;}
	else{
          minmsid=last_srvs_.nodes[nsvid].msid+1;}
        maxmsid=srvs_.nodes[nsvid].msid;
        fprintf(stderr,"RANGE %d %d %d\n",nsvid,minmsid,maxmsid);
        fprintf(fp,"RANGE %d %d %d\n",nsvid,minmsid,maxmsid);}
      //if(nmsid==0xffffffff && mi->second->msid<0xffffffff)  // remove messages from dbl_spend server
      if(maxmsid==0xffffffff && mi->second->msid<0xffffffff){ // remove messages from dbl_spend server
        std::cerr << "REMOVE message " << nsvid << ":" << mi->second->msid << " later ! (DBL-spend server)\n";
        remove_msgs.push(mi->second);
        continue;}
      //if(mi->second->msid>nmsid)
      if(mi->second->msid>maxmsid){
        if(mi->second->now<last_srvs_.now){ // remove messages if failed to be included in 2 blocks
          //if(srvs_.nodes[mi->second->svid].msid>nmsid){
	  //  srvs_.nodes[mi->second->svid].msid=nmsid;} // reset status 
          //if(srvs_.nodes[mi->second->svid].msid>maxmsid){
	  //  srvs_.nodes[mi->second->svid].msid=maxmsid;} // reset status 
          if(!remove){
            svid_msid_rollback(mi->second);}
          remove=true;}
        if(remove){
          std::cerr << "REMOVE message " << nsvid << ":" << mi->second->msid << " later !\n";
          remove_msgs.push(mi->second);}
        else{
          std::cerr << "MOVING message " << nsvid << ":" << mi->second->msid << " to next block\n";
          if(mi->second->status&MSGSTAT_VAL){
            std::cerr << "INVALIDATE message " << nsvid << ":" << mi->second->msid << " later !\n";
            invalidate_msgs.push(mi->second);}
          mi->second->move(srvs_.now+BLOCKSEC);}}
      else{
        if(mi->second->len==message::header_length){
          fprintf(stderr,"IGNOR: %d %d %.16s %0.16llX (%0.16llX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
//???, what about emsid ???
          continue;}
        //assert(mi->second->path==srvs_.now); // check, maybe path is not assigned yet
	if(mi->second->path!=srvs_.now){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          fprintf(stderr,"PATH?: %d %d %.16s %0.16llX (%0.16llX) path=%08X srvs_.now=%08X\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first,mi->second->path,srvs_.now);}
        //if(nmsid!=0xffffffff && mi->second->msid!=emsid)
        if(maxmsid!=0xffffffff && mi->second->msid!=minmsid){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          fprintf(stderr,"?????: %d %d %.16s %0.16llX (%0.16llX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
          //std::cerr << "ERROR, failed to read correct transaction order in block " << nsvid << ":" << mi->second->msid << "<>" << emsid << " (" << nmsid << ")\n";
          std::cerr << "ERROR, failed to read correct transaction order in block " << nsvid << ":" << mi->second->msid << "<>" << minmsid << " (max:" << maxmsid << ")\n";
          for(auto mx=txs_msgs_.begin();mx!=txs_msgs_.end();mx++){
            fprintf(stderr,"%0.16llX\n",mx->first);}
          exit(-1);}
        if(!(mi->second->status & MSGSTAT_VAL)){
          std::cerr << "ERROR, invalid transaction in block\n";
          exit(-1);}
        txcount++;
        //emsid++;
        minmsid++;
        commit_msgs.push_back(mi->second);
        last_block_all_msgs[txcount]=mi->second;
        ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
        //fprintf(stderr,"BLOCK: %d %d %.*s\n",nsvid,mi->second->msid,2*sizeof(hash_t),hash);
        fprintf(stderr,"BLOCK: %d %d %.16s %0.16llX (%0.16llX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
        fprintf(fp,"%d %d %.*s\n",nsvid,mi->second->msid,2*sizeof(hash_t),hash);}}
    txs_.unlock();
    //hash_s last_block_all_message;
    //message_shash(last_block_all_message.hash,last_block_all_msgs); // consider sending this hash to other peers
    //ed25519_key2text(hash,last_block_all_message.hash,sizeof(hash_t));
    //message_shash(srvs_.txshash,last_block_all_msgs); // consider sending this hash to other peers
    srvs_.txs=last_block_all_msgs.size();
    srvs_.txs_put(last_block_all_msgs); //FIXME, add dbl_ messages !!! FIXME FIXME !!!
    ed25519_key2text(hash,srvs_.txshash,sizeof(hash_t));
    fprintf(fp,"0 0 %.*s\n",2*sizeof(hash_t),hash);
    fclose(fp);
    for(;!remove_msgs.empty();remove_msgs.pop()){
      auto mi=remove_msgs.top();
      if(mi->status & MSGSTAT_VAL){ // invalidate first
        mi->status &= ~MSGSTAT_VAL;}
//FIXME, make sure this message is not in other queue !!!
//FIXME, remove from all places or mark as removed :-/
      std::cerr << "REMOVING message " << mi->svid << ":" << mi->msid << "\n";
      //srvs_.nodes[mi->svid].msid=mi->msid-1; // assuming message exists
//FIXME, update peers.svid_msid_new !!!
      mi->remove();}
    check_.lock();
    for(;!invalidate_msgs.empty();invalidate_msgs.pop()){ // this will also invalidate all votes
      auto mi=invalidate_msgs.top();
      //std::cerr << "INVALIDATING message " << mi->svid << ":" << mi->msid << "\n";
      ed25519_key2text(hash,mi->sigh,sizeof(hash_t));
      fprintf(stderr,"INVALIDATING %d %d %.16s %0.16llX\n",mi->svid,mi->msid,hash,mi->hash.num);
      mi->status &= ~MSGSTAT_VAL;
      //srvs_.nodes[mi->svid].msid=mi->msid-1; // assuming message exists
      check_msgs_.push_front(mi);}
    check_.unlock();
    for(auto mi=commit_msgs.begin();mi!=commit_msgs.end();mi++){
      std::cerr << "COMMITING message " << (*mi)->svid << ":" << (*mi)->msid << "\n";}

// TODO, save current account status
    srvs_.finish(); //FIXME, add locking
    last_srvs_=srvs_; // consider not making copies of nodes
    srvs_.now+=BLOCKSEC;
    srvs_.blockdir();
    //TODO, add nodes if needed
    vip_max=srvs_.update_vip();
    free(hash);
    for(auto mj=cnd_msgs_.begin();mj!=cnd_msgs_.end();){
      auto mi=mj++;
      if(mi->second->msid<last_srvs_.now){
        cnd_msgs_.erase(mi);
        continue;}}
    for(auto mj=blk_msgs_.begin();mj!=blk_msgs_.end();){
      auto mi=mj++;
      if(mi->second->msid<last_srvs_.now){
        blk_msgs_.erase(mi);
        continue;}}
    std::cerr << "NEW BLOCK created\n";
  }

  void update_candidates(message_ptr msg)
  { cand_.lock();
    if(do_block>0){ // update candidates, check if this message was not missing
      for(auto it=candidates_.begin();it!=candidates_.end();it++){
        auto m=it->second->svid_miss.find(msg->svid);
        if(m!=it->second->svid_miss.end() && m->second.msid==msg->msid && !memcmp(m->second.sigh,msg->sigh,sizeof(hash_t))){
          it->second->waiting_server.erase(msg->svid);}}}
    cand_.unlock();
  }

  //message_ptr write_handshake(uint32_t ipv4,uint32_t port,uint16_t peer)
  message_ptr write_handshake(uint16_t peer,handshake_t& hs)
  { last_srvs_.header(hs.head); //
    last_srvs_.header_print(hs.head);
    if(peer){
      if(!do_sync){
        memcpy(hs.msha,srvs_.nodes[peer].msha,SHA256_DIGEST_LENGTH);
        hs.msid=srvs_.nodes[peer].msid;}
      else{
        memcpy(hs.msha,last_srvs_.nodes[peer].msha,SHA256_DIGEST_LENGTH);
        hs.msid=last_srvs_.nodes[peer].msid;}}
    else{
      bzero(hs.msha,SHA256_DIGEST_LENGTH);
      hs.msid=0;}
    message_ptr msg(new message(MSGTYPE_INI,(uint8_t*)&hs,(int)sizeof(handshake_t),opts_.svid,msid_,opts_.sk,opts_.pk));
    return(msg);
  }

  void write_message(std::string line) // assume single threaded
  { static boost::mutex mtx_;
    mtx_.lock();
    int msid=++msid_; // can be atomic
    mtx_.unlock();
    writemsid(); //FIXME we should save the message before updating the message counter
    message_ptr msg(new message(MSGTYPE_TXS,(uint8_t*)line.c_str(),(int)line.length(),opts_.svid,msid,opts_.sk,opts_.pk));
    if(!txs_insert(msg)){
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    //update(msg);
  }

  void write_candidate(const hash_s& last_message)
  { do_vote=0;
    message_ptr msg(new message(MSGTYPE_CND,last_message.hash,sizeof(hash_t),opts_.svid,srvs_.now,opts_.sk,opts_.pk)); //FIXME, consider msid=0 ???
//FIXME, is hash ok ?
    if(!cnd_insert(msg)){
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    std::cerr << "SENDING candidate\n";
    update(msg);
  }

  void save_candidate(const hash_s& h,candidate_ptr c)
  {
    cand_.lock(); // lock only candidates
    auto it=candidates_.find(h);
    if(it==candidates_.end()){
      candidates_[h]=c;}
    else if(c->peer){
      it->second->peers.insert(c->peer);}
    cand_.unlock();
  }

  candidate_ptr known_candidate(const hash_s& h,uint16_t peer)
  { 
    cand_.lock(); // lock only candidates
    auto it=candidates_.find(h);
    if(it==candidates_.end()){
      cand_.unlock();
      return(NULL);}
    if(peer){
      it->second->peers.insert(peer);}
    cand_.unlock();
    return(it->second);
  }

  void write_header()
  { header_t head;
    last_srvs_.header(head);
    message_ptr msg(new message(MSGTYPE_BLK,(uint8_t*)&head,sizeof(header_t),opts_.svid,head.now,opts_.sk,opts_.pk));
    if(!blk_insert(msg)){
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    std::cerr << "SENDING block (update)\n";
    //deliver(msg);
    update(msg); //send, even if I am not VIP
// save signature in signature lists
//FIXME, save only if I am important
  }


  void clock()
  { 
    //TODO, number of validators should depend on opts_.
    if(!do_validate){
      do_validate=1;
      threadpool.create_thread(boost::bind(&server::validator, this));
      threadpool.create_thread(boost::bind(&server::validator, this));}

    // block creation cycle
    hash_s cand;
    bool do_hallo=false;
    while(1){
      uint32_t now=time(NULL);
      std::cerr << "CLOCK: " << ((long)(srvs_.now+BLOCKSEC)-(long)now) << "\n";
      if(now>=(srvs_.now+BLOCKSEC) && do_block==0){
        std::cerr << "STOPing validation to start block\n";
        do_validate=0;
        threadpool.join_all();
        std::cerr << "STOPed validation to start block\n";
        //create message hash
        svid_.lock();
        last_svid_msgs.swap(svid_msgs_);
        svid_msgs_.clear();
        svid_.unlock();
        cand_.lock();
        electors.clear();
        candidates_.clear();
        cand_.unlock();
        message_shash(cand.hash,last_svid_msgs);
        message_msha(last_svid_msgs);
          message_ptr put_msg(new message(1+SHA256_DIGEST_LENGTH));
          put_msg->data[0]=MSGTYPE_STP;
          memcpy(put_msg->data+1,cand.hash,SHA256_DIGEST_LENGTH);
          char hash[2*SHA256_DIGEST_LENGTH]; hash[2*SHA256_DIGEST_LENGTH-1]='?';
          ed25519_key2text(hash,put_msg->data+1,SHA256_DIGEST_LENGTH);
          fprintf(stderr,"LAST HASH put %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
          deliver(put_msg); // sets BLOCK_MODE for peers
        candidate_ptr c_ptr(new candidate());
        save_candidate(cand,c_ptr);
        prepare_poll(); // sets do_vote
        do_block=1;
        do_validate=1;
        do_hallo=false;
        threadpool.create_thread(boost::bind(&server::validator, this));
        threadpool.create_thread(boost::bind(&server::validator, this));}
      if(do_block>0 && do_block<3){
        count_votes(now,cand);}
      if(do_block==3){
        std::cerr << "STOPing validation to finish block\n";
        do_validate=0;
        threadpool.join_all();
        std::cerr << "STOPed validation to finish block\n";
        finish_block();
        //writelastpath();
        writemsid();
        //char pathname[16];
        //sprintf(pathname,"%08X",srvs_.now+BLOCKSEC); // size depends on the time_ shift !!!
        //mkdir(pathname,0755);
        svid_.lock();
        svid_msgs_.clear();
        svid_.unlock();
        write_header(); // send new block signature
        do_block=0;
        do_validate=1;
        threadpool.create_thread(boost::bind(&server::validator, this));
        threadpool.create_thread(boost::bind(&server::validator, this));}
      if(!do_block && !do_hallo && now-srvs_.now>(BLOCKSEC/4+ opts_.svid*VOTE_DELAY) && svid_msgs_.size()<MIN_MSGNUM){
        std::cerr << "SILENCE, sending void message due to silence\n";
        std::string line("Hallo world @ ");
        line.append(std::to_string(now));
        write_message(line);
        do_hallo=true;}
      boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
  }

  struct hash_cmp
  { bool operator()(const hash_s& i,const hash_s& j) const {int k=memcmp(i.hash,j.hash,sizeof(hash_t)); return(k<0);}
  };

  void missing_msgs_erase(message_ptr& msg)
  { missing_.lock();
    missing_msgs_.erase(msg->hash.num);
    missing_.unlock();
  }

  void missing_msgs_insert(message_ptr& msg)
  { missing_.lock();
    missing_msgs_[msg->hash.num]=msg;
    missing_.unlock();
  }

  int check_msgs_size()
  { return(check_msgs_.size());
  }

  std::map<uint16_t,message_ptr> last_svid_msgs; // last validated message from server, should change this to now_svid_msgs
  std::map<uint16_t,msidhash_t> svid_msha; // copy of msid and hashed from last_svid_msgs
  //FIXME, use serv_.now instead
  servers last_srvs_;
  //message_ptr block; // my block message, now data in last_srvs_
  int do_sync;
  int do_fast;
  int do_check;
  //int get_headers; //TODO :-( reduce the number of flags
  uint32_t msid_; // change name to msid :-)
  int vip_max;
  boost::mutex peer_; //FIXME, make this private
  std::list<servers> headers; //FIXME, make this private
  uint32_t get_txslist; //block id of the requested txslist of messages
private:
  enum { max_connections = 4 };
  enum { max_recent_msgs = 1024 }; // this is the block chain :->
  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  servers srvs_;
  options& opts_;
  boost::thread_group threadpool;
  boost::thread* clock_thread;
  //uint8_t lasthash[SHA256_DIGEST_LENGTH]; // hash of last block, this should go to path/servers.txt
  //uint8_t prevhash[SHA256_DIGEST_LENGTH]; // hash of previous block, this should go to path/servers.txt
  int do_validate; // keep validation threads running
  //uint32_t maxnow; // do not process messages if time >= maxnow
  candidate_ptr winner; // elected candidate
  std::set<peer_ptr> peers_;
  std::map<hash_s,candidate_ptr,hash_cmp> candidates_; // list of candidates
  message_queue wait_msgs_; //TODO, not used yet :-/
  message_queue check_msgs_;
  std::map<uint16_t,message_ptr> svid_msgs_; //last validated txs message or dbl message from server
  std::map<uint64_t,message_ptr> missing_msgs_; //TODO, start using this, these are messages we still wait for
  std::map<uint64_t,message_ptr> txs_msgs_; //_TXS messages (transactions)
  std::map<uint64_t,message_ptr> cnd_msgs_; //_CND messages (block candidates)
  std::map<uint64_t,message_ptr> blk_msgs_; //_BLK messages (blocks) or messages in a block ehcn syncing
  std::map<uint64_t,message_ptr> dbl_msgs_; //_DBL messages (double spend)
  boost::mutex cand_;
  boost::mutex wait_;
  boost::mutex check_;
  boost::mutex svid_;
  boost::mutex missing_;
  boost::mutex txs_;
  boost::mutex cnd_;
  boost::mutex blk_;
  boost::mutex dbl_;
  // voting
  std::map<uint16_t,uint32_t> electors;
  float votes_max;
  int do_vote;
  int do_block;
  //int vip_ok;
  //int vip_no;
  //std::map<uint16_t,uint32_t> svid_msid; //TODO, maybe not used
};

#endif
