#ifndef CLIENT_HPP
#define CLIENT_HPP

//this could all go to the office class and we could use just the start() function

class client : public boost::enable_shared_from_this<client>
{
public:
  client(boost::asio::io_service& io_service,office& offi,options& opts,server& srv)
    : socket_(io_service),
      offi_(offi),
      buf(NULL)
  {  //read_msg_ = boost::make_shared<message>();
    std::cerr<<"OFFICER ready ("<<offi_.svid<<")\n";
  }

  ~client()
  { std::cerr << "Client left " << addr << ":" << port << "\n"; //LESZEK
    free(buf);
  }

  boost::asio::ip::tcp::socket& socket()
  { return socket_;
  }

  void start() //TODO consider providing a local user file pointer
  { addr = socket_.remote_endpoint().address().to_string();
    port = std::to_string(socket_.remote_endpoint().port());
    std::cerr << "Client entered " << addr << ":" << port << "\n";
    started=time(NULL);
    uint32_t lpath=started-started%BLOCKSEC;
    uint32_t luser=0;
    uint16_t lnode=0;
    //int extralen=0;
    int64_t deposit=-1; // if deposit=0 inform target
    int64_t deduct=0;
    int64_t fee=0;
    buf=malloc(txslen[TXSTYPE_MAX]+64);

    char *txstype=buf;
    int len=boost::asio::read(socket_,boost::asio::buffer(buf,1));
    if(!len && *txstype>=TXSTYPE_MAX){
      std::cerr<<"ERROR: read start failed\n";
      return;}
    len=boost::asio::read(socket_,boost::asio::buffer(buf+1,txslen[*txstype]+64-1));
    if(len!=txslen[*txstype]+64){
      std::cerr<<"ERROR: read message failed\n";
      return;}
    if(*txstype==TXSTYPE_BRO){
      //extralen=utxs.bbank;
      buf=std::realloc(buf,txslen[TXSTYPE_BRO]+64+utxs.bbank);
      len=boost::asio::read(socket_,boost::asio::buffer(buf+txslen[TXSTYPE_BRO]+64,utxs.bbank));
      if(len!=utxs.bbank){
        std::cerr<<"ERROR: read broadcast failed\n";
        return;}}
    /************* START PROCESSING **************/
    user_t usera;
    usertxs utxs;
    utxs.parse(buf);
    offi_.lock_user(utxs.auser); //needed to prevent 2 msgs from a user with same msid
    //consider adding a max txs limit per user
    if(*txstype>=TXSTYPE_MAX){
      std::cerr<<"ERROR: read user failed\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(!offi_.get_user(usera,utxs.abank,utxs.auser)){
      std::cerr<<"ERROR: read user failed\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(utxs.wrong_sig(buf,usera.hash,usera.pk)){
      std::cerr<<"ERROR: bad signature\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if((ttype==TXSTYPE_KEY || ttype==TXSTYPE_BKY) && utxs.wrong_sig2(buf,usera.hash)){
      std::cerr<<"ERROR: bad second signature\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(ttype==TXSTYPE_BKY){
      if(memcmp(utxs.key2(buf),offi_.pkey(),32)){
        std::cerr<<"ERROR: bad old key\n";
        offi_.unlock_user(utxs.auser);
        return;}
      if(utxs.wrong_sig3(buf,usera.hash)){
        std::cerr<<"ERROR: bad second signature\n";
        offi_.unlock_user(utxs.auser);
        return;}}
    if(*txstype==TXSTYPE_INF){ // this is special, just local info
      //if((abs(utxs.ttime-started)>5)){
      //  int diff=(utxs.ttime-started);
      //  std::cerr<<"ERROR: high time difference ("<<diff<<">5)\n";
      //  offi_.unlock_user(utxs.auser);
      //  return;}
//FIXME, read data also from server
//FIXME, if local account locked, check if unlock was successfull based on time passed after change
      if(utxs.abank!=offi.svid || utxs.bbank!=offi.svid){
        std::cerr<<"ERROR: bad bank\n";
        offi_.unlock_user(utxs.auser);
        return;}
      std::cerr<<"SENDING user info ("<<utxs.bbank<<":"<<utxs.buser<<")\n";
      if(utxs.abank!=utxs.bbank || utxs.auser!=utxs.buser)
        user_t userb;
        if(!offi_.get_user(userb,utxs.bbank,utxs.buser)){
          offi_.unlock_user(utxs.auser);
          return;}}
        boost::asio::write(socket_,boost::asio::buffer(&userb,sizeof(user_t)));}
      else{
        boost::asio::write(socket_,boost::asio::buffer(&usera,sizeof(user_t)));}
      //consider sending aditional optional info
      offi_.unlock_user(utxs.auser);
      //offi_.purge_log(...);
      return;}
    if(*txstype==TXSTYPE_LOG){
      if(utxs.abank!=offi.svid){
        std::cerr<<"ERROR: bad bank\n";
        offi_.unlock_user(utxs.auser);
        return;}
      boost::asio::write(socket_,boost::asio::buffer(&usera,sizeof(user_t)));
      std::string slog;
      if(!get_log(utxs.abank,utxs.auser,utxs.ttime,slog)){
        std::cerr<<"ERROR: bad bank\n";
        offi_.unlock_user(utxs.auser);
        return;}
      boost::asio::write(socket_,boost::asio::buffer(slog.c_str(),slog.size()));
      offi_.unlock_user(utxs.auser);
      return;}
    if(utxs.abank!=offi.svid && *txstype!=TXSTYPE_USR){
      std::cerr<<"ERROR: bad bank\n";
      offi_.unlock_user(utxs.auser);
      return;}
    if(usera.msid!=utxs.amsid){
      std::cerr<<"ERROR: bad msid ("<<usera.msid<<"<>"<<utxs.amsid<<")\n";
      offi_.unlock_user(utxs.auser);
      return;}
    //if(usera.time>utxs.ttime){ //does not work because of _GET
    //  std::cerr<<"ERROR: bad transaction time ("<<usera.time<<">"<<utxs.ttime<<")\n";
    //  offi_.unlock_user(utxs.auser);
    //  return;}
    //include additional 2FA later

    //commit trasaction
    if(usera.time+LOCK_TIME<lpath && usera.luser && usera.lpath){//check account lock
      if(*txstype!=TXSTYPE_PUT || utxs.abank!=utxs.bbank || utxs.auser!=utxs.buser || utxs.tmass!=0){
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
      luser=usera.luser;
      lnode=usera.lnode;}
    else if(*txstype==TXSTYPE_BRO){
      fee=TXS_BRO_FEE(utxs.bbank)+TIME_FEE(lpath,usera.lpath);}
    else if(*txstype==TXSTYPE_PUT){
      if(utxs.tmass<0){ //sending info about negative values is allowed to fascilitate exchanges
        utxs.tmass=0;}
      //if(utxs.abank!=utxs.bbank && utxs.auser!=utxs.buser && !check_user(utxs.bbank,utxs.buser)){
      if(!check_user(utxs.bbank,utxs.buser)){
        // does not check if account closed [consider adding this slow check]
	std::cerr<<"ERROR: bad target user ("<<utxs.bbank<<":"<utxs.buser")\n";
        offi_.unlock_user(utxs.auser);
        return;}
      deposit=utxs.tmass;
      deduct=utxs.tmass;
      fee=TXS_PUT_FEE(utxs.tmass)+TIME_FEE(lpath,usera.lpath);}
    else if(*txstype==TXSTYPE_USR){
      if(utxs.bbank!=offi_.svid){
        std::cerr<<"ERROR: bad target bank ("<<utxs.bbank<<")\n";
        offi_.unlock_user(utxs.auser);
        return;}
      deduct=MIN_MASS;
      fee=TIME_FEE(lpath,usera.lpath);
      if(deduct+fee+MIN_MASS>usera.weight){ //check in advance before creating new user
        std::cerr<<"ERROR: too low balance ("<<deduct<<"+"<<fee<<"+"<<MIN_MASS<<">"<<usera.weight<<")\n";
        offi_.unlock_user(utxs.auser);
        return;}
      if(!utxs.abank==offi_.svid && !offi_.try_account((hash_s*)usera.pkey)){
        std::cerr<<"ERROR: failed to open account (pkey known)\n";
        offi_.unlock_user(utxs.auser);
        return;}
      uint32_t nuser=offi_.add_user(utxs.abank,utxs.auser);
      if(!nuser){
        std::cerr<<"ERROR: failed to open account\n";
        offi_.unlock_user(utxs.auser);
        return;}
      //extralen=4+32;
      //buf=std::realloc(buf,txslen[*txstype]+64+extralen);
      buf=std::realloc(buf,utxs.size);
      memcpy(buf+txslen[*txstype]+64+0,&nuser,4);
      memcpy(buf+txslen[*txstype]+64+4,usera.pkey,32);
      if(utxs.abank==offi_.svid){
        lnode=0;
        luser=nuser;} // record new user id
      else{
        //FIXME, check if original bank is in our whitelist
        //offi_.add_msg(buf,txslen[*txstype]+64+extralen,0); //TODO, could return pointer to file
        offi_.add_msg(buf,utxs.auser,0); //TODO, could return pointer to file
        offi_.unlock_user(utxs.auser);
        offi_.add_account((hash_s*)users.pkey,nuser); //blacklist
        user_t userb;
        get_user(userb,utxs.bbank,nuser); // send userb
        boost::asio::write(socket_,boost::asio::buffer(&userb,sizeof(user_t)));
        return;}}
    else if(*txstype==TXSTYPE_BNK){ // we will get a confirmation from the network
      deduct=BANK_MIN_MASS;
      fee=TXS_BNK_FEE*TIME_FEE(lpath,usera.lpath);} 
    else if(*txstype==TXSTYPE_GET){ // we will get a confirmation from the network
      if(utxs.abank==utxs.bbank){
	std::cerr<<"ERROR: bad bank ("<<utxs.bbank<<"), use PUT\n";
        offi_.unlock_user(utxs.auser);
        return;}
      fee=TXS_GET_FEE*TIME_FEE(lpath,usera.lpath);}
    else if(*txstype==TXSTYPE_KEY){
      memcpy(usera.pkey,utxs.key(buf),32);
      fee=TXS_KEY_FEE*TIME_FEE(lpath,usera.lpath);} 
    else if(*txstype==TXSTYPE_BKY){ // we will get a confirmation from the network
      if(utxs.auser){
	std::cerr<<"ERROR: bad user ("<<utxs.auser<<") for this bank changes\n";
        offi_.unlock_user(utxs.auser);
        return;}
      fee=TXS_BKY_FEE*TIME_FEE(lpath,usera.lpath);} 
    else if(*txstype==TXSTYPE_STP){ // we will get a confirmation from the network
      assert(0); //TODO, not implemented later
      fee=TXS_STP_FEE*TIME_FEE(lpath,usera.lpath);} 
    if(deduct+fee+MIN_MASS>usera.weight){
      std::cerr<<"ERROR: too low balance ("<<deduct<<"+"<<fee<<"+"<<MIN_MASS<<">"<<usera.weight<<")\n";
      offi_.unlock_user(utxs.auser);
      return;}
    //send message
    //offi_.add_msg(buf,txslen[*txstype]+64+extralen,fee); //TODO, could return pointer to file
    offi_.add_msg(buf,utxs.size,fee); //TODO, could return pointer to file
    //commit changes
    usera.msid++;
    usera.time=utxs.ttime;
    usera.node=lnode;
    usera.user=luser;
    usera.lpath=lpath;
    //convert message to hash
    uint8_t hash[32];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    //SHA256_Update(&sha256,buf,txslen[*txstype]+64+extralen);
    SHA256_Update(&sha256,buf,utxs.size);
    SHA256_Final(hash,&sha256); //overwrite msg!!!
    //make newhash=hash(oldhash+newmessagehash);
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,usera.hash,32);
    SHA256_Update(&sha256,hash,32);
    SHA256_Final(usera.hash,&sha256);
    offi_.set_user(usera,deduct+fee);
    offi_.unlock_user(utxs.auser);
    if(deposit>=0 && utxs.abank==utxs.bbank){
      offi_.add_deposit(utxs);}
    std::cerr<<"SENDING user info ("<<utxs.abank<<":"<<utxs.auser<<")\n";
    boost::asio::write(socket_,boost::asio::buffer(&usera,sizeof(user_t))); //consider signing this message
    return;
  }

  uint32_t started; //time started
private:
  boost::asio::ip::tcp::socket socket_;
  office& offi_;
  std::string addr;
  std::string port;
  char* buf;
};

#endif
