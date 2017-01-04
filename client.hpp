#ifndef CLIENT_HPP
#define CLIENT_HPP

class client : public boost::enable_shared_from_this<client>
{
public:
  client(boost::asio::io_service& io_service,office& offi,options& opts,server& srv)
    : socket_(io_service),
      offi_(offi),
      opts_(opts),
      srv_(srv)
      //,data(NULL)
  {  //read_msg_ = boost::make_shared<message>();
    std::cerr<<"OFFICER ready ("<<opts_.svid<<")\n";
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

//FIXME, user try/catch !!!
    char txstype=0;
    uint16_t cbank=0;
    uint32_t cuser=0;
    //uint32_t ctime;
    //uint8_t pk[32];
    int len=boost::asio::read(socket_,boost::asio::buffer(&txstype,1));
    if(!len){
      std::cerr<<"ERROR: read start failed\n";
      offi_.leave(shared_from_this());
      return;}
    if(txstype==TXSTYPE_INF){
      uint8_t msg[(11+64)];
      msg[0]=TXSTYPE_INF;
      len=boost::asio::read(socket_,boost::asio::buffer(msg+1,11+64-1));
      if(len!=11+64-1){
	std::cerr<<"ERROR: read txs failed\n";
        offi_.leave(shared_from_this());
        return;}
      memcpy(&cbank,msg+1,2);
      if(opts_.svid!=cbank){
	std::cerr<<"ERROR: bad bank ("<<opts_.svid<<"<>"<<cbank<<")\n";
        offi_.leave(shared_from_this());
        return;}
      memcpy(&cuser,msg+3,4);
      if(srv_.last_srvs_.nodes[cbank].users<=cuser){ //TODO, maybe should read current user limits !
	std::cerr<<"ERROR: bad user ("<<cuser<<")\n";
        offi_.leave(shared_from_this());
        return;}
      user_t u;
      srv_.last_srvs_.get_user(u,cbank,cuser); //TODO, consider using a duplicated fd instead opening the file each time
      if(ed25519_sign_open(msg,11,u.pkey,msg+11)){
	std::cerr<<"ERROR: signature failed\n";
        offi_.leave(shared_from_this());
        return;}
      std::cerr<<"SENDING user ("<<cuser<<")\n";
      boost::asio::write(socket_,boost::asio::buffer(&u,sizeof(user_t)));}
    offi_.leave(shared_from_this());
  }

  uint32_t started; //time started
private:
  boost::asio::ip::tcp::socket socket_;
  office& offi_;
  options& opts_;
  server& srv_;
  //char* data;
  std::string addr;
  std::string port;
};

#endif
