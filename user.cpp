#include <algorithm>
#include <atomic>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/container/flat_set.hpp>
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
#include "ed25519/ed25519.h"
#include "user.hpp"
#include "settings.hpp"

#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/optional/optional.hpp>

// expected format:  :to_bank,to_user,to_mass:to_bank,to_user,to_mass:to_bank,to_user,to_mass ... ;
// the list must terminate with ';'
bool parse_mpt(std::string& text,uint32_t& to_bank,const char* line,int end)
{ int pos=3;
  int add=0;
  std::set<uint64_t> out;
  union {uint64_t big;uint32_t small[2];} to;
  //to.small[1]=0;
  for(;pos<end;pos+=add){
    char to_acct[2+4+8];
    uint32_t& tuser=to.small[0];
    uint32_t& tbank=to.small[1];
     int64_t tmass;
    if(line[pos]==';'){
      return(true);}
    if(3!=sscanf(line+pos,":%X,%X,%lX%n",&tbank,&tuser,&tmass,&add) || !add){
      return(false);}
    if(out.find(to.big)!=out.end()){
      fprintf(stderr,"ERROR: duplicate target: %04X:%08X\n",tbank,tuser);
      return(false);}
    out.insert(to.big);
    memcpy(to_acct+0,&tbank,2);
    memcpy(to_acct+2,&tuser,4);
    memcpy(to_acct+6,&tmass,8);
    text.append(to_acct,2+4+8);
    to_bank++;}
  return(false);
}

uint16_t crc16(const uint8_t* data_p, uint8_t length)
{ uint8_t x;
  uint16_t crc = 0xFFFF;

  while(length--){
    x = crc >> 8 ^ *data_p++;
    x ^= x>>4;
    crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);}
  return crc;
}
uint16_t crc_acnt(uint16_t to_bank,uint32_t to_user)
{ uint8_t data[6];
  memcpy(data,&to_bank,2);
  memcpy(data+2,&to_user,4);
  return(crc16(data,6));
}

bool parse_bank(uint16_t& to_bank,boost::optional<std::string>& json_bank)
{ std::string str_bank=json_bank.get();
  char *endptr;
  if(str_bank.length()!=4){
    fprintf(stderr,"ERROR: parse_user(%s) bad length (required 4)\n",str_bank.c_str());
    return(false);}
  errno=0;
  to_bank=(uint16_t)strtol(str_bank.c_str(),&endptr,16);
  if(errno || endptr==str_bank.c_str() || *endptr!='\0'){
    fprintf(stderr,"ERROR: parse_bank(%s)\n",str_bank.c_str());
    perror("ERROR: strtol");
    return(false);}
  return(true);
}

bool parse_user(uint32_t& to_user,boost::optional<std::string>& json_user)
{ std::string str_user=json_user.get();
  char *endptr;
  if(str_user.length()!=8){
    fprintf(stderr,"ERROR: parse_user(%s) bad length (required 8)\n",str_user.c_str());
    return(false);}
  errno=0;
  to_user=(uint32_t)strtoll(str_user.c_str(),&endptr,16);
  if(errno || endptr==str_user.c_str()){
    fprintf(stderr,"ERROR: parse_user(%s)\n",str_user.c_str());
    perror("ERROR: strtol");
    return(false);}
  return(true);
}

