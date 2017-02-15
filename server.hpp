#ifndef SERVER_HPP
#define SERVER_HPP

#include "main.hpp"
 
class office;
class peer;
typedef boost::shared_ptr<peer> peer_ptr;

class server
{
public:
  server(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint,options& opts) :
    do_sync(1),
    do_fast(1),
    io_service_(io_service),
    acceptor_(io_service, endpoint),
    opts_(opts),
    do_validate(0),
    votes_max(0.0),
    do_vote(0),
    do_block(0)
    //ofip(NULL)
  { mkdir("usr",0755); // create dir for bank accounts
    //mkdir("und",0755); // create dir for undo accounts, not needed
    mklogdir(opts_.svid);
    mklogfile(opts_.svid,0);
    uint32_t path=readmsid(); // reads msid_ and path, FIXME, do not read msid, read only path
    uint32_t lastpath=path;
    fprintf(stderr,"START @ %08X with MSID: %08X\n",path,msid_);
    last_srvs_.get(path);
    if(last_srvs_.nodes.size()<=(unsigned)opts_.svid){ 
      std::cerr << "ERROR: reading servers\n";
      exit(-1);} 
    //if(memcmp(last_srvs_.nodes[opts_.svid].pk,opts_.pk,32)){ // move this to get servers
    pkey=last_srvs_.nodes[opts_.svid].pk;
    if(!last_srvs_.find_key(pkey,skey)){
      char pktext[2*32+1]; pktext[2*32]='\0';
      ed25519_key2text(pktext,pkey,32);
      //std::cerr << "ERROR: failed to find secret key for key:\n"<<pktext<<"\n";
      fprintf(stderr,"ERROR: failed to find secret key for key:\n%.16s\n",pktext);
      exit(-1);}
    //TODO, check vok and vno, if bad, inform user and suggest an older starting point

    if(opts_.init){
      uint32_t now=time(NULL);
      now-=now%BLOCKSEC;
      last_srvs_.init(now-BLOCKSEC);}
    srvs_=last_srvs_;
    pkey=srvs_.nodes[opts_.svid].pk;
    memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
    srvs_.now+=BLOCKSEC;
    srvs_.blockdir();
    vip_max=srvs_.update_vip(); //based on initial weights at start time
    bank_fee.resize(srvs_.nodes.size());

    if(!undo_bank()){//check database consistance, if 
      if(!opts_.fast){
        std::cerr<<"DATABASE check failed, must use fast option to load new datase from network\n";
        exit(-1);}
      do_sync=1;}
    else{
      std::cerr<<"DATABASE check passed\n";
      uint32_t now=time(NULL);
      now-=now%BLOCKSEC;
      if(last_srvs_.now==now-BLOCKSEC){
        do_sync=0;}
      else{
        if(last_srvs_.now<now-MAXLOSS && !opts_.fast){
          std::cerr<<"WARNING, possibly missing too much history for full resync\n";}
        do_sync=1;}}

    //FIXME, move this to a separate thread that will keep a minimum number of connections
    for(std::string addr : opts_.peer){
      //std::cerr<<"CONNECT :"<<addr<<"\n";
      connect(addr);
      boost::this_thread::sleep(boost::posix_time::seconds(2));} //wait some time before connecting to more peers

    if(do_sync){
      if(opts_.fast){ //FIXME, do slow sync after fast sync
        while(do_fast){ // fast_sync changes the status, FIXME use future/promis
          boost::this_thread::sleep(boost::posix_time::seconds(1));}
        //wait for all user files to arrive
        load_banks();}
      else{
        do_fast=0;}
      std::cerr<<"START syncing headers\n";
      load_chain(); // sets do_sync=0;
      svid_msgs_.clear();}
    //load old messages or check/modify

    recyclemsid(lastpath+BLOCKSEC);
    writemsid(); // synced to new position
    clock_thread = new boost::thread(boost::bind(&server::clock, this));
    start_accept(); // should consider sending a 'hallo world' message
    //if(!opts_.offi){
    //  return;}
    //while(ofip==NULL){
    //  std::cerr<<"WAITING for office\n";
    //  boost::this_thread::sleep(boost::posix_time::seconds(2));}
    //start_office();
  }
  ~server()
  { //threadpool.interrupt_all();
    do_validate=0;
    threadpool.join_all();

    clock_thread->interrupt();
    clock_thread->join();
  }

  void recyclemsid(uint32_t lastpath)
  { uint32_t firstmsid=srvs_.nodes[opts_.svid].msid;
    hash_t msha;
    if(firstmsid>msid_){
      std::cerr<<"WARNING increase msid\n";
      msid_=firstmsid;
      return;}
    if(firstmsid==msid_){
      std::cerr<<"NO recycle needed\n";
      return;}
    std::cerr<<"START recycle\n";
    memcpy(msha,srvs_.nodes[opts_.svid].msha,sizeof(hash_t));
    firstmsid++;
    for(uint32_t lastmsid=firstmsid;lastmsid<=msid_;lastmsid++){
//FIXME, must sign this message again if message too old !!!
      message_ptr msg(new message(MSGTYPE_TXS,lastpath,opts_.svid,lastmsid,pkey,msha)); //load from file
      if(msg->status!=MSGSTAT_DAT){
        fprintf(stderr,"ERROR, failed to read message %08X/%02x_%04x_%08x.txt\n",
          lastpath,MSGTYPE_TXS,opts_.svid,lastmsid);
        msid_=lastmsid-1;
        return;}
      if(txs_insert(msg)){
        fprintf(stderr,"RECYCLED message %04X:%08X from %08X/ inserted\n",opts_.svid,lastmsid,lastpath);
	if(srvs_.now!=lastpath){
          fprintf(stderr,"MOVE message %04X:%08X from %08X/ to %08X/\n",opts_.svid,lastmsid,lastpath,srvs_.now);
          msg->move(srvs_.now);}}
      else{
        fprintf(stderr,"RECYCLED message %04X:%08X from %08X/ known\n",opts_.svid,lastmsid,lastpath);}}
    //std::cerr<<"FINISH recycle (remove old files)\n";
    //if(srvs_.now!=lastpath){
    //  message_ptr msg(new message());
    //  msg->path=lastpath;
    //  msg->hashtype(MSGTYPE_TXS);
    //  msg->svid=opts_.svid;
    //  for(;firstmsid<=msid_;firstmsid++){
    //    msg->msid=firstmsid;
    //    fprintf(stderr,"REMOVING message %04X:%08X from %08X/\n",msg->svid,msg->msid,lastpath);
    //    msg->remove();}}
  }

  void mklogdir(uint16_t svid)
  { char filename[64];
    sprintf(filename,"log/%04X",svid);
    mkdir("log",0755); // create dir for user history
    mkdir(filename,0755);
  }

  void mklogfile(uint16_t svid,uint32_t user)
  { char filename[64];
    sprintf(filename,"log/%04X/%03X",svid,user>>20);
    mkdir(filename,0755);
    sprintf(filename,"log/%04X/%03X/%03X",svid,user>>20,(user&0xFFF00)>>8);
    mkdir(filename,0755);
    //create log file ... would be created later anyway
    sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
    int fd=open(filename,O_WRONLY|O_CREAT,0644);
    if(fd<0){
      std::cerr<<"ERROR, failed to create log directory "<<filename<<"\n";}
    close(fd);
  }

  int purge_log(int fd,uint32_t user) // this is ext4 specific !!!
  { log_t log;
    assert(!(4096%sizeof(log_t)));
    int r,size=lseek(fd,0,SEEK_END); // maybe stat would be faster
    if(size%sizeof(log_t)){
      std::cerr<<"ERROR, log corrupt register\n";
      return(-2);}
    if(size<LOG_PURGE_START){
      return(0);}
    lock_user(user);
    size=lseek(fd,0,SEEK_END); // just in case of concurrent purge
    for(;size>=LOG_PURGE_START;size-=4096){
      if(lseek(fd,4096-sizeof(log_t),SEEK_SET)!=4096-sizeof(log_t)){
        unlock_user(user);
        std::cerr<<"ERROR, log lseek error\n";
        return(-1);}
      if((r=read(fd,&log,sizeof(log_t)))!=sizeof(log_t)){
        unlock_user(user);
        //std::cerr<<"ERROR, log read error\n";
        fprintf(stderr,"ERROR, log read error (%d,%s)\n",r,strerror(errno));
        return(-1);}
      if(log.time+MAX_LOG_AGE>=srvs_.now){ // purge first block
        unlock_user(user);
        return(0);}
      if((r=fallocate(fd,FALLOC_FL_COLLAPSE_RANGE,0,4096))<0){
        unlock_user(user);
        //std::cerr<<"ERROR, log purge failed\n";
        fprintf(stderr,"ERROR, log purge failed (%d,%s)\n",r,strerror(errno));
        return(-1);}}
    unlock_user(user);
    return(0);
  }

