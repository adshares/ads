#ifndef CLIENT_HPP
#define CLIENT_HPP

//this could all go to the office class and we could use just the start() function

class client : public boost::enable_shared_from_this<client>
{
public:
  client(boost::asio::io_service& io_service,office& offi)
    : socket_(io_service),
      offi_(offi),
      addr(""),
      port(""),
      buf(NULL),
      len(0),
      more(0)
  {  //read_msg_ = boost::make_shared<message>();
    std::cerr<<"OFFICER ready ("<<offi_.svid<<")\n";
  }

  ~client()
  { std::cerr << "Client left " << addr << ":" << port << "\n"; //LESZEK
    free(buf);
    buf=NULL;
  }

  boost::asio::ip::tcp::socket& socket()
  { return socket_;
  }

  void start() //TODO consider providing a local user file pointer
  { addr = socket_.remote_endpoint().address().to_string();
    port = std::to_string(socket_.remote_endpoint().port());
    std::cerr << "Client entered " << addr << ":" << port << "\n";
    buf=(char*)std::malloc(txslen[TXSTYPE_MAX]+64+128);
    boost::asio::async_read(socket_,boost::asio::buffer(buf,1),
      boost::bind(&client::handle_read_txstype,shared_from_this(),boost::asio::placeholders::error));
    return;
  }

  void handle_read_txstype(const boost::system::error_code& error)
  { if(error){
      std::cerr<<"ERROR: read txstype error\n";
      offi_.leave(shared_from_this());
      return;}
    if(*buf>=TXSTYPE_MAX){
      std::cerr<<"ERROR: read txstype failed\n";
      offi_.leave(shared_from_this());
      return;}
    if(*buf==TXSTYPE_KEY){ // additional confirmation signature
      more=64;}
    len=txslen[(int)*buf]+64+more;
    //std::cerr << "Client txstype " << addr << ":" << port << "\n";
    boost::asio::async_read(socket_,boost::asio::buffer(buf+1,len-1),
      boost::bind(&client::handle_read_txs,shared_from_this(),boost::asio::placeholders::error));
    return;
  }

  void handle_read_txs(const boost::system::error_code& error)
  { if(error){
      std::cerr<<"ERROR: read txs error\n";
      offi_.leave(shared_from_this());
      return;}
    utxs.parse(buf);
    utxs.print_head();
    if(*buf==TXSTYPE_BRO){
      buf=(char*)std::realloc(buf,len+utxs.bbank);
      //std::cerr << "Client more " << addr << ":" << port << "\n";
      boost::asio::async_read(socket_,boost::asio::buffer(buf+len,utxs.bbank),
        boost::bind(&client::handle_read_more,shared_from_this(),boost::asio::placeholders::error));
      return;}
    if(*buf==TXSTYPE_MPT){
      buf=(char*)std::realloc(buf,len+utxs.bbank*(6+8));
      //std::cerr << "Client more " << addr << ":" << port << "\n";
      boost::asio::async_read(socket_,boost::asio::buffer(buf+len,utxs.bbank*(6+8)),
        boost::bind(&client::handle_read_more,shared_from_this(),boost::asio::placeholders::error));
      return;}
    //offi_.lock_user(utxs.auser); //needed to prevent 2 msgs from a user with same msid
    parse();
    //offi_.unlock_user(utxs.auser);
    offi_.leave(shared_from_this());
  }

  void handle_read_more(const boost::system::error_code& error)
  { if(error){
      std::cerr<<"ERROR: read more error\n";
      offi_.leave(shared_from_this());
      return;}
    //offi_.lock_user(utxs.auser); //needed to prevent 2 msgs from a user with same msid
    parse();
    //offi_.unlock_user(utxs.auser);
    offi_.leave(shared_from_this());
  }