bool parse_acnt(uint16_t& to_bank,uint32_t& to_user,boost::optional<std::string>& json_acnt)
{ uint16_t to_csum=0;
  uint16_t to_crc16;
  std::string str_acnt=json_acnt.get();
  char *endptr;
  if(str_acnt.length()!=18){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad length (required 18)\n",str_acnt.c_str());
    return(false);}
  if(str_acnt[4]!='-' || str_acnt[13]!='-'){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad format (required BBBB-UUUUUUUU-XXXX)\n",str_acnt.c_str());
    return(false);}
  str_acnt[4]='\0';
  str_acnt[13]='\0';
  errno=0;
  to_bank=(uint16_t)strtol(str_acnt.c_str(),&endptr,16);
  if(errno || endptr==str_acnt.c_str()){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad bank\n",str_acnt.c_str());
    perror("ERROR: strtol");
    return(false);}
  errno=0;
  to_user=(uint32_t)strtoll(str_acnt.c_str()+5,&endptr,16);
  if(errno || endptr==str_acnt.c_str()+5){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad user\n",str_acnt.c_str());
    perror("ERROR: strtol");
    return(false);}
  if(!strncmp("XXXX",str_acnt.c_str()+14,4)){
    return(true);}
  errno=0;
  to_csum=(uint16_t)strtol(str_acnt.c_str()+14,&endptr,16);
  if(errno || endptr==str_acnt.c_str()+14){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad checksum\n",str_acnt.c_str());
    perror("ERROR: strtol");
    return(false);}
  to_crc16=crc_acnt(to_bank,to_user);
  if(to_csum!=to_crc16){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad checksum (expected %04X)\n",str_acnt.c_str(),to_crc16);
    return(false);}
  return(true);
}

bool parse_sign(uint8_t* to_sign,boost::optional<std::string>& json_sign)
{ std::string str_sign=json_sign.get();
  if(str_sign.length()!=128){
    fprintf(stderr,"ERROR: parse_sign(%s) bad length (required 128)\n",str_sign.c_str());
    return(false);}
  ed25519_text2key(to_sign,str_sign.c_str(),64);
  return(true);
}

bool parse_pkey(uint8_t* to_pkey,boost::optional<std::string>& json_pkey)
{ std::string str_pkey=json_pkey.get();
  if(str_pkey.length()!=64){
    fprintf(stderr,"ERROR: parse_pkey(%s) bad length (required 64)\n",str_pkey.c_str());
    return(false);}
  ed25519_text2key(to_pkey,str_pkey.c_str(),32);
  return(true);
}

