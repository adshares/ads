#include "user.hpp"

user_t myuser;
char* msgp;
int msgl;

int run(settings& sts,std::string& line)
{ uint32_t to_bank; //actually uint16_t
  uint32_t to_user;
  uint64_t to_mass;
  uint32_t now=time(NULL);
  char msgtxt[0xff];
  if(!strncmp(line.c_str(),"INFO",4)){
    static uint8_t msg[(11+64)];
    msg[0]=TXSTYPE_INF;
    memcpy(msg+1,&sts.bank,2);
    memcpy(msg+3,&sts.user,4);
    memcpy(msg+7,&now ,4);
    ed25519_sign(msg,11,sts.sk,sts.pk,msg+11);
    ed25519_key2text(msgtxt,msg,11+64);
    fprintf(stdout,"%.*s\n",(11+64)*2,msgtxt);
    msgp=(char*)msg;
    msgl=11+64;
    return(11+64);}
  if(sscanf(line.c_str(),"SEND:%u:%u:%lu",&to_bank,&to_user,&to_mass)){
    static uint8_t msg[(32+1+28+64)];
    memcpy(msg,&sts.hash,32);
    msg[32]=TXSTYPE_SEN;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    memcpy(msg+33+10,&to_bank,2);
    memcpy(msg+33+12,&to_user,4);
    memcpy(msg+33+16,&to_mass,8); // should be a float
    memcpy(msg+33+24,&now,4); // could be used as additional placeholder
    ed25519_sign(msg,32+1+28,sts.sk,sts.pk,msg+32+1+28);
    ed25519_key2text(msgtxt,msg+32,1+28+64); // do not send last hash
    fprintf(stdout,"%.*s\n",(1+28+64)*2,msgtxt);
    msgp=(char*)msg;
    msgl=1+28+64;
    return(1+28+64);}
  return 0;
}

void print_user(user_t& u)
{  char pkey[64];
   char hash[64];
   ed25519_key2text(pkey,u.pkey,32);
   ed25519_key2text(hash,u.hash,32);
   fprintf(stdout,
     "id:%08X block:%08X weight:%016lX withdraw:%016lX user:%08X node:%04X status:%04X\npkey:%.64s\nhash:%.64s\n",
     u.id,u.block,u.weight,u.withdraw,u.user,u.node,u.status,pkey,hash);
}

void talk(boost::asio::ip::tcp::socket& socket,settings& sts) //len can be deduced from txstype
{ char buf[0xff];
  boost::asio::write(socket,boost::asio::buffer(msgp,msgl));
  if(*msgp==TXSTYPE_INF){
    int len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
    if(len!=sizeof(user_t)){
      std::cerr<<"ERROR reading txstype_inf\n";}
    else{
      memcpy(&myuser,buf,sizeof(user_t));
      print_user(myuser);
      sts.msid=myuser.id;
      //memcpy(sts.hash.c_str(),myuser.hash,SHA256_DIGEST_LENGTH);}}
      sts.hash.assign((char*)myuser.hash,SHA256_DIGEST_LENGTH);}}
  socket.close();
}

int main(int argc, char* argv[])
{ settings sts;
  sts.get(argc,argv);
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query(sts.addr,std::to_string(sts.port).c_str());
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
  boost::asio::ip::tcp::resolver::iterator end;
  boost::asio::ip::tcp::socket socket(io_service);
  boost::system::error_code error = boost::asio::error::host_not_found;
  while (endpoint_iterator != end){
    std::cerr<<"CONNECTING\n";
    //TODO add timeout
    //http://www.boost.org/doc/libs/1_52_0/doc/html/boost_asio/example/timeouts/blocking_tcp_client.cpp
    socket.connect(*endpoint_iterator, error);
    if(!error){
      break;}
    socket.close();
    endpoint_iterator++;}
  if(error){
    throw boost::system::system_error(error);}
  std::cerr<<"CONNECTED\n";
  try{
    if(!sts.exec.empty()){
      int len=run(sts,sts.exec);
      if(!len){
        std::cerr << "ERROR\n";}
      talk(socket,sts);
      return(0);}
    std::string line;
    while (std::getline(std::cin,line)){
      if(line[0]=='\n'){
        break;}
      if(line[0]=='.'){
        break;}
      int len=run(sts,line);
      if(!len){
        break;}
      //TODO, send to server
      talk(socket,sts);
      socket.connect(*endpoint_iterator, error);
      if(!error){
        break;}}}
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";}
  return 0;
}
