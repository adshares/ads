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

user_t myuser;
char* msgp;
int msgl;

int run(settings& sts,std::string& line)
{ uint32_t to_bank; //actually uint16_t
  uint32_t to_user;
   int64_t to_mass;
  uint32_t now=time(NULL);
  char msgtxt[0xff];
  static uint8_t* msg=NULL;
  if(msg==NULL){
    msg=(uint8_t*)std::malloc(1);}
  if(!strncmp(line.c_str(),"INF:",4)){ // get info
    msg=(uint8_t*)std::realloc(msg,11+64);
    msg[0]=TXSTYPE_CON;
    memcpy(msg+1,&sts.bank,2);
    memcpy(msg+3,&sts.user,4);
    memcpy(msg+7,&now ,4);
    ed25519_sign(msg,11,sts.sk,sts.pk,msg+11);
    ed25519_key2text(msgtxt,msg,11+64);
    fprintf(stdout,"%.*s\n",(11+64)*2,msgtxt);
    msgp=(char*)msg;
    msgl=11+64;
    return(msgl);}
  if(!strncmp(line.c_str(),"BRO:",4)){ // broadcast message
    msgl=line.length()-4;
    msg=(uint8_t*)std::realloc(msg,32+1+2+4+4+2+msgl+64);
    memcpy(msg,sts.ha,32);
    msg[32]=TXSTYPE_BRO;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    memcpy(msg+33+10,&msgl,2); // data length
    memcpy(msg+33+12,line.c_str()+4,msgl); // sign last message id
    ed25519_sign(msg,32+1+2+4+4+2+msgl,sts.sk,sts.pk,msg+32+1+2+4+4+2+msgl);
      ed25519_key2text(msgtxt,msg+32,1+2+4+4+2); // do not send last hash
      fprintf(stdout,"%.*s...\n",(1+2+4+4+2)*2,msgtxt);
    msgp=(char*)msg+32;
    msgl+=1+2+4+4+2+64;
    return(msgl);}
  if(sscanf(line.c_str(),"PUT:%u:%u:%ld",&to_bank,&to_user,&to_mass)){ // send funds
    if(to_mass<=0){
      return(0);}
    msg=(uint8_t*)std::realloc(msg,32+1+28+64);
    memcpy(msg,sts.ha,32);
    msg[32]=TXSTYPE_PUT;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    memcpy(msg+33+10,&to_bank,2);
    memcpy(msg+33+12,&to_user,4);
    memcpy(msg+33+16,&to_mass,8); // could be a float
    memcpy(msg+33+24,&now,4); // could be used as additional placeholder
    ed25519_sign(msg,32+1+28,sts.sk,sts.pk,msg+32+1+28);
      ed25519_key2text(msgtxt,msg+32,1+28+64); // do not send last hash
      fprintf(stdout,"%.*s\n",(1+28+64)*2,msgtxt);
    msgp=(char*)msg+32;
    msgl=1+28+64;
    return(msgl);}
  if(sscanf(line.c_str(),"USR:%u",&to_bank)){ // create new local user
    msg=(uint8_t*)std::realloc(msg,32+1+2+4+4+2+64);
    memcpy(msg,sts.ha,32);
    msg[32]=TXSTYPE_USR;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    memcpy(msg+33+10,&to_bank,2); // must be target bank
    ed25519_sign(msg,32+1+2+4+4+2,sts.sk,sts.pk,msg+32+1+2+4+4+2);
      ed25519_key2text(msgtxt,msg+32,1+2+4+4+2); // do not send last hash
      fprintf(stdout,"%.*s\n",(1+2+4+4+2+64)*2,msgtxt);
    msgp=(char*)msg+32;
    msgl+=1+2+4+4+2+64;
    return(msgl);}
  if(!strncmp(line.c_str(),"BNK:",4)){ // create new bank
    msg=(uint8_t*)std::realloc(msg,32+1+2+4+4+64);
    memcpy(msg,sts.ha,32);
    msg[32]=TXSTYPE_BNK;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    ed25519_sign(msg,32+1+2+4+4,sts.sk,sts.pk,msg+32+1+2+4+4);
      ed25519_key2text(msgtxt,msg+32,1+2+4+4); // do not send last hash
      fprintf(stdout,"%.*s\n",(1+2+4+4+64)*2,msgtxt);
    msgp=(char*)msg+32;
    msgl+=1+2+4+4+64;
    return(msgl);}
  if(sscanf(line.c_str(),"GET:%u:%u:%ld",&to_bank,&to_user,&to_mass)){ // retreive funds
    if(to_mass<=0){
      return(0);}
    msg=(uint8_t*)std::realloc(msg,32+1+28+64);
    memcpy(msg,sts.ha,32);
    msg[32]=TXSTYPE_GET;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    memcpy(msg+33+10,&to_bank,2);
    memcpy(msg+33+12,&to_user,4);
    memcpy(msg+33+16,&to_mass,8); // could be a float
    memcpy(msg+33+24,&now,4); // could be used as additional placeholder
    ed25519_sign(msg,32+1+28,sts.sk,sts.pk,msg+32+1+28);
      ed25519_key2text(msgtxt,msg+32,1+28+64); // do not send last hash
      fprintf(stdout,"%.*s\n",(1+28+64)*2,msgtxt);
    msgp=(char*)msg+32;
    msgl=1+28+64;
    return(msgl);}
  if(sscanf(line.c_str(),"KEY:%64s",msgtxt+32)){ // change user key
    ed25519_text2key((uint8_t*)msgtxt,msgtxt+32,32); // do not send last hash
    if(memcmp(msgtxt,sts.pn,32)){
      return(0);}
    msg=(uint8_t*)std::realloc(msg,32+1+2+4+4+4+64+64);
    memcpy(msg,sts.ha,32);
    msg[32]=TXSTYPE_KEY;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    memcpy(msg+33+10,&now,4); // could be used as additional placeholder
    ed25519_sign(msg,32+1+2+4+4+4,sts.sk,sts.pk,msg+32+1+2+4+4+4);
    ed25519_sign(msg,32+1+2+4+4+4,sts.sn,sts.sn,msg+32+1+2+4+4+4+64);
      ed25519_key2text(msgtxt,msg+32,1+2+4+4+4+64+64); // do not send last hash
      fprintf(stdout,"%.*s\n",(1+2+4+4+4+64+64)*2,msgtxt);
    msgp=(char*)msg+32;
    msgl=1+2+4+4+4+64+64;
    return(msgl);}
  if(sscanf(line.c_str(),"BKY:%64s",msgtxt+32)){ // change bank key
    ed25519_text2key((uint8_t*)msgtxt,msgtxt+32,32); // do not send last hash
    if(memcmp(msgtxt,sts.pn,32)){
      return(0);}
    msg=(uint8_t*)std::realloc(msg,32+1+2+4+4+4+64+64);
    memcpy(msg,sts.ha,32);
    msg[32]=TXSTYPE_BKY;
    memcpy(msg+33+0,&sts.bank,2);
    memcpy(msg+33+2,&sts.user,4);
    memcpy(msg+33+6,&sts.msid,4); // sign last message id
    memcpy(msg+33+10,&now,4); // could be used as additional placeholder
    ed25519_sign(msg,32+1+2+4+4+4,sts.sk,sts.pk,msg+32+1+2+4+4+4);
    ed25519_sign(msg,32+1+2+4+4+4,sts.sn,sts.sn,msg+32+1+2+4+4+4+64);
      ed25519_key2text(msgtxt,msg+32,1+2+4+4+4+64+64); // do not send last hash
      fprintf(stdout,"%.*s\n",(1+2+4+4+4+64+64)*2,msgtxt);
    msgp=(char*)msg+32;
    msgl=1+2+4+4+4+64+64;
    return(msgl);}
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
  try{
    boost::asio::write(socket,boost::asio::buffer(msgp,msgl));
    if(*msgp==TXSTYPE_INF || *msgp==TXSTYPE_PUT){
      int len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
      if(len!=sizeof(user_t)){
        std::cerr<<"ERROR reading confirmation\n";}
      else{
        memcpy(&myuser,buf,sizeof(user_t));
        print_user(myuser);
        if(*msgp==TXSTYPE_PUT && (uint32_t)sts.msid+1!=myuser.id){
          std::cerr<<"ERROR PUT failed\n";}
        sts.msid=myuser.id;
        memcpy(sts.ha,myuser.hash,SHA256_DIGEST_LENGTH);}}}
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
      if(error){
        std::cerr<<"ERROR connecting again\n";
        break;}}
    socket.close();}
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";}
  return 0;
}