  // use (2) fallocate to remove beginning of files
  // Remove page A: ret = fallocate(fd, FALLOC_FL_COLLAPSE_RANGE, 0, 4096);
  void put_log(uint16_t svid,std::map<uint64_t,log_t>& log)
  { uint32_t luser=MAX_USERS;
    int fd=-1;
    if(log.empty()){
      return;}
    for(auto it=log.begin();it!=log.end();it++){
      uint32_t user=(it->first)>>32;
      if(luser!=user){
        if(fd>=0){
          purge_log(fd,luser);
          close(fd);
          //unlock_user(luser);
          }
        luser=user;
        char filename[64];
        sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
        fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644); //maybe no lock needed with O_APPEND
        if(fd<0){
          std::cerr<<"ERROR, failed to open log register "<<filename<<"\n";
          return;} // :-( maybe we should throw here something
        //lock_user(user); // needed only when purging
        }
//TODO, monotonic time increase would help
      write(fd,&it->second,sizeof(log_t));}
    purge_log(fd,luser);
    close(fd);
    //unlock_user(luser);
  }

  bool fix_log(uint16_t svid,uint32_t user)
  { char filename[64];
    struct stat sb;
    sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
    int rd=open(filename,O_RDONLY|O_CREAT,0644); //maybe no lock needed with O_APPEND
    if(rd<0){
      std::cerr<<"ERROR, failed to open log register "<<filename<<" for reading\n";
      return(false);}
    fstat(rd,&sb);
    if(!(sb.st_size%sizeof(log_t))){ // file is ok
      close(rd);
      return(false);}
    int l=lseek(rd,sb.st_size%sizeof(log_t),SEEK_SET);
    int ad=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644); //maybe no lock needed with O_APPEND
    if(ad<0){
      std::cerr<<"ERROR, failed to open log register "<<filename<<" for appending\n";
      close(rd);
      return(false);}
    log_t log;
    memset(&log,0,sizeof(log_t));
    write(ad,&log,l); // append a suffix
    close(ad);
    int wd=open(filename,O_WRONLY|O_CREAT,0644); //maybe no lock needed with O_APPEND
    if(wd<0){
      std::cerr<<"ERROR, failed to open log register "<<filename<<" for writing\n";
      close(rd);
      return(false);}
    fstat(wd,&sb);
    if(sb.st_size%sizeof(log_t)){ // file is ok
      std::cerr<<"ERROR, failed to append log register "<<filename<<", why ???\n";
      close(rd);
      close(wd);
      return(false);}
    for(l=sizeof(log_t);l<sb.st_size;l+=sizeof(log_t)){
      if(read(rd,&log,sizeof(log_t))!=sizeof(log_t)){
        std::cerr<<"ERROR, failed to read log register "<<filename<<" while fixing\n";
        break;}
      if(write(wd,&log,sizeof(log_t))!=sizeof(log_t)){
        std::cerr<<"ERROR, failed to write log register "<<filename<<" while fixing\n";
        break;}}
    if(write(wd,&log,sizeof(log_t))!=sizeof(log_t)){
      std::cerr<<"ERROR, failed to write log register "<<filename<<" while duplicating last log\n";}
    close(rd);
    close(wd);
    return(false);
  }

  bool get_log(uint16_t svid,uint32_t user,uint32_t from,std::string& slog)
  { char filename[64];
    struct stat sb;
    if(from==0xffffffff){
      fix_log(svid,user);}
    sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
    int fd=open(filename,O_RDWR|O_CREAT,0644); //maybe no lock needed with O_APPEND
    if(fd<0){
      std::cerr<<"ERROR, failed to open log register "<<filename<<"\n";
      return(false);}
    fstat(fd,&sb);
    uint32_t len=sb.st_size;
    slog.append((char*)&len,4);
    if(len%sizeof(log_t)){
      std::cerr<<"ERROR, log corrupt register "<<filename<<"\n";
      from=0;} // ignore from 
    log_t log;
    int l;
    for(uint32_t tot=0;(l=read(fd,&log,sizeof(log_t)))>0 && tot<len;tot+=l){
      if(log.time<from){
        continue;}
      slog.append((char*)&log,l);}
    if(!(len%sizeof(log_t))){
      purge_log(fd,user);}
    close(fd);
    return(true);
  }

  //update_nodehash is similar
  int undo_bank() //will undo database changes and check if the database is consistant
  { //could use multiple threads but disk access could limit the processing anyway
    uint32_t path=srvs_.now; //use undo from next block
    fprintf(stderr,"CHECK DATA @ %08X (and undo @ %08X)\n",last_srvs_.now,path);
    for(uint16_t bank=1;bank<last_srvs_.nodes.size();bank++){
      char filename[64];
      sprintf(filename,"usr/%04X.dat",bank);
      int fd=open(filename,O_RDWR);
      if(fd<0){
        return(0);}
      sprintf(filename,"%08X/und/%04X.dat",path,bank);
      int ud=open(filename,O_RDONLY);
      uint32_t users=last_srvs_.nodes[bank].users;
       int64_t weight=0;
      SHA256_CTX sha256;
      SHA256_Init(&sha256);
      for(uint32_t user=0;user<users;user++){
        user_t u;
        if(ud>=0){
          u.msid=0;
          if(sizeof(user_t)==read(ud,&u,sizeof(user_t)) && u.msid){
            //std::cerr<<"OVERWRITE: "<<bank<<":"<<user<<" ("<<u.weight<<")\n";
            fprintf(stderr,"OVERWRITE: %04X:%08X (weight:%016lX)\n",bank,user,u.weight);
            write(fd,&u,sizeof(user_t)); //overwrite bank file
            goto NEXTUSER;}}
        if(sizeof(user_t)!=read(fd,&u,sizeof(user_t))){
          close(fd);
          if(ud>=0){
            close(ud);}
          //std::cerr << "ERROR loading bank "<<bank<<" (bad read)\n";
          fprintf(stderr,"ERROR loading bank %04X (bad read)\n",bank);
          return(0);}
        NEXTUSER:;
        weight+=u.weight;
        SHA256_Update(&sha256,&u,sizeof(user_t));}
      close(fd);
      if(ud>=0){
        close(ud);}
      if(last_srvs_.nodes[bank].weight!=weight){
        //std::cerr << "ERROR loading bank "<<bank<<" (bad sum:"<<last_srvs_.nodes[bank].weight<<"<>"<<weight<<")\n";
        fprintf(stderr,"ERROR loading bank %04X (bad sum:%016lX<>%016lX)\n",
          bank,last_srvs_.nodes[bank].weight,weight);
        return(0);}
      uint8_t hash[32];
      SHA256_Final(hash,&sha256);
      if(memcmp(last_srvs_.nodes[bank].hash,hash,32)){
        //std::cerr << "ERROR loading bank "<<bank<<" (bad hash)\n";
        fprintf(stderr,"ERROR loading bank %04X (bad hash)\n",bank);
        return(0);}
      if(ud>=0){
        unlink(filename);}}
    return(1);
  }

  void load_banks()
  {
    //create missing bank messages
    uint16_t end=last_srvs_.nodes.size();
    missing_.lock();
    for(uint16_t bank=1;bank<end;bank++){
      //TODO, unlikely but maybe we have the correct bank already, we could check this
      message_ptr put_msg(new message());
      put_msg->data[0]=MSGTYPE_USG;
      put_msg->data[1]=0;
      memcpy(put_msg->data+2,&last_srvs_.now,4);
      memcpy(put_msg->data+6,&bank,2);
      put_msg->msid=last_srvs_.now;
      put_msg->svid=bank;
      put_msg->hash.num=put_msg->dohash(put_msg->data);
      put_msg->got=0; // do first request emidiately
      missing_msgs_[put_msg->hash.num]=put_msg;
      fillknown(put_msg);
      uint16_t peer=put_msg->request();
      if(peer){
        //std::cerr << "REQUESTING USR "<<bank<<" from "<<peer<<"\n";
        fprintf(stderr,"REQUESTING BANK %04X from %04X\n",bank,peer);
        deliver(put_msg,peer);}}
    while(missing_msgs_.size()){
      //do new requests here
      missing_.unlock();
      std::cerr << "WAITING for banks\n";
      boost::this_thread::sleep(boost::posix_time::seconds(2)); //yes, yes, use futur/promise instead
      missing_.lock();}
    missing_.unlock();
    //we should have now all banks
  }

  uint64_t need_bank(uint16_t bank) //FIXME, return 0 if not at this stage
  { union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0;
    h.dat[1]=MSGTYPE_USR;
    memcpy(h.dat+2,&last_srvs_.now,4);
    memcpy(h.dat+6,&bank,2);
    missing_.lock();
    if(missing_msgs_.find(h.num)!=missing_msgs_.end()){
      missing_.unlock();
      return(h.num);}
    missing_.unlock();
    return(0);
  }

  void have_bank(uint64_t hnum)
  { missing_.lock();
    missing_msgs_.erase(hnum);
    missing_.unlock();
  }

  void load_chain()
  { //char pathname[16];
    uint32_t now=time(NULL);
    now-=now%BLOCKSEC;
    do_validate=1;
    threadpool.create_thread(boost::bind(&server::validator, this));
    threadpool.create_thread(boost::bind(&server::validator, this));
//FIXME, must start with a matching nowhash and load serv_
    for(auto block=headers.begin();srvs_.now<now;){
      peer_.lock();
      if(block==headers.end()){ // wait for peers to load more blocks
        servers& peer_ls=headers.back();
        peer_.unlock();
	if(block!=headers.begin()){
          //std::cerr<<"WAITING for headers "<<block->now<<", maybe need more peers\n";
          //fprintf(stderr,"WAITING for headers %08X, maybe need more peers\n",headers.rbegin()->now+BLOCKSEC);
          fprintf(stderr,"WAITING for headers %08X, maybe need more peers\n",peer_ls.now+BLOCKSEC);
          get_more_headers(peer_ls.now+BLOCKSEC,peer_ls.nowhash);}
        peer_.lock();
        if(block==headers.end()){ // wait for peers to load more blocks
          peer_.unlock();
          boost::this_thread::sleep(boost::posix_time::seconds(2));}
	else{
          peer_.unlock();}
        continue;}
      peer_.unlock();
      //std::cerr<<"START syncing header "<<block->now<<"\n";
      fprintf(stderr,"START syncing header %08X\n",block->now);
      if(srvs_.now!=block->now){
        //std::cerr<<"ERROR, got strange block numbers "<<srvs_.now<<"<>"<<block->now<<"\n";
        fprintf(stderr,"ERROR, got strange block numbers %08X<>%08X\n",srvs_.now,block->now);
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
              //std::cerr << "REQUESTING TXL from "<<svid<<"\n";
              fprintf(stderr,"REQUESTING TXL from %04X\n",svid);
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
      std::set<uint16_t> update;
      missing_.lock();
      for(auto it=missing_msgs_.begin();it!=missing_msgs_.end();){
        missing_.unlock();
        update.insert(it->second->svid);
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
	if(jt->second->load()){
          if(!jt->second->sigh_check(jt->second->data+4)){
            jt->second->read_head(); //to get 'now'
            //std::cerr << "LOADING TXS "<<jt->second->svid<<":"<<jt->second->msid<<" from database ("<<jt->second->path<<")\n";
            fprintf(stderr,"LOADING TXS %04X:%08X from path:%08X\n",
              jt->second->svid,jt->second->msid,jt->second->path);
            check_.lock();
            check_msgs_.push_back(jt->second); // send to validator
            check_.unlock();
            missing_msgs_erase(jt->second);}
          else{
            //std::cerr << "LOADING TXS "<<jt->second->svid<<":"<<jt->second->msid<<" from database("<<jt->second->path<<" failed !!!)\n";
            fprintf(stderr,"LOADING TXS %04X:%08X from path:%08X failed\n",
              jt->second->svid,jt->second->msid,jt->second->path);
            jt->second->len=message::header_length;}
          continue;} // assume no lock needed
	fillknown(jt->second);
	uint16_t svid=jt->second->request(); //FIXME, maybe request only if this is the next needed message, need to have serv_ ... ready for this check :-/
        if(svid){
          //std::cerr << "REQUESTING TXS from "<<svid<<"\n";
          fprintf(stderr,"REQUESTING TXS from %04X\n",svid);
          deliver(jt->second,svid);}
        missing_.lock();}
      missing_.unlock();
      //wait for all messages to be processed by the validators
      blk_.lock();
      while(blk_msgs_.size()){
        blk_.unlock();
        boost::this_thread::sleep(boost::posix_time::seconds(1)); //yes, yes, use futur/promise instead
	blk_.lock();}
      blk_.unlock();
      std::cerr << "COMMIT deposits\n";
      commit_block(update); // process bkn and get transactions
      commit_deposit(update);
      std::cerr << "UPDATE accounts\n";
      for(auto it=update.begin();it!=update.end();it++){
        assert(*it<srvs_.nodes.size());
        //FIXME, this can be slow :-( maybe we could run this at the end of sync only :-(
        srvs_.update_nodehash(*it);} //also calculates weight of the node
      //finish block
      srvs_.finish(); //FIXME, add locking
      if(memcmp(srvs_.nowhash,block->nowhash,SHA256_DIGEST_LENGTH)){
        //std::cerr<<"ERROR, failed to arrive at correct hash at block "<<srvs_.now<<", fatal\n";
        fprintf(stderr,"ERROR, failed to arrive at correct hash at block %08X, fatal\n",srvs_.now);
        exit(-1);}
      last_srvs_=srvs_; // consider not making copies of nodes
      memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
      srvs_.now+=BLOCKSEC;
      srvs_.blockdir();
      now=time(NULL);
      now-=now%BLOCKSEC;
      peer_.lock();
      block++;
      peer_.unlock();}
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

        //FIXME, do this after loading banks
	std::cerr<<"SYNC copy\n";
        srvs_=last_srvs_; //FIXME, create a copy function
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
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

  uint32_t readmsid()
  { if(opts_.init){
      msid_=0;
      return(0);}
    FILE* fp=fopen("msid.txt","r");
    if(fp==NULL){
      return(0);}
    uint32_t path;
    uint32_t svid;
    fscanf(fp,"%X %X %X",&msid_,&path,&svid);
    fclose(fp);
    if(svid!=(uint32_t)opts_.svid){
      throw("FATAL ERROR: failed to read correct svid from msid.txt\n");}
    return(path);
    /*std::ifstream myfile("msid.txt");
    if(!myfile.is_open()){
      msid_=0;
      return(0);}
    std::string line1;
    std::string line2;
    getline (myfile,line1);
    getline (myfile,line2);
    myfile.close();
    msid_=atoi(line1.c_str());
    return(atoi(line2.c_str()));*/
  }

  //FIXME, move this to servers.hpp
  void writemsid()
  { FILE* fp=fopen("msid.txt","w");
    if(fp==NULL){
      throw("FATAL ERROR: failed to write to msid.txt\n");}
    fprintf(fp,"%08X %08X %04X\n",msid_,last_srvs_.now,opts_.svid);
    fclose(fp);
    /*std::ofstream myfile("msid.txt");
    if(!myfile.is_open()){
      throw("FATAL ERROR: failed to write to msid.txt \n");}
    myfile << std::to_string(msid_) << "\n" << std::to_string(last_srvs_.now) << "\n";
    myfile.close();*/
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
    SHA256_Final(mhash, &sha256); // std::cerr << "message_shash\n";
  }

  void count_votes(uint32_t now,hash_s& cand) // cand_.locked()
  { candidate_ptr num1=NULL;
    candidate_ptr num2=NULL;
    uint64_t votes_counted=0;
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
    if(do_block<2 && (
        (num1->score>(num2!=NULL?num2->score:0)+(votes_max-votes_counted))||
        (now>srvs_.now+BLOCKSEC+MAX_ELEWAIT))){
      uint64_t x=(num2!=NULL?num2->score:0);
      //std::cerr << "BLOCK elected: " << num1->score << " second:" << x << " max:" << votes_max << " counted:" << votes_counted << "\n";
      if(now>srvs_.now+BLOCKSEC+MAX_ELEWAIT){
        fprintf(stderr,"CANDIDATE SELECTED:%016lX second:%016lX max:%016lX counted:%016lX BECAUSE OF TIMEOUT!!!\n",
          num1->score,x,votes_max,votes_counted);}
      else{
        fprintf(stderr,"CANDIDATE ELECTED:%016lX second:%016lX max:%016lX counted:%016lX\n",
          num1->score,x,votes_max,votes_counted);}
      do_block=2;
      winner=num1;
      if(winner->failed_peer){
        std::cerr << "BAD CANDIDATE elected :-( must resync :-( \n"; // FIXME, do not exit, initiate sync
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
      //std::cerr << "ELECTOR accepted : " << it->second->svid << "(" << it->second->msid << ")\n";
      fprintf(stderr,"ELECTOR accepted:%04X msid:%08X\n",it->second->svid,it->second->msid);
      svid_rank.push_back(it->second->svid);}
    if(!svid_rank.size()){
      std::cerr << "ERROR, no valid server for this block :-(, TODO create own block\n";}
    else{
      //std::cerr << "SORT \n";
      std::sort(svid_rank.begin(),svid_rank.end(),[this](const uint16_t& i,const uint16_t& j){return(this->srvs_.nodes[i].weight>this->srvs_.nodes[j].weight);});} //fuck, lambda :-/
    //TODO, save this list
    for(uint32_t j=0;j<VOTES_MAX && j<svid_rank.size();j++){
      if(svid_rank[j]==opts_.svid){
        do_vote=1+j;}
      //std::cerr << "ELECTOR[" << svid_rank[j] << "]=" << srvs_.nodes[svid_rank[j]].weight << "\n";
      fprintf(stderr,"ELECTOR[%d]=%016lX\n",svid_rank[j],srvs_.nodes[svid_rank[j]].weight);
      electors[svid_rank[j]]=srvs_.nodes[svid_rank[j]].weight;
      votes_max+=srvs_.nodes[svid_rank[j]].weight;}
    winner=NULL;
    //std::cerr << "ELECTOR max:" << votes_max << "\n";
    fprintf(stderr,"ELECTOR max:%016lX\n",votes_max);
    cand_.unlock();
  }

  message_ptr message_svidmsid(uint16_t svid,uint32_t msid)
  { 
    union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0; // hash
    h.dat[1]=0; // message type
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    fprintf(stderr,"HASH find:%016lX (%04X:%08X) %d:%d\n",h.num,svid,msid,svid,msid);
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
  { fprintf(stderr,"HASH find:%016lX (%04X%08X) %d:%d\n",msg->hash.num,msg->svid,msg->msid,msg->svid,msg->msid);
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
for(auto me=cnd_msgs_.begin();me!=cnd_msgs_.end();me++){ fprintf(stderr,"HASH have: %016lX (%02X)\n",me->first,me->second->hashval(svid));} //data[4+(svid%64)]
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
    fprintf(stderr,"HASH insert:%016lX (DBL)\n",msg->hash.num);
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
    fprintf(stderr,"HASH insert:%016lX (CND) [len:%d]\n",msg->hash.num,msg->len);
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
    fprintf(stderr,"HASH insert:%016lX (BLK)\n",msg->hash.num);
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
    //fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d]\n",msg->hash.num,msg->len);
    std::map<uint64_t,message_ptr>::iterator it=txs_msgs_.find(msg->hash.num);
    if(it!=txs_msgs_.end()){
      message_ptr osg=it->second;
      // overwrite message status with the current one
      // not needed any more msg->status=osg->status; // for peer.hpp to check if message is already validated
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
        if(do_sync && memcmp(osg->sigh,msg->sigh,SHA256_DIGEST_LENGTH)){
          txs_.unlock();
          fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] WRONG SIGNATURE HASH!\n",msg->hash.num,msg->len);
          return(0);}
        osg->update(msg);
        osg->path=srvs_.now;
        txs_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){ //FIXME, change path
          fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] SAVE FAILED, ABORT!\n",msg->hash.num,msg->len);
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
            fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] DOUBLE SPEND!\n",msg->hash.num,msg->len);
            create_double_spend_proof(pre,osg); // should copy messages from this server to ds_msgs_
            return(1);}
          if(nxt!=NULL && nxt->len>message::header_length && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
            fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] DOUBLE SPEND!\n",msg->hash.num,msg->len);
            create_double_spend_proof(nxt,osg); // should copy messages from this server to ds_msgs_
            return(1);}
          if(osg->now>=srvs_.now+BLOCKSEC){
            fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] delay to %08X/ ???\n",msg->hash.num,msg->len,osg->now);
            wait_.lock();
            wait_msgs_.push_back(osg);
            wait_.unlock();
            return(1);}}//FIXME, process wait messages later
        // process ordinary messages
        fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] queued\n",msg->hash.num,msg->len);
        check_.lock();
        check_msgs_.push_back(osg);
        check_.unlock();
        return(1);}
      else{ // update info about peer inventory
        txs_.unlock();
        fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] ignored\n",msg->hash.num,msg->len);
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    else{
      if(msg->len==message::header_length){
        txs_msgs_[msg->hash.num]=msg;
        txs_.unlock();
        fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] set as missing\n",msg->hash.num,msg->len);
        missing_msgs_insert(msg);
        return(1);}
      if(msg->svid==opts_.svid){ // own message
        txs_msgs_[msg->hash.num]=msg;
        msg->path=srvs_.now;
        txs_.unlock();
        fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] store as own\n",msg->hash.num,msg->len);
        msg->save();
        check_.lock();
        check_msgs_.push_back(msg); // running though validator
        check_.unlock();
	assert(msg->peer==msg->svid);
        return(1);}
      txs_.unlock();
      fprintf(stderr,"HASH insert:%016lX (TXS) [len:%d] UNEXPECTED!\n",msg->hash.num,msg->len);
      return(-1);}
  }

  void cnd_validate(message_ptr msg)
  {
    std::cerr << "CANDIDATE test\n";
    msg->status=MSGSTAT_VAL; // all can vote
    cand_.lock();
    if(electors.find(msg->svid)==electors.end()){ //FIXME, electors should have assigned rank when building poll
      //std::cerr << "BAD ELECTOR "<< msg->svid <<" :-( \n";
      fprintf(stderr,"BAD ELECTOR %04X\n",msg->svid);
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
    //std::cerr << "CANDIDATE score: "<< it->second->score <<"(+"<< electors[msg->svid] <<")\n";
    fprintf(stderr,"CANDIDATE score:%016lX (added:%016lX)\n",it->second->score,electors[msg->svid]);
    electors[msg->svid]=0;
    cand_.unlock();
    update(msg); // update others
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
    if(no){
      fprintf(stderr,"\n\nBLOCK differs, disconnect!\n\n\n\n");
      disconnect(msg->svid);}
  }

  void missing_sent_remove(uint16_t svid)
  { missing_.lock();
    for(auto mi=missing_msgs_.begin();mi!=missing_msgs_.end();mi++){
      mi->second->sent.erase(svid);}
    missing_.unlock();
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
	for(auto mj=missing_msgs_.begin();mj!=missing_msgs_.end();){
          //FIXME, request _BLK messages if BLOCK ready 
          auto mi=mj++;
          if(mi->second->hash.dat[1]==MSGTYPE_BLK){ //FIXME, consider checking mi->first
            if(mi->second->msid<last_srvs_.now-BLOCKSEC){
              missing_msgs_.erase(mi);
              continue;}
            if(now>mi->second->got+MAX_MSGWAIT && mi->second->msid<=last_srvs_.now){
              tmp_msgs_.push_back(mi->second);}}
          else{
            if(mi->second->msid<srvs_.nodes[mi->second->svid].msid){
              missing_msgs_.erase(mi);
              continue;}
            if(now>mi->second->got+MAX_MSGWAIT && srvs_.nodes[mi->second->svid].msid==mi->second->msid-1){
              tmp_msgs_.push_back(mi->second);}}}
	missing_.unlock();
        for(auto re=tmp_msgs_.begin();re!=tmp_msgs_.end();re++){
	  uint16_t svid=(*re)->request();
          if(svid){
            fprintf(stderr,"HASH request:%016lX [%016lX] (%04X) %d:%d\n",(*re)->hash.num,*((uint64_t*)(*re)->data),svid,(*re)->svid,(*re)->msid); // could be bad allignment
            //std::cerr << "REQUESTING MESSAGE from "<<svid<<" ("<<(*re)->svid<<":"<<(*re)->msid<<")\n";
            fprintf(stderr,"REQUESTING MESSAGE %04X:%08X from %04X\n",(*re)->svid,(*re)->msid,svid);
            deliver((*re),svid);}}
        //checking waiting messages
        tmp_msgs_.clear();
        wait_.lock();
	for(auto wa=wait_msgs_.begin();wa!=wait_msgs_.end();){
          if((*wa)->now<srvs_.now+BLOCKSEC && srvs_.nodes[(*wa)->svid].msid==(*wa)->msid-1){
            //std::cerr << "QUEUING MESSAGE "<<(*wa)->svid<<":"<<(*wa)->msid<<"\n";
            fprintf(stderr,"QUEUING MESSAGE %04X:%08X\n",(*wa)->svid,(*wa)->msid);
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
        //could concider a custom lock check against opening the same usr/XXXX.dat file
        //this will be needed later if we need to provide usr/XXXX.dat file for syncing
        check_.unlock();
        if(srvs_.nodes[msg->svid].status & SERVER_DBL){ // ignore messages from DBL server
          continue;} // no update
        //if(msg->status==MSGSTAT_VAL || srvs_.nodes[msg->svid].msid>=msg->msid)
        if(srvs_.nodes[msg->svid].msid>=msg->msid){
          //std::cerr <<"WARNING ignoring validation of old message "<<msg->svid<<":"<<msg->msid<<"<="<<srvs_.nodes[msg->svid].msid<<"\n";
          fprintf(stderr,"WARNING ignoring validation of old message %04X:%08X (<=%08X)\n",
            msg->svid,msg->msid,srvs_.nodes[msg->svid].msid);
          continue;}
	if(srvs_.nodes[msg->svid].msid!=msg->msid-1){ //assume only 1 validator per bank
          //std::cerr <<"WARNING postponing validation of future message "<<msg->svid<<":"<<msg->msid<<"!="<<srvs_.nodes[msg->svid].msid<<"+1\n";
          fprintf(stderr,"WARNING postponing validation of future message %04X:%08X (!=%08X+1)\n",
            msg->svid,msg->msid,srvs_.nodes[msg->svid].msid);
          wait_.lock();
          wait_msgs_.push_back(msg);
          wait_.unlock();
          continue;}
        bool valid=process_message(msg); //maybe ERROR should be also returned.
        if(valid){
          msg->print_text("VALID");
          msg->status=MSGSTAT_VAL;
          svid_.lock();
          node* nod=&srvs_.nodes[msg->svid];
          nod->msid=msg->msid;
          nod->mtim=msg->now;
          memcpy(nod->msha,msg->sigh,sizeof(hash_t));
          svid_msgs_[msg->svid]=msg;
          svid_.unlock();}
        else{
          //TODO, inform peer if peer==author;
          std::cerr<<"ERROR, have invalid message !!!\n";
          exit(-1);}
        if(!do_sync){
          //simulate delay, FIXME, remove after sync tests
          fprintf(stderr,"SLEEP 5s after validation\n");
          boost::this_thread::sleep(boost::posix_time::seconds(5.0*((float)random()/(float)RAND_MAX)));
          if(valid){
            update_candidates(msg);
            update(msg);}}
        else{
          if(!valid){
            std::cerr<<"ERROR, sync failed :-( :-(\n";
            msg->print("");
            exit(-1);}
          blk_.lock();
          blk_msgs_.erase(msg->hash.num);
          blk_.unlock();}}}
  }

  uint64_t make_ppi(uint32_t amsid,uint16_t abank,uint16_t bbank)
  { ppi_t ppi;
    ppi.v32[0]=amsid;
    ppi.v16[2]=abank;
    ppi.v16[3]=bbank;
    return(ppi.v64);
  }

  uint16_t ppi_abank(const uint64_t& ppi)
  { ppi_t *p=(ppi_t*)&ppi;
    return(p->v16[2]);
  }

  uint16_t ppi_bbank(const uint64_t& ppi)
  { ppi_t *p=(ppi_t*)&ppi;
    return(p->v16[3]);
  }

  bool remove_message(message_ptr msg) // log removing of message
  { uint8_t* p=(uint8_t*)msg->data+4+64+10;
    std::map<uint64_t,log_t> log;
    //TODO, load message from file
    uint32_t now=time(NULL);
    while(p<(uint8_t*)msg->data+msg->len){
      //uint8_t txstype=*p;
      usertxs utxs;
      assert(*p<TXSTYPE_INF);
      utxs.parse((char*)p);
      if(*p==TXSTYPE_CON){
        p+=utxs.size;
        continue;}
      uint32_t mpos=(p-msg->data);
      //if(msg->svid==opts_.svid){
      if(utxs.abank==opts_.svid){
        uint64_t key=((uint64_t)utxs.auser)<<32;
        key|=mpos;
        log_t alog;
        alog.time=now;
        alog.type=*p|0x4000; //outgoing|removed
        alog.node=utxs.bbank;
        alog.user=utxs.buser;
        alog.umid=utxs.amsid;
        alog.nmid=msg->msid; //can be overwritten with info
        alog.mpos=mpos; //can be overwritten with info
        alog.weight=utxs.tmass;
        log[key]=alog;}
      if((*p==TXSTYPE_PUT || *p==TXSTYPE_GET) && utxs.bbank==opts_.svid){
        uint64_t key=((uint64_t)utxs.buser)<<32;
        key|=mpos;
        log_t blog;
        blog.time=now;
        blog.type=*p|0x8000|0x4000; //incoming|removed
        blog.node=utxs.abank;
        blog.user=utxs.auser;
        blog.umid=utxs.amsid;
        blog.nmid=msg->msid; //can be overwritten with info
        blog.mpos=mpos; //can be overwritten with info
        blog.weight=utxs.tmass;
        log[key]=blog;}
      p+=utxs.size;}
    put_log(opts_.svid,log); //TODO, add loging options for multiple banks
    return(true);
  }

  bool undo_message(message_ptr msg)
  { char* p=(char*)msg->data+4+64+10;
    std::map<uint64_t,int64_t> txs_deposit;
    std::set<uint64_t> txs_get; //set lock / withdraw
    //TODO, load message from file
    bool old_bky=false;
    //bool old_usr=false;
    while(p<(char*)msg->data+msg->len){
      //char txstype=*p;
      usertxs utxs;
      assert(*p<TXSTYPE_INF);
      if(*p==TXSTYPE_PUT){
        utxs.parse(p);
        if(utxs.abank!=utxs.bbank){
          fprintf(stderr,"WARNING undoing put\n");
          union {uint64_t big;uint32_t small[2];} to;
          to.small[0]=utxs.buser; //assume big endian
          to.small[1]=utxs.bbank; //assume big endian
          txs_deposit[to.big]+=utxs.tmass;}
	p+=utxs.size;
	continue;}
      if(*p==TXSTYPE_GET){
        utxs.parse(p);
        uint64_t ppi=make_ppi(msg->msid,msg->svid,utxs.bbank);
        txs_get.insert(ppi);
	p+=utxs.size;
        fprintf(stderr,"WARNING undoing get\n");
	continue;}
      //this is added to undo file
      /*if(*p==TXSTYPE_USR && old_usr==false){ //reverse bank key change
        utxs.parse(p);
	uint32_t luser=utxs.nuser(p);
	if(luser>last_srvs_.nodes[msg->svid].users){
          srvs_.nodes[msg->svid].users=luser;
          old_usr=true;
          fprintf(stderr,"WARNING undoing new user addition (users:%08X)\n",luser);}
	p+=utxs.size;
	continue;}*/
      if(*p==TXSTYPE_BKY && old_bky==false){ //reverse bank key change
        old_bky=true;
        memcpy(srvs_.nodes[msg->svid].pk,utxs.opkey(p),32);
        if(msg->svid==opts_.svid){
          fprintf(stderr,"WARNING undoing local bank key change\n");}}
      p+=utxs.get_size(p);}
    uint64_t ppi=make_ppi(msg->msid,msg->svid,msg->svid);
    blk_.lock();
    //blk_bky.erase(ppi);
    blk_bnk.erase(ppi);
    //blk_put.erase(ppi);
    for(auto it=txs_get.begin();it!=txs_get.end();it++){
      blk_get.erase(*it);}
    blk_.unlock();
    deposit_.lock();
    for(auto it=txs_deposit.begin();it!=txs_deposit.end();it++){
      deposit[it->first]-=it->second;}
    deposit_.unlock();
    std::map<uint32_t,user_t> undo;
    uint32_t users=msg->load_undo(undo);
    //this could be a srvs_.function()
    fprintf(stderr,"UNDO USERS:%08X\n",users);
    if(users){
      if(srvs_.nodes[msg->svid].users!=users){
        fprintf(stderr,"WARNING undoing user additions (users back to:%08X)\n",users);}
      srvs_.nodes[msg->svid].users=users;}
    char filename[64];
    sprintf(filename,"usr/%04X.dat",msg->svid);
    int fd=open(filename,O_RDWR|O_CREAT,0644); //use memmory mapped file due to unordered transactions
    if(fd<0){
      //std::cerr<<"ERROR, failed to open bank register "<<msg->svid<<", fatal\n";
      fprintf(stderr,"ERROR, failed to open bank register %04X, fatal\n",msg->svid);
      exit(-1);}
    for(auto it=undo.begin();it!=undo.end();it++){
      user_t& u=it->second;
      fprintf(stderr,"UNDO:%04X:%08X m:%08X t:%08X s:%04X b:%04X u:%08X l:%08X r:%08X v:%016lX\n",
        msg->svid,it->first,u.msid,u.time,u.stat,u.node,u.user,u.lpath,u.rpath,u.weight);
      lseek(fd,it->first*sizeof(user_t),SEEK_SET);
      write(fd,&it->second,sizeof(user_t));}
    close(fd);
    return(true);
  }

  void log_broadcast(uint32_t path,char* p,int len,uint8_t* hash) // may need bankid and hash to verify signature
  { static uint32_t lpath=0;
    static int fd=-1;
    static boost::mutex log_;
    log_.lock();
    if(path!=lpath || fd<0){
      if(fd>=0){
        close(fd);}
      lpath=path;
      char filename[64];
      sprintf(filename,"%08X/bro.log",path);
      fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644); //TODO maybe O_TRUNC not needed
      if(fd<0){
        log_.unlock();
        fprintf(stderr,"ERROR, failed to open BROADCAST LOG %s\n",filename);
        return;}}
    write(fd,p,len);
    write(fd,hash,32);
    log_.unlock();
  }

  bool process_message(message_ptr msg)
  { char* p=(char*)msg->data+4+64+10;
    char filename[64];
    sprintf(filename,"usr/%04X.dat",msg->svid);
    int fd=open(filename,O_RDWR|O_CREAT,0644); //use memmory mapped file due to unordered transactions
    if(fd<0){
      //std::cerr<<"ERROR, failed to open bank register "<<msg->svid<<", fatal\n";
      fprintf(stderr,"ERROR, failed to open bank register %04X, fatal\n",msg->svid);
      exit(-1);}
    //sprintf(filename,"%08X/und/%04X.dat",srvs_.now,msg->svid);
    //int ud=open(filename,O_WRONLY|O_CREAT,0644);
    //if(ud<0){
    //  std::cerr<<"ERROR, failed to open bank undo "<<msg->svid<<", fatal\n";
    //  exit(-1);}
    std::map<uint64_t,log_t> log;
    std::map<uint32_t,user_t> changes;
    std::map<uint32_t,user_t> undo;
    std::map<uint32_t,int64_t> local_deposit;
    std::map<uint64_t,int64_t> txs_deposit;
    //std::map<uint64_t,std::vector<hash_s>> txs_bky; //change bank key
    //std::map<uint64_t,hash_s> txs_bky; //change bank key
    std::map<uint64_t,std::vector<uint32_t>> txs_bnk; //create new bank
    std::map<uint64_t,std::vector<get_t>> txs_get; //set lock / withdraw
    //std::map<uint64_t,std::vector<uint32_t>> txs_put; //cancel lock
    uint32_t users=srvs_.nodes[msg->svid].users;
    uint32_t ousers=users;
    uint32_t lpath=srvs_.now;
    uint32_t now=time(NULL); //needed for the log
    hash_t new_bky;
    bool set_bky=false;
    fprintf(stderr,"PROCESS MSG %04X:%08X\n",msg->svid,msg->msid);
    while(p<(char*)msg->data+msg->len){
      //char txstype=*p;
      uint32_t luser=0;
      uint16_t lnode=0;
      int64_t deduct=0;
      int64_t fee=0;
      /************* START PROCESSING **************/
      //user_t uempty; //needed for nothig :-(
      //user_t& usera=uempty;
      user_t* usera=NULL;
      usertxs utxs;
      if(!utxs.parse(p)){
        std::cerr<<"ERROR: failed to parse transaction\n";
        return(false);}
      utxs.print_head();
      if(*p==TXSTYPE_CON){
        //std::cerr<<"INFO: parsed CON transaction\n";
        p+=utxs.size;
        continue;}
      if(*p>=TXSTYPE_INF){
        std::cerr<<"ERROR: unknown transaction\n";
        return(false);}
      if(utxs.ttime>lpath+BLOCKSEC+5){ // remember that values are unsigned !
        //std::cerr<<"ERROR: time in the future block\n";
	fprintf(stderr,"ERROR: time in the future block time:%08X block:%08X limit %08X\n",
	  utxs.ttime,lpath,lpath+BLOCKSEC+5);
        return(false);}
      if(utxs.abank!=msg->svid && *p!=TXSTYPE_USR){
        std::cerr<<"ERROR: bad bank\n";
        utxs.print_head();
        return(false);}
      if(*p==TXSTYPE_USR){ // check lock first
        // signature checked later only for local users
        if(utxs.bbank!=msg->svid){
          std::cerr<<"ERROR: bad target bank\n";
          return(false);}
        //TXSTYPE_USER then the account was checked by the bank 
        //bank has created new account with (user,pkey) supplied
        //this transaction must be also reversible
	luser=utxs.nuser(p);
        char* npkey=utxs.npkey(p);
        if(luser>users){
          //std::cerr<<"ERROR: bad target user id "<<luser<<"\n";
          fprintf(stderr,"ERROR: bad target user id %08X\n",luser);
          return(false);}
	if(luser<users){ //1. check if overwriting was legal
          auto lu=changes.find(luser); // get user
          if(lu==changes.end()){
            user_t u;
            lseek(fd,luser*sizeof(user_t),SEEK_SET);
            read(fd,&u,sizeof(user_t));
            changes[luser]=u;
            undo[luser]=u;
            usera=&changes[luser];}
          else{
            usera=&lu->second;}
          if(usera->weight-TIME_FEE(lpath,usera->lpath)>=0){
            //std::cerr<<"ERROR: illegal overwriting of account "<<utxs.bbank<<":"<<luser<<"\n";
            fprintf(stderr,"WARNING, overwriting account %04X:%08X [weight:%016lX fee:%08X]\n",
              utxs.bbank,luser,usera->weight,TIME_FEE(lpath,usera->lpath));
            //fprintf(stderr,"ERROR: illegal overwriting of account %04X:%08X\n",utxs.bbank,luser);
            return(false);}}
        else{ //TODO, consider locking
          user_t u;
          users++;
          changes[luser]=u;
          //undo[luser]=u;
          usera=&changes[luser];}
        srvs_.init_user(*usera,msg->svid,luser,(utxs.abank==utxs.bbank?MIN_MASS:0),(uint8_t*)npkey,utxs.ttime);
        srvs_.put_user(*usera,msg->svid,luser);
        if(utxs.abank!=utxs.bbank){
          /* save log , only opts_.svid logging supported */
          if(utxs.abank==opts_.svid){
            uint32_t mpos=((uint8_t*)p-(uint8_t*)msg->data);
            uint64_t key=(uint64_t)utxs.auser<<32;
            key|=mpos;
            log_t alog;
            alog.time=now;
            alog.type=*p; //outgoing
            alog.node=utxs.bbank;
            alog.user=utxs.buser;
            alog.umid=utxs.amsid;
            alog.nmid=msg->msid; //can be overwritten with info
            alog.mpos=mpos; //can be overwritten with info
            alog.weight=utxs.tmass;
            log[key]=alog;}
          p+=utxs.size;
          continue;}}
      if(utxs.auser>=users){
        //std::cerr<<"ERROR, bad userid "<<utxs.abank<<":"<<utxs.auser<<"\n";
        fprintf(stderr,"ERROR, bad userid %04X:%08X\n",utxs.abank,utxs.auser);
        return(false);}
      auto au=changes.find(utxs.auser); // get user
      if(au==changes.end()){
        user_t u;
        lseek(fd,utxs.auser*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        changes[utxs.auser]=u;
        undo[utxs.auser]=u;
        usera=&changes[utxs.auser];}
      else{
        usera=&au->second;}
      //if(!offi_.get_user(usera,utxs.abank,utxs.auser)){
      //  std::cerr<<"ERROR: read user failed\n";
      //  return(false);}
      if(utxs.wrong_sig((uint8_t*)p,(uint8_t*)usera->hash,(uint8_t*)usera->pkey)){
        std::cerr<<"ERROR: bad signature\n";
        return(false);}
      if(usera->msid!=utxs.amsid){
        //std::cerr<<"ERROR: bad msid ("<<usera->msid<<"<>"<<utxs.amsid<<")\n";
        fprintf(stderr,"ERROR: bad msid %04X:%08X\n",usera->msid,utxs.amsid);
        return(false);}
      //if(usera->time>utxs.ttime){ //does not work because of _GET
      //  std::cerr<<"ERROR: bad transaction time ("<<usera->time<<">"<<utxs.ttime<<")\n";
      //  return(false);}
      //process transactions
      if(usera->time+2*LOCK_TIME<lpath && usera->user && usera->node && (usera->user!=utxs.auser || usera->node!=utxs.abank)){//check account lock
        if(*p!=TXSTYPE_PUT || utxs.abank!=utxs.bbank || utxs.auser!=utxs.buser || utxs.tmass!=0){
          std::cerr<<"ERROR: account locked, send 0 to yourself and wait for unlock\n";
          return(false);}}
        //if(usera->lpath>usera->time){
        //  std::cerr<<"ERROR: account unlock in porgress\n";
        //  return(false);}
        //when locked user,path and time are not modified
        //fee=TXS_LOCKCANCEL_FEE;
        //uint64_t ppi=make_ppi(msg->msid,msg->svid,utxs.abank); //FIXME, const ppi
        //txs_put[ppi].push_back(utxs.auser);
        //change to unlock
        //utxs.ttime=usera->time;
        //luser=usera->luser;
        //lnode=usera->lnode;
      else if(*p==TXSTYPE_BRO){
        log_broadcast(lpath,p,utxs.size,usera->hash);
        utxs.print_broadcast(p);
        fee=TXS_BRO_FEE(utxs.bbank)+TIME_FEE(lpath,usera->lpath);}
      else if(*p==TXSTYPE_PUT){
        if(utxs.tmass<0){ //sending info about negative values is allowed to fascilitate exchanges
          utxs.tmass=0;}
        //if(utxs.abank!=utxs.bbank && utxs.auser!=utxs.buser && !check_user(utxs.bbank,utxs.buser))
        if(!srvs_.check_user(utxs.bbank,utxs.buser)){
          // does not check if account closed [consider adding this slow check]
          //std::cerr<<"ERROR: bad target user ("<<utxs.bbank<<":"<<utxs.buser<<")\n";
          fprintf(stderr,"ERROR: bad target user %04X:%08X\n",utxs.bbank,utxs.buser);
          return(false);}
        if(utxs.abank==utxs.bbank){
          local_deposit[utxs.buser]+=utxs.tmass;}
        else{
          union {uint64_t big;uint32_t small[2];} to;
          to.small[0]=utxs.buser; //assume big endian
          to.small[1]=utxs.bbank; //assume big endian
          txs_deposit[to.big]+=utxs.tmass;}
        deduct=utxs.tmass;
        fee=TXS_PUT_FEE(utxs.tmass)+TIME_FEE(lpath,usera->lpath);}
      else if(*p==TXSTYPE_USR){ // this is local bank
        assert(utxs.abank==msg->svid);
        deduct=MIN_MASS;
        fee=TIME_FEE(lpath,usera->lpath);}
      else if(*p==TXSTYPE_BNK){ // we will get a confirmation from the network
        uint64_t ppi=make_ppi(msg->msid,msg->svid,utxs.abank);
        txs_bnk[ppi].push_back(utxs.auser);
        fee=TXS_BNK_FEE*TIME_FEE(lpath,usera->lpath);}
      else if(*p==TXSTYPE_GET){
        if(utxs.abank==utxs.bbank){
          //std::cerr<<"ERROR: bad bank ("<<utxs.bbank<<"), use PUT\n";
          fprintf(stderr,"ERROR: bad bank %04X, use PUT\n",utxs.bbank);
          return(false);}
        uint64_t ppi=make_ppi(msg->msid,msg->svid,utxs.bbank);
        get_t get;
        get.auser=utxs.auser;
        get.buser=utxs.buser;
        memcpy(get.pkey,usera->pkey,32);
        txs_get[ppi].push_back(get);
        fee=TXS_GET_FEE*TIME_FEE(lpath,usera->lpath);}
      else if(*p==TXSTYPE_KEY){
        memcpy(usera->pkey,utxs.key(p),32);
        fee=TXS_KEY_FEE*TIME_FEE(lpath,usera->lpath);}
      else if(*p==TXSTYPE_BKY){ // we will get a confirmation from the network
        if(utxs.auser){
          //std::cerr<<"ERROR: bad user ("<<utxs.auser<<") for this bank changes\n";
          fprintf(stderr,"ERROR: bad user %04X for this bank changes\n",utxs.auser);
          return(false);}
        if(memcmp(srvs_.nodes[msg->svid].pk,utxs.opkey(p),32)){
          std::cerr<<"ERROR: bad current key\n";
          return(false);}
	set_bky=true;
        memcpy(new_bky,utxs.key(p),sizeof(hash_t));
        //txs_bky[ppi].push_back(hash_s);
        //txs_bky[ppi]=hash_s;
        //blk_.lock();
        //blk_bky[ppi]=hash_s;
        //blk_.unlock();
        fee=TXS_BKY_FEE*TIME_FEE(lpath,usera->lpath);}
      else if(*p==TXSTYPE_STP){ // we will get a confirmation from the network
        assert(0); //TODO, not implemented
        fee=TXS_STP_FEE*TIME_FEE(lpath,usera->lpath);}
      if(deduct+fee+MIN_MASS>usera->weight){
        //std::cerr<<"ERROR: too low balance ("<<deduct<<"+"<<fee<<"+"<<MIN_MASS<<">"<<usera->weight<<")\n";
        fprintf(stderr,"ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
          deduct,fee,(uint64_t)MIN_MASS,usera->weight);
        return(false);}
      /* save log , only opts_.svid logging supported */
      uint32_t mpos=((uint8_t*)p-(uint8_t*)msg->data);
      if(msg->svid==opts_.svid){
        uint64_t key=(uint64_t)utxs.auser<<32;
        key|=mpos;
        log_t alog;
        alog.time=now;
        alog.type=*p; //outgoing
        alog.node=utxs.bbank;
        alog.user=utxs.buser;
        alog.umid=utxs.amsid;
        alog.nmid=msg->msid; //can be overwritten with info
        alog.mpos=mpos; //can be overwritten with info
        alog.weight=utxs.tmass;
        log[key]=alog;}
      if((*p==TXSTYPE_PUT || *p==TXSTYPE_GET) && utxs.bbank==opts_.svid){
        uint64_t key=(uint64_t)utxs.buser<<32;
        key|=mpos;
        log_t blog;
        blog.time=now;
        blog.type=*p|0x8000; //incoming
        blog.node=utxs.abank;
        blog.user=utxs.auser;
        blog.umid=utxs.amsid;
        blog.nmid=msg->msid; //can be overwritten with info
        blog.mpos=mpos; //can be overwritten with info
        blog.weight=utxs.tmass;
        log[key]=blog;}
      usera->msid++;
      usera->time=utxs.ttime;
      usera->node=lnode;
      usera->user=luser;
      usera->lpath=lpath;
      //convert message to hash
      uint8_t hash[32];
      SHA256_CTX sha256;
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,p,txslen[(int)*p]+64);
      SHA256_Final(hash,&sha256);
      //make newhash=hash(oldhash+newmessagehash);
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,usera->hash,32);
      SHA256_Update(&sha256,hash,32);
      SHA256_Final(usera->hash,&sha256);
      usera->weight+=local_deposit[utxs.auser]-deduct-fee;
      local_deposit[utxs.auser]=0;//to find changes[utxs.auser]
      p+=utxs.size;}
    //all transactions accepted
    //commit local changes
    user_t u;
    int offset=(char*)&u.weight-(char*)&u;
    //FIXME, lock here in case there is a bank read during sync, or accept errors when sending bank
    for(auto it=local_deposit.begin();it!=local_deposit.end();it++){
      auto jt=changes.find(it->first);
      if(jt!=changes.end()){
        jt->second.weight+=it->second;
        //assert(it->first<=srvs_.nodes[msg->svid].users);
        //if(it->first>=srvs_.nodes[msg->svid].users){ //create new user
        //  user_t u;
        //  memset(&u,0,sizeof(user_t));
        //  put_user(jt->second,msg->svid,it->first);} //will update node users
        //else{
        lseek(fd,jt->first*sizeof(user_t),SEEK_SET);
        write(fd,&jt->second,sizeof(user_t));}//}
      else{
        lseek(fd,it->first*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        undo[it->first]=u;
        u.weight+=it->second;
        lseek(fd,-sizeof(user_t)+offset,SEEK_CUR);
        write(fd,&u.weight,sizeof(uint64_t));}}
    //close(ud);
    close(fd);
    //save undo files
    msg->save_undo(undo,ousers); //message
    srvs_.save_undo(msg->svid,undo,ousers); //databank, will change srvs_.nodes[msg->svid].users
    //commit remote deposits
    //check for maximum deposits.size(), if too large, save old deposits and work on new ones;
    deposit_.lock();
    for(auto it=txs_deposit.begin();it!=txs_deposit.end();it++){
      deposit[it->first]+=it->second;}
    deposit_.unlock();
    //store block transactions
    blk_.lock();
    if(set_bky){
      memcpy(srvs_.nodes[msg->svid].pk,new_bky,32);
      if(msg->svid==opts_.svid){
        if(!srvs_.find_key(new_bky,skey)){
          fprintf(stderr,"ERROR, failed to change to new bank key, fatal!\n");
          exit(-1);}}}
    //blk_bky.insert(txs_bky.begin(),txs_bky.end());
    blk_bnk.insert(txs_bnk.begin(),txs_bnk.end());
    blk_get.insert(txs_get.begin(),txs_get.end());
    //blk_put.insert(txs_put.begin(),txs_put.end());
    blk_.unlock();
    put_log(opts_.svid,log); //TODO, add loging options for multiple banks
    return(true);
  }

  int open_bank(uint16_t svid) //
  { char filename[64];
    fprintf(stderr,"OPEN usr/%04X.dat",svid);
    sprintf(filename,"usr/%04X.dat",svid);
    int fd=open(filename,O_RDWR|O_CREAT,0644);
    if(fd<0){
      //std::cerr<<"ERROR, failed to open bank register "<<svid<<", fatal\n";
      fprintf(stderr,"ERROR, failed to open bank register %04X, fatal\n",svid);
      exit(-1);}
    return(fd);
    //sprintf(filename,"%08X/und/%04X.dat",srvs_.now,svid);
    //ud=open(filename,O_WRONLY|O_CREAT,0644);
    //if(ud<0){
    //  std::cerr<<"ERROR, failed to open bank undo "<<svid<<", fatal\n";
    //  exit(-1);}
  }

  void commit_block(std::set<uint16_t>& update) //assume single thread
  { //create new banks
    if(!blk_bnk.empty()){
      std::set<uint64_t> new_bnk; // list of available banks for takeover
      uint16_t peer=0;
      for(auto it=srvs_.nodes.begin()+1;it!=srvs_.nodes.end();it++,peer++){ // start with bank=1
        if(it->mtim+BANK_MIN_MTIME<srvs_.now && it->weight<BANK_MIN_WEIGHT){
          uint64_t bnk=it->weight<<16;
          bnk|=peer;
          new_bnk.insert(bnk);}}
      for(auto it=blk_bnk.begin();it!=blk_bnk.end();it++){
        uint16_t abank=ppi_abank(it->first);
	if(abank==opts_.svid){
//FIXME !!!
//FIXME, report to local office if this is the local bank
//FIXME !!!
        }
        int fd=open_bank(abank);
        update.insert(abank);
        for(auto tx=it->second.begin();tx!=it->second.end();tx++){
          user_t u;
          lseek(fd,(*tx)*sizeof(user_t),SEEK_SET);
          read(fd,&u,sizeof(user_t));
          uint16_t peer=0;
          // create new bank and new user
          if(!new_bnk.empty()){
            auto bi=new_bnk.begin();
            peer=(*bi)&0xffff;
            new_bnk.erase(bi);
            fprintf(stderr,"BANK, overwrite %04X\n",peer);
            srvs_.put_node(u,peer,*tx);}
          else if(srvs_.nodes.size()<BANK_MAX-1){
            peer=srvs_.add_node(u,*tx);
            fprintf(stderr,"BANK, add new bank %04X\n",peer);}
          else{
            close(fd);
            goto BLK_END;}
          update.insert(peer);
          u.node=peer;
          u.user=0;
          lseek(fd,-sizeof(user_t),SEEK_CUR);
          write(fd,&u,sizeof(user_t));}
        close(fd);}
      BLK_END:blk_bnk.clear();}

    //withdraw funds
    uint16_t svid=0;
    int fd=0;
    std::map<uint32_t,user_t> undo;
    for(auto it=blk_get.begin();it!=blk_get.end();it++){
      uint16_t abank=ppi_abank(it->first);
      uint16_t bbank=ppi_bbank(it->first);
      update.insert(abank);
      update.insert(bbank);
      if(bbank!=svid){
	if(svid==opts_.svid){
//FIXME !!!
//FIXME, report to local office if this is the local bank
//FIXME !!!
        }
        srvs_.save_undo(svid,undo,0);
        undo.clear();
        svid=bbank;
        fd=open_bank(svid);}
      if(bbank==opts_.svid){
//FIXME !!!
//FIXME, report to local office if this is the local bank
//FIXME !!!
      }
      union {uint64_t big;uint32_t small[2];} to;
      to.small[1]=abank; //assume big endian
      for(auto tx=it->second.begin();tx!=it->second.end();tx++){
        user_t u;
        lseek(fd,tx->buser*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        if(!memcmp(u.pkey,tx->pkey,32)){ //FIXME, add transaction fees processing
          if(u.weight<=0){
            continue;}
          if(u.node!=abank||u.user!=tx->auser){
            undo.emplace(tx->buser,u);
            u.node=abank;
            u.user=tx->auser;
            u.time=srvs_.now;}
          else if(u.time+2*LOCK_TIME>srvs_.now){
            continue;}
          else{ // withdraw all funds
            int64_t weight=u.weight;
            int64_t bfee=TIME_FEE(srvs_.now,u.rpath);
            weight-=bfee;
            int64_t afee=TXS_PUT_FEE(weight);
            weight-=afee;
            if(weight<=0){
              continue;}
            to.small[0]=tx->auser; //assume big endian
            deposit[to.big]+=weight;
            bank_fee[bbank]+=bfee;
            bank_fee[abank]+=afee;
            u.weight=0;
            u.rpath=srvs_.now;}} 
          lseek(fd,-sizeof(user_t),SEEK_CUR);
          write(fd,&u,sizeof(user_t));}}
    if(svid){
      if(svid==opts_.svid){
//FIXME !!!
//FIXME, report to local office if this is the local bank
//FIXME !!!
      }
      srvs_.save_undo(svid,undo,0);
      undo.clear();}
  }

  void commit_deposit(std::set<uint16_t>& update) //assume single thread
  { uint16_t lastsvid=0;
    int ud=0,fd=0;
    user_t u;
    int offset=(char*)&u.weight-(char*)&u;
    std::map<uint32_t,user_t> undo;
    for(auto it=deposit.begin();it!=deposit.end();it++){
      if(it->second==0){
        continue;}
      union {uint64_t big;uint32_t small[2];} to;
      to.big=it->first;
      uint32_t user=to.small[0];
      uint16_t svid=to.small[1];
      if(svid!=lastsvid){
        srvs_.save_undo(lastsvid,undo,0);
        undo.clear();
        //FIXME, should stop sync attempts on bank file, lock bank or accept sync errors
        char filename[64];
        if(fd){
          close(fd);}
        if(ud){
          close(ud);}
        lastsvid=svid;
	update.insert(svid);
        fprintf(stderr,"DEPOSIT to usr/%04X.dat\n",svid);
        sprintf(filename,"usr/%04X.dat",svid);
        fd=open(filename,O_RDWR,0644);
        if(fd<0){
          //std::cerr<<"ERROR, failed to open bank register "<<svid<<", fatal\n";
          fprintf(stderr,"ERROR, failed to open bank register %04X, fatal\n",svid);
          exit(-1);}
        sprintf(filename,"%08X/und/%04X.dat",srvs_.now,svid);
        ud=open(filename,O_WRONLY|O_CREAT,0644);
        if(ud<0){
          //std::cerr<<"ERROR, failed to open bank undo "<<svid<<", fatal\n";
          fprintf(stderr,"ERROR, failed to open bank undo %04X, fatal\n",svid);
          exit(-1);}}
//FIXME
//FIXME, report to local bank !!!
//FIXME
      lseek(fd,user*sizeof(user_t),SEEK_SET);
      read(fd,&u,sizeof(user_t));
      undo.emplace(user,u);
      u.weight+=it->second;
      lseek(fd,-sizeof(user_t)+offset,SEEK_CUR);
      write(fd,&u.weight,sizeof(uint64_t));}
      //uint64_t val;
      //lseek(fd,user*sizeof(user_t)+offset,SEEK_SET);
      //read(fd,&val,sizeof(uint64_t));
      //val+=it->second;
      //lseek(fd,-sizeof(uint64_t),SEEK_CUR);
      //write(fd,&val,sizeof(uint64_t));
    if(lastsvid){
      srvs_.save_undo(lastsvid,undo,0);
      undo.clear();}
    if(ud){
      close(ud);}
    if(fd){
      close(fd);}
    deposit.clear(); //remove deposits after commiting
  }

  bool accept_message(uint32_t lastmsid)
  { return(lastmsid<=msid_ && msid_==srvs_.nodes[opts_.svid].msid); //quick fix, in future lastmsid==msid_
  }

  void update_list(std::vector<uint64_t>& txs,std::vector<uint64_t>& dbl,uint16_t peer_svid)
  { txs_.lock();
    for(auto me=txs_msgs_.begin();me!=txs_msgs_.end();me++){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      if(me->second->status==MSGSTAT_VAL){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_PUT;
        h.dat[1]=me->second->hashval(peer_svid); //data[4+(peer_svid%64)]
        me->second->sent_erase(peer_svid);
        txs.push_back(h.num);}}
    txs_.unlock();
    dbl_.lock();
    for(auto me=dbl_msgs_.begin();me!=dbl_msgs_.end();me++){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      if(me->second->status==MSGSTAT_VAL){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_DBP;
        h.dat[1]=0;//me->second->data[4+(peer_svid%64)];
        me->second->sent_erase(peer_svid);
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
        fprintf(stderr,"WARNING msid==0 for svid:%04X (svid_miss)\n",(uint32_t)svid); //FIXME, this caused errors
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
          fprintf(stderr,"FATAL, could not find svid_have message :-( %016lX\n",h.num);
          exit(-1);}
        if(!memcmp(sigh,mi->second->sigh,sizeof(hash_t))){
          last_block_svid_msgs[it->first]=mi->second;
          break;}
        mi++;}}
    for(auto it=winner->svid_miss.begin();it!=winner->svid_miss.end();it++){
      uint16_t svid=it->first;
      uint32_t msid=it->second.msid;
      if(!msid){
        fprintf(stderr,"WARNING msid==0 for svid:%04X (svid_miss)\n",(uint32_t)svid); //FIXME, this caused errors
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
          fprintf(stderr,"FATAL, could not find svid_miss message :-( %016lX\n",h.num);
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
      nod->msid=it->second->msid; //TODO, it should be in the nodes already
      nod->mtim=it->second->now; //TODO, it should be in the nodes already
      memcpy(nod->msha,it->second->sigh,sizeof(hash_t)); //TODO, it should be in the nodes already
      ed25519_key2text(hash,it->second->sigh,sizeof(hash_t));
      fprintf(stderr,"DELTA: %d %d %.*s\n",it->first,it->second->msid,(int)(2*sizeof(hash_t)),hash);
      fprintf(fp,"%d %d %.*s\n",it->first,it->second->msid,(int)(2*sizeof(hash_t)),hash);}
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
    fprintf(stderr,"DELTA: 0 0 %.*s\n",(int)(2*sizeof(hash_t)),hash);
    fprintf(fp,"0 0 %.*s\n",(int)(2*sizeof(hash_t)),hash);
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
        //std::cerr << "ERASE message " << mi->second->svid << ":" << mi->second->msid << "\n";
        fprintf(stderr,"ERASE message %04X:%08X\n",mi->second->svid,mi->second->msid);
	mi->second->remove_undo();
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
        // maxmsid=srvs_.nodes[nsvid].msid; ERROR, must use last_block_svid_msgs
	auto lbsm=last_block_svid_msgs.find(nsvid);
        if(lbsm==last_block_svid_msgs.end()){
          maxmsid=0;}
        else{
          maxmsid=lbsm->second->msid;}
        fprintf(stderr,"RANGE %d %d %d\n",nsvid,minmsid,maxmsid);
        fprintf(fp,"RANGE %d %d %d\n",nsvid,minmsid,maxmsid);}
      //if(nmsid==0xffffffff && mi->second->msid<0xffffffff)  // remove messages from dbl_spend server
      if(maxmsid==0xffffffff && mi->second->msid<0xffffffff){ // remove messages from dbl_spend server
        //std::cerr << "REMOVE message " << nsvid << ":" << mi->second->msid << " later ! (DBL-spend server)\n";
        fprintf(stderr,"REMOVE message %04X:%08X later! (DBL-spend server)\n",nsvid,mi->second->msid);
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
          //std::cerr << "REMOVE message " << nsvid << ":" << mi->second->msid << " later !\n";
          fprintf(stderr,"REMOVE message %04X:%08X later!\n",nsvid,mi->second->msid);
          remove_msgs.push(mi->second);}
        else{
          if(mi->second->status&MSGSTAT_VAL){
            fprintf(stderr,"INVALIDATE message %04X:%08X later!\n",nsvid,mi->second->msid);
            invalidate_msgs.push(mi->second);}
          fprintf(stderr,"MOVING message %04X:%08X to %08X/\n",nsvid,mi->second->msid,srvs_.now+BLOCKSEC);
          mi->second->move(srvs_.now+BLOCKSEC);}}
      else{
        if(mi->second->len==message::header_length){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          fprintf(stderr,"IGNOR: %04X:%08X %.16s %016lX (%016lX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
//???, what about emsid ???
          continue;}
        //assert(mi->second->path==srvs_.now); // check, maybe path is not assigned yet
	if(mi->second->path!=srvs_.now){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          fprintf(stderr,"PATH?: %04X:%08X %.16s %016lX (%016lX) path=%08X srvs_.now=%08X\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first,mi->second->path,srvs_.now);}
        //if(nmsid!=0xffffffff && mi->second->msid!=emsid)
        if(maxmsid!=0xffffffff && mi->second->msid!=minmsid){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          fprintf(stderr,"?????: %04X:%08X %.16s %016lX (%016lX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
          //std::cerr << "ERROR, failed to read correct transaction order in block " << nsvid << ":" << mi->second->msid << "<>" << emsid << " (" << nmsid << ")\n";
          fprintf(stderr,"ERROR, failed to read correct transaction order in block %04x:%08X<>%08X (max:%08X)\n",
            nsvid,mi->second->msid,minmsid,maxmsid);
          for(auto mx=txs_msgs_.begin();mx!=txs_msgs_.end();mx++){
            fprintf(stderr,"%016lX\n",mx->first);}
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
        fprintf(stderr,"BLOCK: %d %d %.16s %016lX (%016lX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
        fprintf(fp,"%d %d %.*s\n",nsvid,mi->second->msid,(int)(2*sizeof(hash_t)),hash);}}
    txs_.unlock();
    //hash_s last_block_all_message;
    //message_shash(last_block_all_message.hash,last_block_all_msgs); // consider sending this hash to other peers
    //ed25519_key2text(hash,last_block_all_message.hash,sizeof(hash_t));
    //message_shash(srvs_.txshash,last_block_all_msgs); // consider sending this hash to other peers
    srvs_.txs=last_block_all_msgs.size();
    srvs_.txs_put(last_block_all_msgs); //FIXME, add dbl_ messages !!! FIXME FIXME !!!
    ed25519_key2text(hash,srvs_.txshash,sizeof(hash_t));
    fprintf(fp,"0 0 %.*s\n",(int)(2*sizeof(hash_t)),hash);
    fclose(fp);
    for(;!remove_msgs.empty();remove_msgs.pop()){
      auto mi=remove_msgs.top();
      if(mi->status & MSGSTAT_VAL){ // invalidate first
	undo_message(mi);
        mi->status &= ~MSGSTAT_VAL;}
//FIXME, make sure this message is not in other queue !!!
//FIXME, remove from all places or mark as removed :-/
      //std::cerr << "REMOVING message " << mi->svid << ":" << mi->msid << "\n";
      fprintf(stderr,"REMOVING message %04X:%08X\n",mi->svid,mi->msid);
      //srvs_.nodes[mi->svid].msid=mi->msid-1; // assuming message exists
//FIXME, update peers.svid_msid_new !!!
      remove_message(mi); //FIXME add this , must log message removal
      mi->remove();}
    check_.lock();
    for(;!invalidate_msgs.empty();invalidate_msgs.pop()){
      auto mi=invalidate_msgs.top();
      //std::cerr << "INVALIDATING message " << mi->svid << ":" << mi->msid << "\n";
      ed25519_key2text(hash,mi->sigh,sizeof(hash_t));
      fprintf(stderr,"INVALIDATING %04X:%08X %.16s %016lX %08X/\n",mi->svid,mi->msid,hash,mi->hash.num,mi->path);
      undo_message(mi);
      mi->status &= ~MSGSTAT_VAL;
      //srvs_.nodes[mi->svid].msid=mi->msid-1; // assuming message exists
      check_msgs_.push_front(mi);}
    check_.unlock();
    std::set<uint16_t> update;
    for(auto mi=commit_msgs.begin();mi!=commit_msgs.end();mi++){
      update.insert((*mi)->svid);
      //std::cerr << "COMMITING message " << (*mi)->svid << ":" << (*mi)->msid << "\n";
      fprintf(stderr,"COMMITING message %04X:%08X\n",(*mi)->svid,(*mi)->msid);}
    commit_block(update); // process bkn and get transactions
    commit_deposit(update);
//TODO, save current account status
    std::cerr << "UPDATE accounts\n";
//TODO
//TODO try running update_nodehash in background while working on a new block
//TODO
    for(auto it=update.begin();it!=update.end();it++){
      assert(*it<srvs_.nodes.size());
      //consider locking
      srvs_.update_nodehash(*it);} //also calculates weight of the node

    srvs_.finish(); //FIXME, add locking
    last_srvs_=srvs_; // consider not making copies of nodes
    memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
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
    hs.do_sync=do_sync;
    //ed25519_printkey(skey,32);
    //ed25519_printkey(pkey,32);
    hash_t empty;
    message_ptr msg(new message(MSGTYPE_INI,(uint8_t*)&hs,(int)sizeof(handshake_t),opts_.svid,msid_,skey,pkey,empty));
    return(msg);
  }

  uint32_t write_message(std::string line) // assume single threaded
  { static boost::mutex mtx_;
    mtx_.lock(); //FIXME, probably no lock needed
    int msid=++msid_; // can be atomic
    mtx_.unlock();
    writemsid(); //FIXME we should save the message before updating the message counter
    message_ptr msg(new message(MSGTYPE_TXS,(uint8_t*)line.c_str(),(int)line.length(),opts_.svid,msid,skey,pkey,srvs_.nodes[opts_.svid].msha));
    if(!txs_insert(msg)){
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    return(msid_);
    //update(msg);
  }

  void write_candidate(const hash_s& last_message)
  { do_vote=0;
    hash_t empty;
    message_ptr msg(new message(MSGTYPE_CND,last_message.hash,sizeof(hash_t),opts_.svid,srvs_.now,skey,pkey,empty)); //FIXME, consider msid=0 ???
//FIXME, is hash ok ?
    if(!cnd_insert(msg)){
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    std::cerr << "SENDING candidate\n";
    update(msg); // update peers even if we are not an elector
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
    hash_t empty;
    message_ptr msg(new message(MSGTYPE_BLK,(uint8_t*)&head,sizeof(header_t),opts_.svid,head.now,skey,pkey,empty));
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
    while(1){
      uint32_t now=time(NULL);
      if(missing_msgs_.size()){
        fprintf(stderr,"CLOCK: %02lX (check:%d wait:%d peers:%d hash:%8X) (miss:%d:%016lX)\n",
          ((long)(srvs_.now+BLOCKSEC)-(long)now),(int)check_msgs_.size(),(int)wait_msgs_.size(),
          (int)peers_.size(),(uint32_t)*((uint32_t*)srvs_.nowhash),(int)missing_msgs_.size(),
          missing_msgs_.begin()->first);}
      else{
        fprintf(stderr,"CLOCK: %02lX (check:%d wait:%d peers:%d hash:%8X)\n",
          ((long)(srvs_.now+BLOCKSEC)-(long)now),(int)check_msgs_.size(),
          (int)wait_msgs_.size(),(int)peers_.size(),(uint32_t)*((uint32_t*)srvs_.nowhash));}
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
          fprintf(stderr,"LAST HASH put %.*s\n",(int)(2*SHA256_DIGEST_LENGTH),hash);
          deliver(put_msg); // sets BLOCK_MODE for peers
        candidate_ptr c_ptr(new candidate());
        save_candidate(cand,c_ptr);
        prepare_poll(); // sets do_vote
        do_block=1;
        do_validate=1;
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
      boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
  }

  bool break_silence(uint32_t now,std::string& message) // will be obsolete if we start tolerating empty blocks
  { static uint32_t do_hallo=0;
    if(!do_block && do_hallo!=srvs_.now && now-srvs_.now>(uint32_t)(BLOCKSEC/4+ opts_.svid*VOTE_DELAY) && svid_msgs_.size()<MIN_MSGNUM){
      std::cerr << "SILENCE, sending void message due to silence\n";
      usertxs txs(TXSTYPE_CON,0,0,now);
      message.append((char*)txs.data,txs.size);
      do_hallo=srvs_.now;
      return(true);}
    return(false);
  }

//struct hash_cmp
//{ bool operator()(const hash_s& i,const hash_s& j) const {int k=memcmp(i.hash,j.hash,sizeof(hash_t)); return(k<0);}
//};

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

  //void officep(office& o)
  //{ ofip=&o;
  //}
  //void start_office();

  void lock_user(uint32_t cuser)
  { ulock_[cuser & 0xff].lock();
  }

  void unlock_user(uint32_t cuser)
  { ulock_[cuser & 0xff].unlock();
  }


  void join(peer_ptr participant);
  void leave(peer_ptr participant);
  void disconnect(uint16_t svid);
  int duplicate(peer_ptr participant);
  void deliver(message_ptr msg);
  int deliver(message_ptr msg,uint16_t svid);
  void update(message_ptr msg);
  void svid_msid_rollback(message_ptr msg);
  void start_accept();
  void handle_accept(peer_ptr new_peer,const boost::system::error_code& error);
  void connect(std::string peer_address);
  void fillknown(message_ptr msg);
  void get_more_headers(uint32_t now,uint8_t* nowhash);

  //FIXME, move this to servers.hpp
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
  office* ofip;
  uint8_t *pkey; //used by office/client to create BKY transaction
private:
  hash_t skey;
  boost::mutex ulock_[0x100];
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
  std::map<uint16_t,uint64_t> electors;
  uint64_t votes_max;
  int do_vote;
  int do_block;
  //int vip_ok;
  //int vip_no;
  //std::map<uint16_t,uint32_t> svid_msid; //TODO, maybe not used
  std::map<uint64_t,int64_t> deposit;
  boost::mutex deposit_;
  //std::map<uint64_t,hash_s> blk_bky; //change bank key
  std::map<uint64_t,std::vector<uint32_t>> blk_bnk; //create new bank
  std::map<uint64_t,std::vector<get_t>> blk_get; //set lock / withdraw
  //std::map<uint64_t,std::vector<uint32_t>> blk_put; //cancel lock
  std::vector<int64_t> bank_fee;
};

#endif // SERVER_HPP
