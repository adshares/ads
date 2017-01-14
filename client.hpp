#ifndef CLIENT_HPP
#define CLIENT_HPP

//this could all go to the office class and we could use just the start() function

class client : public boost::enable_shared_from_this<client>
{
public:
  client(boost::asio::io_service& io_service,office& offi,options& opts,server& srv)
    : socket_(io_service),
      offi_(offi)
      //opts_(opts)
      //srv_(srv)
      //,data(NULL)
  {  //read_msg_ = boost::make_shared<message>();
    std::cerr<<"OFFICER ready ("<<offi_.svid<<")\n";
  }

  ~client()
  { std::cerr << "Client left " << addr << ":" << port << "\n"; //LESZEK
    //if(data!=NULL){
    //  free(data);
    //  data=NULL;}
  }

  boost::asio::ip::tcp::socket& socket()
  { return socket_;
  }

  void start() //TODO consider providing a local user file pointer
  { addr = socket_.remote_endpoint().address().to_string();
    port = std::to_string(socket_.remote_endpoint().port());
    std::cerr << "Client entered " << addr << ":" << port << "\n";
    started=time(NULL);

/* do not use handshake
    halo_t halo;
    int len=boost::asio::read(socket_,boost::asio::buffer(&halo,sizeof(halo_t)));
    if(len!=sizeof(halo_t)){
      offi_.leave(shared_from_this());}
    char* data=(char*)malloc(sizeof(halo.len));
    int len=boost::asio::read(socket_,boost::asio::buffer(data,sizeof(halo.len)));
    if(len!=sizeof(halo.len)){
      offi_.leave(shared_from_this());} */

    char txstype=0;
    uint16_t cbank=0;
    uint32_t cuser=0;
    uint32_t ctime;
    int len=boost::asio::read(socket_,boost::asio::buffer(&txstype,1));
    if(!len){
      std::cerr<<"ERROR: read start failed\n";
      return;}
    if(txstype==TXSTYPE_INF){
      uint8_t msg[(1+10+64)];
      msg[0]=TXSTYPE_INF;
      len=boost::asio::read(socket_,boost::asio::buffer(msg+1,10+64));
      if(len!=10+64){
	std::cerr<<"ERROR: read txs failed\n";
        return;}
      memcpy(&cbank,msg+1,2);
      if(offi_.svid!=cbank){
	std::cerr<<"ERROR: bad bank ("<<offi_.svid<<"<>"<<cbank<<")\n";
        return;}
      memcpy(&cuser,msg+3,4);
      if(offi_.users<=cuser){ //TODO, maybe should read local user limits !
	std::cerr<<"ERROR: bad user ("<<cuser<<")\n";
        return;}
      memcpy(&ctime,msg+7,4);
      if(abs(ctime-started)>5){
	int diff=(ctime-started);
	std::cerr<<"ERROR: high time difference ("<<diff<<">5)\n";
        return;}
      user_t u;
      offi_.get_user(u,cbank,cuser);
      if(ed25519_sign_open(msg,11,u.pkey,msg+11)){
	std::cerr<<"ERROR: signature failed for user "<<cbank<<":"<<cuser<<"\n";
        return;}
      std::cerr<<"SENDING user ("<<cuser<<")\n";
      boost::asio::write(socket_,boost::asio::buffer(&u,sizeof(user_t))); //consider signing this message
      //consider sending aditional optional info
      return;}
    if(txstype==TXSTYPE_PUT){
      uint8_t msg[(32+1+28+64)];
      uint32_t msid;
      uint16_t to_bank;
      uint32_t to_user;
       int64_t to_mass;
      msg[32]=TXSTYPE_PUT;
      len=boost::asio::read(socket_,boost::asio::buffer(msg+33,28+64));
      if(len!=28+64){
	std::cerr<<"ERROR: read txs failed\n";
        return;}
      memcpy(&cbank,msg+33+0,2);
      if(offi_.svid!=cbank){
	std::cerr<<"ERROR: bad bank ("<<offi_.svid<<"<>"<<cbank<<")\n";
        return;}
      memcpy(&cuser,msg+33+2,4);
      if(offi_.users<=cuser){ //TODO, maybe should read current user limits !
	std::cerr<<"ERROR: bad user ("<<cuser<<")\n";
        return;}
      memcpy(&msid,msg+33+6,4); // sign last message id
      memcpy(&to_bank,msg+33+10,2);
      memcpy(&to_user,msg+33+12,4);
      memcpy(&to_mass,msg+33+16,8); // should be a float
      memcpy(&ctime,msg+33+24,4);
      if(abs(ctime-started)>5){
	int diff=(ctime-started);
	std::cerr<<"ERROR: high time difference ("<<diff<<">5)\n";
        return;}
      if(to_mass<=0){
	std::cerr<<"ERROR: bad txs value ("<<to_mass<<")\n";
        return;}
      if(!offi_.check_account(to_bank,to_user)){
	std::cerr<<"ERROR: bad to_account ("<<to_user<<":"<<to_bank<<")\n";
        return;}
      user_t u;
      offi_.lock_user(cuser);
      offi_.get_user(u,cbank,cuser);
      memcpy(msg,u.hash,32);
      if(ed25519_sign_open(msg,32+1+28,u.pkey,msg+32+1+28)){
        char msgtxt[2*(32+1+28+64)];
        ed25519_key2text(msgtxt,msg,32+1+28+64);
        fprintf(stdout,"%.*s\n",2*32,msgtxt);
        fprintf(stdout,"%.*s\n",(1+28+64)*2,msgtxt+2*32);
	std::cerr<<"ERROR: signature failed\n";
        offi_.unlock_user(cuser);
        return;}
      if(msid!=u.id){
	std::cerr<<"ERROR: bad msid ("<<msid<<"<>"<<u.id<<")\n";
        return;}
      if(ctime<=u.block){
	std::cerr<<"ERROR: too many transactions ("<<ctime<<"<="<<u.block<<")\n"; //TODO, ignore during testing
        return;}
       int64_t fee=TXS_SEN_FEE(to_mass)+TIME_FEE(ctime-u.block);
      if(to_mass+fee+MIN_MASS>u.weight){
	std::cerr<<"ERROR: too low balance ("<<to_mass<<"+"<<fee<<"+"<<MIN_MASS<<">"<<u.weight<<")\n";
        return;}
      //send message
      offi_.add_msg(msg+32,1+28+64,fee); //TODO, could return pointer to file, unless we use only validated pointers
      //commit changes
      u.id++;
      u.block=ctime;
      //convert message to hash
      SHA256_CTX sha256;
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,msg+32,1+28+64);
      SHA256_Final(msg+32,&sha256); //overwrite msg!!!
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,msg,64); //make newhash=hash(oldhash+newmessagehash);
      SHA256_Final(u.hash,&sha256);
      offi_.set_user(u,cbank,cuser,to_mass+fee);
      offi_.unlock_user(cuser);
      if(to_bank==cbank){
        //add incomming transaction to history?
        offi_.add_deposit(to_user,to_mass);}
      std::cerr<<"SENDING user ("<<cuser<<") after txs_sen\n";
      boost::asio::write(socket_,boost::asio::buffer(&u,sizeof(user_t))); //consider signing this message
      return;}
    std::cerr<<"ERROR, unknown transaction\n";
  }

  uint32_t started; //time started
private:
  boost::asio::ip::tcp::socket socket_;
  office& offi_;
  //options& opts_;
  //server& srv_;
  //char* data;
  std::string addr;
  std::string port;
};

#endif