usertxs_ptr run_json(settings& sts,char* line,bool& dryrun)
{ uint16_t to_bank=0;
  uint32_t to_user=0;
   int64_t to_mass=0;
  uint64_t to_info=0;
  uint32_t to_from=0xffffffff;
  uint8_t  to_pkey[32];
  uint8_t  to_sign[64];
//uint32_t to_msid=0; //FIXME, use !!!
//uint32_t to_hash[32]; //FIXME, use !!!
  uint32_t now=time(NULL);
  dryrun=false;
  usertxs_ptr txs=NULL;
  std::stringstream ss;
  boost::property_tree::ptree pt;
  ss << line;
  try{
    std::cout<<line;
    boost::property_tree::read_json(ss,pt);}
  catch (std::exception& e){
    std::cerr << "RUN_JSON Exception: " << e.what() << "\n";}
  boost::optional<std::string> json_sign=pt.get_optional<std::string>("signature");
  if(json_sign && !parse_sign(to_sign,json_sign)){
    return(NULL);}
  boost::optional<std::string> json_pkey=pt.get_optional<std::string>("pkey");
  if(json_pkey && !parse_pkey(to_pkey,json_pkey)){
    return(NULL);}
  boost::optional<std::string> json_bank=pt.get_optional<std::string>("bank");
  if(json_bank && !parse_bank(to_bank,json_bank)){
    return(NULL);}
  boost::optional<std::string> json_user=pt.get_optional<std::string>("user");
  if(json_user && !parse_user(to_user,json_user)){
    return(NULL);}
  boost::optional<std::string> json_acnt=pt.get_optional<std::string>("account");
  if(json_acnt && !parse_acnt(to_bank,to_user,json_acnt)){
    return(NULL);}
  boost::optional<uint32_t> json_from=pt.get_optional<uint32_t>("since");
  if(json_from){
    to_from=json_from.get();}
  boost::optional< int64_t> json_mass=pt.get_optional< int64_t>("amount");
  if(json_mass){
    to_mass=json_mass.get();}
  boost::optional<uint64_t> json_info=pt.get_optional<uint64_t>("message");
  if(json_info){
    to_info=json_info.get();}
  boost::optional<bool> json_null=pt.get_optional<bool>("dryrun");
  if(json_null){
    dryrun=json_null.get();}

  std::string run=pt.get<std::string>("run");
  if(!run.compare("get_me")){
    txs=boost::make_shared<usertxs>(TXSTYPE_INF,sts.bank,sts.user,sts.bank,sts.user,now);}
  else if(!run.compare("get_user")){
    txs=boost::make_shared<usertxs>(TXSTYPE_INF,sts.bank,sts.user,to_bank,to_user,now);}
  else if(!run.compare("get_log")){
    txs=boost::make_shared<usertxs>(TXSTYPE_LOG,sts.bank,sts.user,to_from);}
  else if(!run.compare("get_broadcast")){
    txs=boost::make_shared<usertxs>(TXSTYPE_BLG,sts.bank,sts.user,to_from);}
  else if(!run.compare("send_broadcast")){
    std::string text=pt.get<std::string>("text");
    txs=boost::make_shared<usertxs>(TXSTYPE_BRO,sts.bank,sts.user,sts.msid,now,text.length(),to_user,to_mass,to_info,text.c_str());}
  else if(!run.compare("send_to_many")){ //FIXME !!!
    uint32_t to_num=0;
    std::string text;
    if(!parse_mpt(text,to_num,NULL,0) || !to_num){
      return(NULL);}
    txs=boost::make_shared<usertxs>(TXSTYPE_MPT,sts.bank,sts.user,sts.msid,now,to_num,to_user,to_mass,to_info,text.c_str());}
  else if(!run.compare("send")){
    txs=boost::make_shared<usertxs>(TXSTYPE_PUT,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare("send_user_application")){
    txs=boost::make_shared<usertxs>(TXSTYPE_USR,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare("send_node_application")){
    txs=boost::make_shared<usertxs>(TXSTYPE_BNK,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare("send_account_recovery")){
    txs=boost::make_shared<usertxs>(TXSTYPE_GET,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare("send_new_user_public_key")){
    txs=boost::make_shared<usertxs>(TXSTYPE_KEY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)to_pkey);}
  else if(!run.compare("send_new_node_public_key")){
    txs=boost::make_shared<usertxs>(TXSTYPE_BKY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)to_pkey);}
  else{
    fprintf(stderr,"ERROR: run not defined or unknown\n");
    return(NULL);}
  if(txs==NULL){
    return(NULL);}
  txs->sign(sts.ha,sts.sk,sts.pk);
  if(!run.compare("send_new_public_key")){
    if(txs->sign2(to_sign)){
      fprintf(stderr,"ERROR, bad new KEY empty string signature\n");
      return(NULL);}}
  return(txs);
}

//usertxs_ptr run(settings& sts,std::string& line)
usertxs_ptr run(settings& sts,const char* line,int len,bool& dryrun)
{ uint32_t to_bank=0; //actually uint16_t
  uint32_t to_user=0;
   int64_t to_mass=0;
  uint64_t to_info=0;
  uint32_t now=time(NULL);
  dryrun=false;
  if(!strncmp(line,"ME::",4)){ // get info about me
    usertxs_ptr txs(new usertxs(TXSTYPE_INF,sts.bank,sts.user,sts.bank,sts.user,now));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"INF:%X:%X",&to_bank,&to_user)){ // get info about a different user
    usertxs_ptr txs(new usertxs(TXSTYPE_INF,sts.bank,sts.user,to_bank,to_user,now));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"LOG:%X",&now)){ // get info about me and my log since 'now'
    usertxs_ptr txs(new usertxs(TXSTYPE_LOG,sts.bank,sts.user,now)); //now==0xffffffff => fix log file if needed
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(!strncmp(line,"BRO:",4)){ // broadcast message
    usertxs_ptr txs(new usertxs(TXSTYPE_BRO,sts.bank,sts.user,sts.msid,now,len-4,to_user,to_mass,to_info,line+4));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(!strncmp(line,"MPT:",4)){ // send funds to multiple to-accounts
    std::string text;
    if(!parse_mpt(text,to_bank,line,len) || !to_bank){
      return(NULL);}
    usertxs_ptr txs(new usertxs(TXSTYPE_MPT,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,text.c_str()));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"PUT:%X:%X:%lX:%lX",&to_bank,&to_user,&to_mass,&to_info)){ // send funds
    usertxs_ptr txs(new usertxs(TXSTYPE_PUT,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"USR:%X",&to_bank)){ // create new user @ to_bank
    usertxs_ptr txs(new usertxs(TXSTYPE_USR,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(!strncmp(line,"BNK:",4)){ // create new bank
    usertxs_ptr txs(new usertxs(TXSTYPE_BNK,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"GET:%X:%X",&to_bank,&to_user)){ // retreive funds
    usertxs_ptr txs(new usertxs(TXSTYPE_GET,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(!strncmp(line,"KEY:",4)){ // change user key
    if(len<4+2*32+1*2*64){
      fprintf(stderr,"ERROR, bad KEY format; should be:\nKEY:new_public_key:empty_string_signature\n");
      return(NULL);}
    hash_t key;
    ed25519_signature sig;
    ed25519_text2key(key,line+4,32); // do not send last hash
    ed25519_text2key(sig,line+4+2*32+1,64); // do not send last hash
    usertxs_ptr txs(new usertxs(TXSTYPE_KEY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)key));
    txs->sign(sts.ha,sts.sk,sts.pk);
    if(txs->sign2(sig)){
      fprintf(stderr,"ERROR, bad new KEY empty string signature\n");
      return(NULL);}
    return(txs);}
  else if(!strncmp(line,"BKY:",4)){ // change bank key
    hash_t key;
    ed25519_text2key(key,line+4,32); // do not send last hash
    usertxs_ptr txs(new usertxs(TXSTYPE_BKY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)key));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"BLG:%X",&now)){ // get last broadcast log from block before now
    usertxs_ptr txs(new usertxs(TXSTYPE_BLG,sts.bank,sts.user,now)); //now==0xffffffff => fix log file if needed
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else{
    return(NULL);}
}

void print_user(user_t& u)
{  char pkey[64];
   char hash[64];
   ed25519_key2text(pkey,u.pkey,32);
   ed25519_key2text(hash,u.hash,32);
   fprintf(stdout,
     "msid:%08X time:%08X stat:%04X node:%04X user:%08X lpath:%08X rpath:%08X balance:%016lX\npkey:%.64s\nhash:%.64s\n",
     u.msid,u.time,u.stat,u.node,u.user,u.lpath,u.rpath,u.weight,pkey,hash);
}

void print_log(log_t* log,int len)
{ int fd=open("usr.log",O_RDONLY|O_CREAT,0640);
  write(fd,(char*)log,len);
  close(fd);
  if(len%sizeof(log_t)){
    std::cerr<<"ERROR, bad log allignment, viewer not yet implemanted, hex-view usr.log to investigate\n";}
  for(log_t* end=log+len/sizeof(log_t);log<end;log++){
    fprintf(stdout,"%08X ?%04X b%04X u%08X m%08X x%08X y%08X v%016lX\n",
      log->time,log->type,log->node,log->user,log->umid,log->nmid,log->mpos,log->weight);}
}

void save_blg(char* blg,int len,uint32_t path) // blg is 4 bytes longer than len (includes path in 4 bytes)
{ char filename[64];
  sprintf(filename,"bro.%08X",path);
  int fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
  if(len){
    write(fd,(char*)blg,len);}
  close(fd);
}

void talk(boost::asio::ip::tcp::socket& socket,settings& sts,usertxs_ptr txs,bool json,bool dryrun) //len can be deduced from txstype
{ char buf[0xff];
  try{
    txs->print_head();
    txs->print();
    if(dryrun){
      return;}
    boost::asio::write(socket,boost::asio::buffer(txs->data,txs->size));
    if(txs->ttype==TXSTYPE_BLG){
      uint32_t head[2];
      if(2*sizeof(uint32_t)!=boost::asio::read(socket,boost::asio::buffer(head,2*sizeof(uint32_t)))){
        std::cerr<<"ERROR reading broadcast log length\n";
        socket.close();
        return;}
      uint32_t path=head[0];
      int len=(int)head[1];
      char* blg=NULL;
      if(len){
        blg=(char*)malloc(len);//last 4 bytes: the block time of the broadcast log file
        if(blg==NULL){
          fprintf(stderr,"ERROR allocating %08X bytes\n",len);
          socket.close();
          return;}
        if(len!=(int)boost::asio::read(socket,boost::asio::buffer(blg,len))){ // exception will ...
          std::cerr<<"ERROR reading broadcast log\n";
          free(blg);
          socket.close();
          return;}}
      save_blg(blg,len,path);
      free(blg);}
    else{
      int len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
      if(len!=sizeof(user_t)){
        std::cerr<<"ERROR reading confirmation\n";}
      else{
        user_t myuser;
        memcpy(&myuser,buf,sizeof(user_t));
        print_user(myuser);
        if(txs->ttype!=TXSTYPE_INF || ((int)txs->buser==sts.user && (int)txs->bbank==sts.bank)){
          if(txs->ttype!=TXSTYPE_INF && txs->ttype!=TXSTYPE_LOG && (uint32_t)sts.msid+1!=myuser.msid){
            std::cerr<<"ERROR transaction failed (bad msid)\n";}
          if(memcmp(sts.pk,myuser.pkey,32)){
            if(txs->ttype==TXSTYPE_KEY){
              std::cerr<<"PKEY changed\n";}
            else{
              std::cerr<<"PKEY differs\n";}}
          sts.msid=myuser.msid;
          memcpy(sts.ha,myuser.hash,SHA256_DIGEST_LENGTH);}}
      if(txs->ttype==TXSTYPE_INF){
        len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
        if(len!=sizeof(user_t)){
          std::cerr<<"ERROR reading info\n";}
        else{
          user_t myuser;
          memcpy(&myuser,buf,sizeof(user_t));
          print_user(myuser);}}
      else if(txs->ttype==TXSTYPE_LOG){
        int len;
        if(sizeof(int)!=boost::asio::read(socket,boost::asio::buffer(&len,sizeof(int)))){
          std::cerr<<"ERROR reading log length\n";
          socket.close();
          return;}
        if(!len){
          fprintf(stderr,"No new log entries\n");}
        else{
          log_t* log=(log_t*)std::malloc(len);
          if(len!=(int)boost::asio::read(socket,boost::asio::buffer((char*)log,len))){ // exception will cause leak
            std::cerr<<"ERROR reading log\n";
            free(log);
            socket.close();
            return;}
          print_log(log,len);
          free(log);}}
      else{
        struct {uint32_t msid;uint32_t mpos;} m;
        if(sizeof(m)!=boost::asio::read(socket,boost::asio::buffer(&m,sizeof(m)))){
          std::cerr<<"ERROR reading transaction confirmation\n";
          socket.close();
          return;}
        else{
          fprintf(stdout,"MSID:%08X MPOS:%08X\n",m.msid,m.mpos);}}}}
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
  bool connected=true;
  bool json=false;
  bool dryrun=false;
  try{
    if(!sts.exec.empty()){
      usertxs_ptr txs=run(sts,sts.exec.c_str(),sts.exec.length(),dryrun);
      if(txs==NULL){
        std::cerr << "ERROR\n";}
      talk(socket,sts,txs,json,dryrun);
      return(0);}
    //std::string line;
    char line[0x10000];line[0xffff]='\0';
    usertxs_ptr txs;
    //while (std::getline(std::cin,line)){
    while(NULL!=fgets(line,0xffff,stdin)){
      if(line[0]=='\n' || line[0]=='#'){
        continue;}
      int len=strlen(line);
      if(!json && isalpha(line[0])){
        json=false;
        if(line[len-1]=='\n'){
          line[len-1]='\0';
          len--;}
        txs=run(sts,line,len,dryrun);}
      else{
        json=true;
        while(len && 0xffff-len>1 && NULL!=fgets(line+len,0xffff-len,stdin)){
          if(line[len]==',' && line[len+1]=='\n'){
            line[len]=' ';
            break;}
          len+=strlen(line+len);}
        txs=run_json(sts,line,dryrun);}
      if(txs==NULL){
        break;}
      //TODO, send to server
      if(!connected){
        socket.connect(*endpoint_iterator, error);
        if(error){
          std::cerr<<"ERROR connecting again\n";
          break;}}
      talk(socket,sts,txs,json,dryrun);
      socket.close();
      connected=false;}
    if(connected){
      socket.close();}}
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";}
  return 0;
}