  void parse()
  { started=time(NULL);
    //std::cerr << "Client parse " << addr << ":" << port << "\n";
    uint32_t lpath=started-started%BLOCKSEC;
    uint32_t luser=0;
    uint16_t lnode=0;
    int64_t deposit=-1; // if deposit=0 inform target
    int64_t deduct=0;
    int64_t fee=0;
    user_t usera;
    int32_t diff=utxs.ttime-started;
    //TODO, should not allow same user multiple times in mpt otheriwse the log becomes unclear
    std::vector<uint16_t> mpt_bank; // for MPT to local bank
    std::vector<uint32_t> mpt_user; // for MPT to local bank
    std::vector< int64_t> mpt_mass; // for MPT to local bank
    msid=0;
    mpos=0;

    offi_.lock_user(utxs.auser); //needed to prevent 2 msgs from a user with same msid
//FIXME, read the rest ... add additional signatures
    //consider adding a max txs limit per user
    if(!offi_.get_user(usera,utxs.abank,utxs.auser)){
      std::cerr<<"ERROR: read user failed\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(utxs.wrong_sig((uint8_t*)buf,(uint8_t*)usera.hash,(uint8_t*)usera.pkey)){
      std::cerr<<"ERROR: bad signature\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(*buf==TXSTYPE_KEY && utxs.wrong_sig2((uint8_t*)buf)){
      std::cerr<<"ERROR: bad second signature\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(diff>2 && *buf!=TXSTYPE_BLG){
      std::cerr<<"ERROR: time in the future ("<<diff<<">2s)\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(*buf==TXSTYPE_BKY){
      hash_t skey;
      if(!offi_.find_key((uint8_t*)utxs.key(buf),skey)){
        std::cerr<<"ERROR: bad second signature\n";
        offi_.unlock_user(utxs.auser);
        return;}}
    if(*buf==TXSTYPE_INF){ // this is special, just local info
      if((abs(diff)>2)){
        std::cerr<<"ERROR: high time difference ("<<diff<<">2s)\n";
        offi_.unlock_user(utxs.auser);
        return;}
//FIXME, read data also from server
//FIXME, if local account locked, check if unlock was successfull based on time passed after change
      if(utxs.abank!=offi_.svid && utxs.bbank!=offi_.svid){
        std::cerr<<"ERROR: bad bank for INF\n";
        offi_.unlock_user(utxs.auser);
        return;}
      std::cerr<<"SENDING user info ("<<utxs.bbank<<":"<<utxs.buser<<")\n";
      user_t userb;
      if(utxs.abank!=utxs.bbank || utxs.auser!=utxs.buser){
        if(!offi_.get_user(userb,utxs.bbank,utxs.buser)){
          std::cerr<<"FAILED to get local user info ("<<utxs.bbank<<":"<<utxs.buser<<")\n";
        offi_.unlock_user(utxs.auser);
          return;}
        boost::asio::write(socket_,boost::asio::buffer(&userb,sizeof(user_t)));}
      else{
        boost::asio::write(socket_,boost::asio::buffer(&usera,sizeof(user_t)));}
      if(!offi_.get_user_global(userb,utxs.bbank,utxs.buser)){
        std::cerr<<"FAILED to get global user info ("<<utxs.bbank<<":"<<utxs.buser<<")\n";
        offi_.unlock_user(utxs.auser);
        return;}
      boost::asio::write(socket_,boost::asio::buffer(&userb,sizeof(user_t)));
      offi_.unlock_user(utxs.auser);
      return;}
    //if(utxs.abank!=offi_.svid && *buf!=TXSTYPE_USR){
    if(utxs.abank!=offi_.svid){
      std::cerr<<"ERROR: bad bank\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(*buf==TXSTYPE_LOG){
      //if(utxs.abank!=offi_.svid){
      //  std::cerr<<"ERROR: bad bank for LOG\n";
      //  return;}
      boost::asio::write(socket_,boost::asio::buffer(&usera,sizeof(user_t)));
      std::string slog;
      if(!offi_.get_log(utxs.abank,utxs.auser,utxs.ttime,slog)){
        std::cerr<<"ERROR: get log failed\n";
        offi_.unlock_user(utxs.auser);
        return;}
      fprintf(stderr,"SENDING user %04X:%08X log [size:%lu]\n",utxs.abank,utxs.auser,slog.size());
      boost::asio::write(socket_,boost::asio::buffer(slog.c_str(),slog.size()));
      offi_.unlock_user(utxs.auser);
      return;}
    if(*buf==TXSTYPE_BLG){
      uint32_t head[2];
      uint32_t &path=head[0];
      uint32_t &size=head[1];
      uint32_t lpath=offi_.last_path();
      if(!utxs.ttime){
        path=lpath;}
      else{
        path=utxs.ttime-utxs.ttime%BLOCKSEC;
        if(path>=lpath){
          fprintf(stderr,"ERROR, broadcast %08X not ready (>=%08X)\n",path,lpath);
          offi_.unlock_user(utxs.auser);
          return;}}
      //FIXME, report only completed broadcast files (<=last_path())
      char filename[64];
      sprintf(filename,"blk/%03X/%05X/bro.log",path>>20,path&0xFFFFF);
      int fd=open(filename,O_RDONLY);
      if(fd<0){
        size=0;
        boost::asio::write(socket_,boost::asio::buffer(head,2*sizeof(uint32_t)));
        fprintf(stderr,"SENDING broadcast log %08X [empty]\n",path);
        offi_.unlock_user(utxs.auser);
        return;}
      struct stat sb;
      fstat(fd,&sb);
      if(sb.st_size>MAX_BLG_SIZE){
        sb.st_size=MAX_BLG_SIZE;}
      if(sb.st_size>MESSAGE_CHUNK){ // change to MESSAGE_TOO_LONG or similar
        size=MESSAGE_CHUNK;}
      else{
        size=sb.st_size;}
      fprintf(stderr,"SENDING broadcast log %08X [len:%d]\n",path,size);
      if(!size){
        boost::asio::write(socket_,boost::asio::buffer(head,2*sizeof(uint32_t)));
        close(fd);
        offi_.unlock_user(utxs.auser);
        return;}
      char buf[2*sizeof(uint32_t)+size];
      memcpy(buf,head,2*sizeof(uint32_t));
      for(uint32_t pos=0;pos<sb.st_size;){
        int len=read(fd,buf+2*sizeof(uint32_t),size);
        if(len<=0){
          close(fd);
          fprintf(stderr,"ERROR, failed to read BROADCAST LOG %s [size:%08X,pos:%08X]\n",filename,size,pos);
          offi_.unlock_user(utxs.auser);
          return;}
        if(!pos){
          boost::asio::write(socket_,boost::asio::buffer(buf,2*sizeof(uint32_t)+len));}
        else{
          boost::asio::write(socket_,boost::asio::buffer(buf+2*sizeof(uint32_t),len));}
        pos+=len;}
      close(fd);
      offi_.unlock_user(utxs.auser);
      return;}
    if(usera.msid!=utxs.amsid){
      std::cerr<<"ERROR: bad msid ("<<usera.msid<<"<>"<<utxs.amsid<<")\n";
      offi_.unlock_user(utxs.auser);
      return;}
    //if(usera.time>utxs.ttime){ //does not work because of _GET
    //  std::cerr<<"ERROR: bad transaction time ("<<usera.time<<">"<<utxs.ttime<<")\n";
    //  return;}
    //include additional 2FA later

    //commit trasaction
    if(usera.time+LOCK_TIME<lpath && usera.user && usera.node && (usera.user!=utxs.auser || usera.node!=utxs.abank)){//check account lock
      if(*buf!=TXSTYPE_PUT || utxs.abank!=utxs.bbank || utxs.auser!=utxs.buser || utxs.tmass!=0){
        std::cerr<<"ERROR: account locked, send 0 to yourself and wait for unlock\n";
        offi_.unlock_user(utxs.auser);
        return;}
      if(usera.lpath>usera.time){
        std::cerr<<"ERROR: account unlock in porgress\n";
        offi_.unlock_user(utxs.auser);
        return;}
      //when locked user,path and time are not modified
      //fee=TXS_LOCKCANCEL_FEE; no fee needed because only 1 cancel permitted
      utxs.ttime=usera.time;//lock time not changed
      //need to read data with _INF to confirm unlock !!!
      luser=usera.user;
      lnode=usera.node;}
    else if(*buf==TXSTYPE_BRO){
      fee=TXS_BRO_FEE(utxs.bbank)+TIME_FEE(lpath,usera.lpath);}
    else if(*buf==TXSTYPE_PUT){
      if(utxs.tmass<0){ //sending info about negative values is allowed to fascilitate exchanges
        utxs.tmass=0;}
      //if(utxs.abank!=utxs.bbank && utxs.auser!=utxs.buser && !check_user(utxs.bbank,utxs.buser)){
      if(!offi_.check_user(utxs.bbank,utxs.buser)){
        // does not check if account closed [consider adding this slow check]
	std::cerr<<"ERROR: bad target user ("<<utxs.bbank<<":"<<utxs.buser<<")\n";
        offi_.unlock_user(utxs.auser);
        return;}
      deposit=utxs.tmass;
      deduct=utxs.tmass;
      fee=TXS_PUT_FEE(utxs.tmass)+TIME_FEE(lpath,usera.lpath);}
    else if(*buf==TXSTYPE_MPT){
      char* tbuf=utxs.toaddresses(buf);
      //utxs.print_toaddresses(buf,utxs.bbank);
      utxs.tmass=0;
      mpt_bank.reserve(utxs.bbank);
      mpt_user.reserve(utxs.bbank);
      mpt_mass.reserve(utxs.bbank);
      std::set<uint64_t> out;
      union {uint64_t big;uint32_t small[2];} to;
      to.small[1]=0;
      for(int i=0;i<utxs.bbank;i++,tbuf+=6+8){
        uint32_t& tuser=to.small[0];
        uint32_t& tbank=to.small[1];
        //uint16_t tbank;
        //uint32_t tuser;
         int64_t tmass;
        memcpy(&tbank,tbuf+0,2);
        memcpy(&tuser,tbuf+2,4);
        memcpy(&tmass,tbuf+6,8);
        if(tmass<=0){ //only positive non-zero values allowed
          std::cerr<<"ERROR: only positive non-zero transactions allowed in MPT\n";
          offi_.unlock_user(utxs.auser);
          return;}
        if(out.find(to.big)!=out.end()){
          fprintf(stderr,"ERROR: duplicate target: %04X:%08X\n",tbank,tuser);
          offi_.unlock_user(utxs.auser);
          return;}
        if(!offi_.check_user((uint16_t)tbank,tuser)){
          // does not check if account closed [consider adding this slow check]
          std::cerr<<"ERROR: bad target user ("<<tbank<<":"<<tuser<<")\n";
          offi_.unlock_user(utxs.auser);
          return;}
        out.insert(to.big);
        mpt_bank.push_back((uint16_t)tbank);
        mpt_user.push_back(tuser);
        mpt_mass.push_back(tmass);
        utxs.tmass+=tmass;}
      deposit=utxs.tmass;
      deduct=utxs.tmass;
      fee=TXS_MPT_FEE(utxs.tmass,utxs.bbank)+TIME_FEE(lpath,usera.lpath);}
    else if(*buf==TXSTYPE_USR){
      //if(utxs.bbank!=offi_.svid){
      //  std::cerr<<"ERROR: bad target bank ("<<utxs.bbank<<")\n";
      //  offi_.unlock_user(utxs.auser);
      //  return;}
      deduct=MIN_MASS;
      fee=TXS_USR_FEE; //TIME_FEE(lpath,usera.lpath);
      if(deduct+fee+MIN_MASS>usera.weight){ //check in advance before creating new user
        std::cerr<<"ERROR: too low balance ("<<deduct<<"+"<<fee<<"+"<<MIN_MASS<<">"<<usera.weight<<")\n";
        offi_.unlock_user(utxs.auser);
        return;}
      if(utxs.bbank!=offi_.svid){
        uint32_t now=time(NULL);
        if(now%BLOCKSEC>BLOCKSEC/2){
          fprintf(stderr,"ERROR: bad timing for remote account request, try after %d seconds\n",
            BLOCKSEC-now%BLOCKSEC);
          offi_.unlock_user(utxs.auser);
          return;}
        bzero(buf+txslen[(int)*buf]+64+0,4+32);} //FIXME, remove 32 bytes
      else{
        uint32_t nuser=offi_.add_user(utxs.abank,usera.pkey,utxs.ttime,utxs.auser);
        if(!nuser){
          std::cerr<<"ERROR: failed to open account\n";
          offi_.unlock_user(utxs.auser);
          return;}
        memcpy(buf+txslen[(int)*buf]+64+0,&nuser,4);
        memcpy(buf+txslen[(int)*buf]+64+4,usera.pkey,32); //FIXME, this data is not needed !!!
        lnode=0;
        luser=nuser;}}
      /*if(utxs.abank!=offi_.svid){
      //if(utxs.abank!=offi_.svid && !offi_.try_account((hash_s*)usera.pkey)){
      //  std::cerr<<"ERROR: failed to open account (pkey known)\n";
      //  offi_.unlock_user(utxs.auser);
      //  return;}
      // add_account ...

        //FIXME, check if original bank is in our whitelist
        offi_.add_msg((uint8_t*)buf,utxs.size,0,msid,mpos); //TODO, could return pointer to file
        offi_.add_account((hash_s*)usera.pkey,nuser); //blacklist
        user_t userb;
        offi_.get_user(userb,utxs.bbank,nuser); // send userb
//FIXME, send also transaction info
        boost::asio::write(socket_,boost::asio::buffer(&userb,sizeof(user_t)));
        offi_.unlock_user(utxs.auser);
        return;}}*/
    else if(*buf==TXSTYPE_BNK){ // we will get a confirmation from the network
      deduct=BANK_MIN_MASS;
      fee=TXS_BNK_FEE*TIME_FEE(lpath,usera.lpath);} 
    else if(*buf==TXSTYPE_GET){ // we will get a confirmation from the network
      if(utxs.abank==utxs.bbank){
	std::cerr<<"ERROR: bad bank ("<<utxs.bbank<<"), use PUT\n";
        offi_.unlock_user(utxs.auser);
        return;}
//FIXME, check second user and stop transaction if GET is pednig
      fee=TXS_GET_FEE*TIME_FEE(lpath,usera.lpath);}
    else if(*buf==TXSTYPE_KEY){
      memcpy(usera.pkey,utxs.key(buf),32);
      fee=TXS_KEY_FEE*TIME_FEE(lpath,usera.lpath);} 
    else if(*buf==TXSTYPE_BKY){ // we will get a confirmation from the network
      if(utxs.auser){
	std::cerr<<"ERROR: bad user ("<<utxs.auser<<") for this bank changes\n";
        offi_.unlock_user(utxs.auser);
        return;}
      //buf=(char*)std::realloc(buf,utxs.size);
      fee=TXS_BKY_FEE*TIME_FEE(lpath,usera.lpath);} 
    else if(*buf==TXSTYPE_STP){ // we will get a confirmation from the network
      assert(0); //TODO, not implemented later
      fee=TXS_STP_FEE*TIME_FEE(lpath,usera.lpath);} 
    if(deduct+fee+MIN_MASS>usera.weight){
      std::cerr<<"ERROR: too low balance ("<<deduct<<"+"<<fee<<"+"<<MIN_MASS<<">"<<usera.weight<<")\n";
      offi_.unlock_user(utxs.auser);
      return;}
    //send message
    //commit bank key change
    if(*buf==TXSTYPE_BKY){ // commit key change
      memcpy(utxs.opkey(buf),offi_.pkey,32);
      memcpy(offi_.pkey,utxs.key(buf),32);}
    //offi_.add_msg(buf,txslen[(int)*buf]+64+extralen,fee); //TODO, could return pointer to file
    offi_.add_msg((uint8_t*)buf,utxs.size,fee,msid,mpos); //TODO, could return pointer to file
    if(!msid||!mpos){
      std::cerr<<"ERROR: message submission failed ("<<msid<<","<<mpos<<")\n";
      offi_.unlock_user(utxs.auser);
      return;}
    //commit changes
    usera.msid++;
    usera.time=utxs.ttime;
    usera.node=lnode;
    usera.user=luser;
    usera.lpath=lpath;
    //convert message to hash (use signature as input)
    uint8_t hash[32];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,utxs.get_sig(buf),64);
    SHA256_Final(hash,&sha256);
    //make newhash=hash(oldhash+newmessagehash);
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,usera.hash,32);
    SHA256_Update(&sha256,hash,32);
    SHA256_Final(usera.hash,&sha256);
    offi_.set_user(utxs.auser,usera,deduct+fee);
    //log
    uint32_t now=time(NULL);
    if(*buf==TXSTYPE_MPT && mpt_user.size()>0){
      int end=mpt_user.size();
      std::map<uint64_t,log_t> log;
      for(int i=0;i<end;i++){
        uint64_t key=((uint64_t)utxs.auser)<<32;
        key|=i;
        log_t alog;
        alog.time=now;
        alog.type=*buf;
        alog.node=mpt_bank[i];
        alog.user=mpt_user[i];
        alog.umid=utxs.amsid;
        alog.nmid=msid; //can be overwritten with info
        alog.mpos=mpos; //can be overwritten with info
        alog.weight=mpt_mass[i];
	bzero(alog.info,32);
	log[key]=alog;}
      offi_.put_log(offi_.svid,log);} // could be processed 
    else{
      log_t alog;
      alog.time=now;
      alog.type=*buf;
      alog.node=utxs.bbank;
      alog.user=utxs.buser;
      alog.umid=utxs.amsid;
      alog.nmid=msid; //can be overwritten with info
      alog.mpos=mpos; //can be overwritten with info
      alog.weight=utxs.tmass;
      memcpy(alog.info,utxs.tinfo,32);
      offi_.put_log(offi_.svid,utxs.auser,alog);}
    offi_.unlock_user(utxs.auser);
    if(*buf==TXSTYPE_MPT && mpt_user.size()>0){
      int end=mpt_user.size();
      for(int i=0;i<end;i++){
        if(mpt_bank[i]==offi_.svid){
          log_t blog;
          blog.time=now;
          blog.type=*buf|0x8000; //incoming
          blog.node=utxs.abank;
          blog.user=utxs.auser;
          blog.umid=utxs.amsid;
          blog.nmid=msid; //can be overwritten with info
          blog.mpos=mpos; //can be overwritten with info
          blog.weight=mpt_mass[i];
	  bzero(blog.info,32);
          offi_.lock_user(utxs.buser);
          offi_.put_log(offi_.svid,utxs.buser,blog);
          offi_.unlock_user(utxs.buser);
          offi_.add_deposit(mpt_user[i],mpt_mass[i]);}}}
    else if(utxs.abank==utxs.bbank){
      log_t blog;
      blog.time=now;
      blog.type=*buf|0x8000; //incoming
      blog.node=utxs.abank;
      blog.user=utxs.auser;
      blog.umid=utxs.amsid;
      blog.nmid=msid; //can be overwritten with info
      blog.mpos=mpos; //can be overwritten with info
      blog.weight=utxs.tmass;
      memcpy(blog.info,utxs.tinfo,32);
      offi_.lock_user(utxs.buser);
      offi_.put_log(offi_.svid,utxs.buser,blog);
      offi_.unlock_user(utxs.buser);
      if(*buf==TXSTYPE_PUT && deposit>=0){
        offi_.add_deposit(utxs);}}
    //FIXME, return msid and mpos
    fprintf(stderr,"SENDING new user info %04X:%08X @ msg %08X:%08X\n",utxs.abank,utxs.auser,msid,mpos);
    boost::asio::write(socket_,boost::asio::buffer(&usera,sizeof(user_t))); //consider signing this message
    boost::asio::write(socket_,boost::asio::buffer(&msid,sizeof(uint32_t)));
    boost::asio::write(socket_,boost::asio::buffer(&mpos,sizeof(uint32_t)));
    return;
  }

  uint32_t started; //time started
private:
  boost::asio::ip::tcp::socket socket_;
  office& offi_;
  std::string addr;
  std::string port;
  char* buf;
  int len;
  int more;
  usertxs utxs;
  uint32_t msid;
  uint32_t mpos;
};

#endif // CLIENT_HPP
