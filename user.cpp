#include <algorithm>
#include <atomic>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <openssl/sha.h>
#include <set>
#include <stack>
#include <vector>
#include "user.hpp"
#include "ed25519/ed25519.h"
#include "settings.hpp"

int cmpkey2(std::string& line)
{ if(line.length()<4+64){
    return(-1);}
  uint8_t msgtxt[64];
  ed25519_text2key((uint8_t*)msgtxt,line.c_str()+4,32); // do not send last hash
  return(memcmp(msgtxt,sts.pn,32));
}

usertxs_ptr run(settings& sts,std::string& line)
{ uint32_t to_bank=0; //actually uint16_t
  uint32_t to_user=0;
   int64_t to_mass=0;
  uint64_t to_info=0;
  uint32_t now=time(NULL);
  if(!strncmp(line.c_str(),"ME::",4)){ // get info about me
    usertxs_ptr txs(new usertxs(TXSTYPE_INF,sts.bank,sts.user,sts.bank,sts.user);}
  else if(sscanf(line.c_str(),"INF:%u:%u",&to_bank,&to_user)){ // get info about a different user
    usertxs_ptr txs(new usertxs(TXSTYPE_INF,sts.bank,sts.user,to_bank,to_user);}
  else if(sscanf(line.c_str(),"LOG:%u",&now)){ // get info about me and my log since 'now'
    usertxs_ptr txs(new usertxs(TXSTYPE_INF,sts.bank,sts.user,now);} //now==0xffffffff => fix log file if needed
  else if(!strncmp(line.c_str(),"BRO:",4)){ // broadcast message
    usertxs_ptr txs(new usertxs(TXSTYPE_BRO,sts.bank,sts.user,sts.msid,now,line.length()-4,to_user,to_mass,line.c_str()+4);}
  else if(sscanf(line.c_str(),"PUT:%u:%u:%ld:%ld",&to_bank,&to_user,&to_mass,&to_info)){ // send funds
    usertxs_ptr txs(new usertxs(TXSTYPE_PUT,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,NULL,NULL);}
  else if(sscanf(line.c_str(),"USR:%u",&to_bank)){ // create new user @ to_bank
    usertxs_ptr txs(new usertxs(TXSTYPE_USR,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,NULL,NULL);}
  else if(!strncmp(line.c_str(),"BNK:",4)){ // create new bank
    usertxs_ptr txs(new usertxs(TXSTYPE_BNK,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,NULL,NULL);}
  else if(sscanf(line.c_str(),"GET:%u:%u",&to_bank,&to_user)){ // retreive funds
    usertxs_ptr txs(new usertxs(TXSTYPE_GET,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,NULL,NULL);}
  else if(!strncmp(line.c_str(),"KEY:",4)){ // get info
    if(!check_key2(line)){
      return(NULL);}
    hash_t key;
    ed25519_text2key(key,line.c_str()+4,32); // do not send last hash
    usertxs_ptr txs(new usertxs(TXSTYPE_KEY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,key,NULL);
    txs->sign(sts.ha,sts.sk,sts.pk);
    txs->sign2(sts.sn,sts.pn);
    txs->print();
    return(txs);}
  else if(!strncmp(line.c_str(),"BKY:",4)){ // get info
    if(cmpkey2(line)){
      return(NULL);}
    hash_t key;
    ed25519_text2key(key,line.c_str()+4,32); // do not send last hash
    usertxs_ptr txs(new usertxs(TXSTYPE_BKY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,key,sts.po);
    txs->sign(sts.ha,sts.sk,sts.pk);
    txs->sign2(sts.sn,sts.pn);
    txs->sign3(sts.so,sts.po); //add this !!!
    txs->print();
    return(txs);}
  else{
    return(NULL);}
  txs->sign(sts.ha,sts.sk,sts.pk);
  txs->print();
  return(txs);
}

void print_user(user_t& u)
{  char pkey[64];
   char hash[64];
   ed25519_key2text(pkey,u.pkey,32);
   ed25519_key2text(hash,u.hash,32);
   fprintf(stdout,
     "msid:%08X time:%08X stat:%04X node:%04X user:%08X lpath:%08X rpath:%08X balance:%016lX\npkey:%.64s\nhash:%.64s\n",
     u.id,u.time,u.stat,u.node,u.user,u.lpath,u.rpath,u.weight,pkey,hash);
}

void print_log(log_t* log,int len)
{ int fd=open("usr.log",O_RDONLY|O_CREATE);
  write(fd,(char*)log,len);
  close(fd);
  if(len%sizeof(log_t)){
    std::cerr<<"ERROR, bad log allignment, viewer not yet implemanted, hex-view usr.log to investigate\n";}
  for(log_t* end=log+len/sizeof(log_t);log<end;log++){
    fprintf(stdout,"%08X %04X %04X %08X %08X %08X %08X %016X\n",
      log->time,log->type,log->node,log->nmid,log->user,log->umid,log->mpos,log->eight);}
}

void talk(boost::asio::ip::tcp::socket& socket,settings& sts,usertxs_ptr txs) //len can be deduced from txstype
{ char buf[0xff];
  try{
    boost::asio::write(socket,boost::asio::buffer(txs.message(),txs.length()));
    int len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
    if(len!=sizeof(user_t)){
      std::cerr<<"ERROR reading confirmation\n";}
    else{
      user_t myuser;
      memcpy(&myuser,buf,sizeof(user_t));
      print_user(myuser);
      if(*msgp!=TXSTYPE_INF && *msgp!=TXSTYPE_LOG && (uint32_t)sts.msid+1!=myuser.id){
        std::cerr<<"ERROR transaction failed\n";}
      sts.msid=myuser.id;
      memcpy(sts.ha,myuser.hash,SHA256_DIGEST_LENGTH);}
    if(*msgp==TXSTYPE_LOG){
      int len;
      if(sizeof(int)!=boost::asio::read(socket,boost::asio::buffer(&len,sizeof(int)))){
        std::cerr<<"ERROR reading log\n";
        socket.close();
        return;}
      log_t* log=(log_t*)std::malloc(len+sizeof(log_t));
      if(len!=boost::asio::read(socket,boost::asio::buffer((char*)log,len))){ // exception will cause leak
        std::cerr<<"ERROR reading log\n";
        free(log);
        socket.close();
        return;}
      print_log(log,len);
      free(log);}}
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";}
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
      usertxs_ptr txs=run(sts,sts.exec);
      if(txs==NULL){
        std::cerr << "ERROR\n";}
      talk(socket,sts,txs);
      return(0);}
    std::string line;
    while (std::getline(std::cin,line)){
      if(line[0]=='\n'){
        break;}
      if(line[0]=='.'){
        break;}
      usertxs_ptr txs=run(sts,line);
      if(txs==NULL){
        break;}
      //TODO, send to server
      talk(socket,sts,txs);
      socket.connect(*endpoint_iterator, error);
      if(error){
        std::cerr<<"ERROR connecting again\n";
        break;}}
    socket.close();}
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";}
  return 0;
}
