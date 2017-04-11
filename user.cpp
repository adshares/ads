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
#include <boost/optional/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
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
#include <sstream>
#include <stack>
#include <vector>
#include "ed25519/ed25519.h"
#include "user.hpp"
#include "settings.hpp"

/* !!! from main.hpp !!! */
#define BLOCKSEC 0x20 /* block period in seconds */ 
#define TXS_MIN_FEE    (0x1000) /* minimum fee per transaction */
#define TXS_PUT_FEE(x) (0x1000  +((x)>>13)) /* local wires fee (weight) (/8192) */
#define TXS_MPT_FEE(x) (0x100   +((x)>>13)) /* + MIN_FEE !!! */

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

//bool parse_bank(uint16_t& to_bank,boost::optional<std::string>& json_bank)
//{ std::string str_bank=json_bank.get();
bool parse_bank(uint16_t& to_bank,std::string str_bank)
{ char *endptr;
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

//bool parse_user(uint32_t& to_user,boost::optional<std::string>& json_user)
//{ std::string str_user=json_user.get();
bool parse_user(uint32_t& to_user,std::string str_user)
{ char *endptr;
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

/*bool parse_sign(uint8_t* to_sign,boost::optional<std::string>& json_sign)
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
}*/

bool parse_key(uint8_t* to_key,boost::optional<std::string>& json_key,int len)
{ std::string str_key=json_key.get();
  if((int)str_key.length()!=2*len){
    fprintf(stderr,"ERROR: parse_key(%s) bad string length (required %d)\n",str_key.c_str(),2*len);
    return(false);}
  ed25519_text2key(to_key,str_key.c_str(),len);
  return(true);
}

bool parse_amount(int64_t& amount,std::string str_amount)
{ int64_t big=0;//,small=0;
  char small[11]=" 000000000";
  int n=sscanf(str_amount.c_str(),"%ld%10s",&big,small);
  if(n<1){
    fprintf(stderr,"ERROR: parse_amount(%s)\n",str_amount.c_str());
    return(false);}
  if(n==1 || small[0]!='.'){
    amount=big*1000000000;
    //fprintf(stderr,"INT:%20ld STR:%s\n",amount,str_amount.c_str());
    return(true);}
  for(n=1;n<10;n++){
    if(!isdigit(small[n])){
      break;}}
  for(;n<10;n++){
    small[n]='0';}
  if(big>=0){
    amount=big*1000000000+atol(small+1);}
  else{
    amount=big*1000000000-atol(small+1);}
  //fprintf(stderr,"INT:%20ld STR:%s\n",amount,str_amount.c_str());
  return(true);
}

char* print_amount(int64_t amount)
{ static char text[32];
  int64_t a=fabsl(amount);
  if(amount>=0){
    sprintf(text,"%ld.%ld",a/1000000000,a%1000000000);}
  else{
    sprintf(text,"-%ld.%ld",a/1000000000,a%1000000000);}
  //fprintf(stderr,"INT:%20ld STR:%s\n",amount,text);
  return(text);
}

usertxs_ptr run_json(settings& sts,char* line)
{ uint16_t to_bank=0;
  uint32_t to_user=0;
   int64_t to_mass=0;
  //uint64_t to_info=0;
  uint32_t to_from=0;
  uint8_t  to_info[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  uint8_t  to_pkey[32];
  uint8_t  to_sign[64];
  uint32_t now=time(NULL);
  usertxs_ptr txs=NULL;
  std::stringstream ss;
  boost::property_tree::ptree pt;
  ss << line;
  try{
    std::cerr<<line;
    boost::property_tree::read_json(ss,pt);}
  catch (std::exception& e){
    std::cerr << "RUN_JSON Exception: " << e.what() << "\n";}
  boost::optional<std::string> json_sign=pt.get_optional<std::string>("signature");
  if(json_sign && !parse_key(to_sign,json_sign,64)){
    return(NULL);}
  boost::optional<std::string> json_pkey=pt.get_optional<std::string>("pkey");
  if(json_pkey && !parse_key(to_pkey,json_pkey,32)){
    return(NULL);}
  boost::optional<std::string> json_hash=pt.get_optional<std::string>("hash");
  if(json_hash && !parse_key(sts.ha,json_hash,32)){
    return(NULL);}
  boost::optional<std::string> json_mass=pt.get_optional<std::string>("amount");
  if(json_mass && !parse_amount(to_mass,json_mass.get())){
    return(NULL);}
  boost::optional<std::string> json_bank=pt.get_optional<std::string>("node");
  if(json_bank && !parse_bank(to_bank,json_bank.get())){
    return(NULL);}
  boost::optional<std::string> json_user=pt.get_optional<std::string>("id");
  if(json_user && !parse_user(to_user,json_user.get())){
    return(NULL);}
  boost::optional<std::string> json_acnt=pt.get_optional<std::string>("address");
  if(json_acnt && !sts.parse_acnt(to_bank,to_user,json_acnt.get())){
    return(NULL);}
  boost::optional<uint32_t> json_from=pt.get_optional<uint32_t>("from");
  if(json_from){
    to_from=json_from.get();}
  boost::optional<uint32_t> json_msid=pt.get_optional<uint32_t>("msid");
  if(json_msid){
    sts.msid=json_msid.get();}

  std::string run=pt.get<std::string>("run");
  if(!run.compare("get_me")){
    txs=boost::make_shared<usertxs>(TXSTYPE_INF,sts.bank,sts.user,sts.bank,sts.user,now);}
  else if(!run.compare(txsname[TXSTYPE_INF])){
    if(!to_bank && !to_user) { // no target account specified
      txs=boost::make_shared<usertxs>(TXSTYPE_INF,sts.bank,sts.user,sts.bank,sts.user,now);}
    else{
      txs=boost::make_shared<usertxs>(TXSTYPE_INF,sts.bank,sts.user,to_bank,to_user,now);}}
  else if(!run.compare(txsname[TXSTYPE_LOG])){
    sts.lastlog=to_from; //save requested log period
    fprintf(stderr,"LOG: print from %d (%08X)\n",to_from,to_from);
    to_from=0;
    char filename[64];
    mkdir("log",0755);
    sprintf(filename,"log/%04X_%08X.bin",sts.bank,sts.user);
    int fd=open(filename,O_RDONLY);
    if(fd>=0 && lseek(fd,-sizeof(log_t),SEEK_END)>=0){
      read(fd,&to_from,sizeof(uint32_t));
      if(to_from>0){
        to_from--;} // accept 1s overlap
      fprintf(stderr,"LOG: setting last time to %d (%08X)\n",to_from,to_from);}
    txs=boost::make_shared<usertxs>(TXSTYPE_LOG,sts.bank,sts.user,to_from);}
  else if(!run.compare(txsname[TXSTYPE_BLG])){
    txs=boost::make_shared<usertxs>(TXSTYPE_BLG,sts.bank,sts.user,to_from);}
  else if(!run.compare("send_again")){
    boost::optional<std::string> json_data=pt.get_optional<std::string>("data");
    if(json_data){
      std::string data_str=json_data.get();
      int len=data_str.length()/2;
      uint8_t *data=(uint8_t*)malloc(len+1);
      data[len]='\0';
      if(!parse_key(data,json_data,len)){
        free(data);
        return(NULL);}
      txs=boost::make_shared<usertxs>(data,len);
      free(data);
      return(txs);}
    return(NULL);}
  else if(!run.compare(txsname[TXSTYPE_BRO])){
    boost::optional<std::string> json_text_hex=pt.get_optional<std::string>("message");
    if(json_text_hex){
      std::string text_hex=json_text_hex.get();
      int len=text_hex.length()/2;
      uint8_t *text=(uint8_t*)malloc(len+1);
      text[len]='\0';
      if(!parse_key(text,json_text_hex,len)){
        free(text);
        return(NULL);}
      txs=boost::make_shared<usertxs>(TXSTYPE_BRO,sts.bank,sts.user,sts.msid,now,len,to_user,to_mass,to_info,(const char*)text);
      free(text);}
    else{
      boost::optional<std::string> json_text_asci=pt.get_optional<std::string>("message_ascii");
      if(json_text_asci){
         std::string text=json_text_asci.get();
         txs=boost::make_shared<usertxs>(TXSTYPE_BRO,sts.bank,sts.user,sts.msid,now,text.length(),to_user,to_mass,to_info,text.c_str());}
      else{
        return(NULL);}}}
  else if(!run.compare(txsname[TXSTYPE_MPT])){
    uint32_t to_num=0;
    std::string text;
    for(boost::property_tree::ptree::value_type &wire : pt.get_child("wires")){
      uint16_t tbank;
      uint32_t tuser;
      //int64_t tmass = wire.second.data();
       int64_t tmass; // = wire.second.get_value<int64_t>();
      if(!sts.parse_acnt(tbank,tuser,wire.first)){
        return(NULL);}
      if(!parse_amount(tmass,wire.second.get_value<std::string>())){
        return(NULL);}
      char to_acct[2+4+8];
      memcpy(to_acct+0,&tbank,2);
      memcpy(to_acct+2,&tuser,4);
      memcpy(to_acct+6,&tmass,8);
      text.append(to_acct,2+4+8);
      to_num++;}
    if(!to_num){
      return(NULL);}
    txs=boost::make_shared<usertxs>(TXSTYPE_MPT,sts.bank,sts.user,sts.msid,now,to_num,to_user,to_mass,to_info,text.c_str());}
  else if(!run.compare(txsname[TXSTYPE_PUT])){
    boost::optional<std::string> json_info=pt.get_optional<std::string>("message"); // TXSTYPE_PUT only
    if(json_info && !parse_key(to_info,json_info,32)){
      return(NULL);}
    txs=boost::make_shared<usertxs>(TXSTYPE_PUT,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare(txsname[TXSTYPE_USR])){
    txs=boost::make_shared<usertxs>(TXSTYPE_USR,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare(txsname[TXSTYPE_BNK])){
    txs=boost::make_shared<usertxs>(TXSTYPE_BNK,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare(txsname[TXSTYPE_GET])){
    txs=boost::make_shared<usertxs>(TXSTYPE_GET,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);}
  else if(!run.compare(txsname[TXSTYPE_KEY])){
    txs=boost::make_shared<usertxs>(TXSTYPE_KEY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)to_pkey);}
  else if(!run.compare(txsname[TXSTYPE_BKY])){
    txs=boost::make_shared<usertxs>(TXSTYPE_BKY,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)to_pkey);}
  else{
    fprintf(stderr,"ERROR: run not defined or unknown\n");
    return(NULL);}
  if(txs==NULL){
    return(NULL);}
  txs->sign(sts.ha,sts.sk,sts.pk);
  if(!run.compare(txsname[TXSTYPE_KEY])){
    if(txs->sign2(to_sign)){
      fprintf(stderr,"ERROR, bad new KEY empty string signature\n");
      return(NULL);}}
  return(txs);
}

//usertxs_ptr run(settings& sts,std::string& line)
usertxs_ptr run(settings& sts,const char* line,int len)
{ uint32_t to_bank=0; //actually uint16_t
  uint32_t to_user=0;
   int64_t to_mass=0;
  uint64_t to_idat[4]={0,0,0,0};
  uint8_t *to_info=(uint8_t*)to_idat;
  uint32_t now=time(NULL);
  if(!strncmp(line,"ME::",4)){ // get info about me
    usertxs_ptr txs(new usertxs(TXSTYPE_INF,sts.bank,sts.user,sts.bank,sts.user,now));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"INF:%X:%X",&to_bank,&to_user)){ // get info about a different user
    usertxs_ptr txs(new usertxs(TXSTYPE_INF,sts.bank,sts.user,to_bank,to_user,now));
    txs->sign(sts.ha,sts.sk,sts.pk);
    return(txs);}
  else if(sscanf(line,"LOG:%X",&now)){ // get info about me and my log from 'now'
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
  else if(sscanf(line,"PUT:%X:%X:%lX:%lX:%lX:%lX:%lX",&to_bank,&to_user,&to_mass,&to_idat[0],&to_idat[1],&to_idat[2],&to_idat[3])){ // send funds
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

void print_user(user_t& u,boost::property_tree::ptree& pt,bool json,bool local,uint32_t bank,uint32_t user,settings& sts)
{ char pkey[65];
  char hash[65];
  ed25519_key2text(pkey,u.pkey,32);
  ed25519_key2text(hash,u.hash,32);
  pkey[64]='\0';
  hash[64]='\0';
  if(!json){
    fprintf(stderr,
      "msid:%08X time:%08X stat:%04X node:%04X user:%08X lpath:%08X rpath:%08X balance:%016lX\npkey:%.64s\nhash:%.64s\n",
      u.msid,u.time,u.stat,u.node,u.user,u.lpath,u.rpath,u.weight,pkey,hash);}
  else{
    uint16_t suffix=sts.crc_acnt(bank,user);
    char ucnt[19]="";
    char acnt[19];
    sprintf(acnt,"%04X-%08X-%04X",bank,user,suffix);
    if(u.node){
      suffix=sts.crc_acnt(u.node,u.user);
      sprintf(ucnt,"%04X-%08X-%04X",u.node,u.user,suffix);}
    if(local){
      pt.put("account.address",acnt);
      pt.put("account.node",bank);
      pt.put("account.id",user);
      pt.put("account.msid",u.msid);
      pt.put("account.time",u.time);
      pt.put("account.status",u.stat);
      pt.put("account.paired_node",u.node);
      pt.put("account.paired_id",u.user);
      if((u.node || u.user) && (u.node != bank || u.user != user)){
        pt.put("account.paired_address",ucnt);}
      pt.put("account.local_change",u.lpath);
      pt.put("account.remote_change",u.rpath);
      pt.put("account.balance",print_amount(u.weight));
      pt.put("account.public_key",pkey);
      pt.put("account.hash",hash);}
    else{
      pt.put("network_account.address",acnt);
      pt.put("network_account.node",bank);
      pt.put("network_account.id",user);
      pt.put("network_account.msid",u.msid);
      pt.put("network_account.time",u.time);
      pt.put("network_account.status",u.stat);
      pt.put("network_account.paired_node",u.node);
      pt.put("network_account.paired_id",u.user);
      if((u.node || u.user) && (u.node != bank || u.user != user)){
        pt.put("network_account.paired_address",ucnt);}
      pt.put("network_account.local_change",u.lpath);
      pt.put("network_account.remote_change",u.rpath);
      pt.put("network_account.balance",print_amount(u.weight));
      pt.put("network_account.public_key",pkey);
      pt.put("network_account.hash",hash);}}
}

void print_log(boost::property_tree::ptree& pt,settings& sts)
{ char filename[64];
  sprintf(filename,"log/%04X_%08X.bin",sts.bank,sts.user);
  int fd=open(filename,O_RDONLY);
  if(fd<0){
    fprintf(stderr,"ERROR, failed to open log file %s\n",filename);
    return;}
  if(sts.lastlog>1){ //guess a good starting position
    off_t end=lseek(fd,0,SEEK_END);
    if(end>0){
      off_t start=end;
      off_t tseek=0;
      while((start=lseek(fd,(start>(off_t)(sizeof(log_t)*32)?-sizeof(log_t)*32-tseek:-start-tseek),SEEK_CUR))>0){
        uint32_t ltime=0;
        tseek=read(fd,&ltime,sizeof(uint32_t));
        if(ltime<sts.lastlog-1){ // tollerate 1s difference
          lseek(fd,-tseek,SEEK_CUR);
          break;}}}}
  log_t ulog;
  boost::property_tree::ptree logtree;
  while(read(fd,&ulog,sizeof(log_t))==sizeof(log_t)){
    if(ulog.time<sts.lastlog){
fprintf(stderr,"SKIPP %d [%08X]\n",ulog.time,ulog.time);
      continue;}
    char info[65];
    info[64]='\0';
    ed25519_key2text(info,ulog.info,32);
    if(!sts.json){
      fprintf(stderr,"%08X ?%04X b%04X u%08X m%08X x%08X y%08X v%016lX i%64s\n",
        ulog.time,ulog.type,ulog.node,ulog.user,ulog.umid,ulog.nmid,ulog.mpos,ulog.weight,info);}
    else{
      boost::property_tree::ptree logentry;
      uint16_t suffix=sts.crc_acnt(ulog.node,ulog.user);
      char acnt[19];
      uint16_t txst=ulog.type&0xFF;
      sprintf(acnt,"%04X-%08X-%04X",ulog.node,ulog.user,suffix);
      logentry.put("time",ulog.time);
      logentry.put("type_no",ulog.type);
      // FIXME: properly flag confirmed transactions, which will not be rolled back
      logentry.put("confirmed", 1);
      if(txst<TXSTYPE_MAX){
        logentry.put("type",logname[txst]);}
      if(ulog.type & 0x4000){ //errors
        if(txst==TXSTYPE_STP){ //node start
          logentry.put("account.error","logerror");
          logentry.put("account.newtime",ulog.time);
          logentry.put("account.badtime",ulog.nmid);
          logentry.put("account.badblock",ulog.mpos);
          logtree.push_back(std::make_pair("",logentry));
          continue;}}
      if(ulog.type & 0x8000){ //incomming network transactions (responses) except _GET
        if(txst==TXSTYPE_STP){ //node start
          logentry.put("node_start_msid",ulog.nmid);
          logentry.put("node_start_block",ulog.mpos);
          int64_t weight;
          uint8_t hash[8];
          uint32_t lpath;
          uint32_t rpath;
          uint8_t pkey[6];
          uint16_t stat;
          memcpy(&weight,ulog.info+ 0,8);
          memcpy(   hash,ulog.info+ 8,8);
          memcpy( &lpath,ulog.info+16,4);
          memcpy( &rpath,ulog.info+20,4);
          memcpy(   pkey,ulog.info+24,6);
          memcpy(  &stat,ulog.info+30,2);
          char hash_hex[17];hash_hex[16]='\0';
          ed25519_key2text(hash_hex,hash,8);
          char pkey_hex[13];pkey_hex[12]='\0';
          ed25519_key2text(pkey_hex,pkey,6);
          logentry.put("account.balance",print_amount(weight));
          logentry.put("account.local_change",lpath);
          logentry.put("account.remote_change",rpath);
          logentry.put("account.hash_prefix_8",hash_hex);
          logentry.put("account.public_key_prefix_6",pkey_hex);
          logentry.put("account.status",stat);
          logentry.put("account.msid",ulog.umid);
          logentry.put("account.node",ulog.node);
          logentry.put("account.id",ulog.user);
          logentry.put("dividend",print_amount(ulog.weight));
          uint16_t suffix=sts.crc_acnt(ulog.node,ulog.user);
          char acnt[19]="";
          sprintf(acnt,"%04X-%08X-%04X",ulog.node,ulog.user,suffix);
          logentry.put("account.address",acnt);
          logtree.push_back(std::make_pair("",logentry));
          continue;}
        if(txst==TXSTYPE_DIV){ //dividend
          logentry.put("node_msid",ulog.nmid);
          logentry.put("node_block",ulog.mpos);
          logentry.put("dividend",print_amount(ulog.weight));
          logtree.push_back(std::make_pair("",logentry));
          continue;}
        if(txst==TXSTYPE_FEE){ //bank profit
          logentry.put("profit",print_amount(ulog.weight));
          if(ulog.nmid){
            logentry.put("node",ulog.node);
            logentry.put("node_msid",ulog.nmid);
            int64_t fee;
            int64_t div;
            int64_t put;
            memcpy(&fee,ulog.info+ 0,8);
            memcpy(&div,ulog.info+ 8,8);
            memcpy(&put,ulog.info+16,8);
            logentry.put("profit_fee",print_amount(fee));
            logentry.put("profit_div",print_amount(div));
            logentry.put("profit_put",print_amount(put));}
          else{
            logentry.put("node_block",ulog.mpos);
            int64_t div;
            int64_t usr;
            int64_t get;
            int64_t fee;
            memcpy(&div,ulog.info+ 0,8);
            memcpy(&usr,ulog.info+ 8,8);
            memcpy(&get,ulog.info+16,8);
            memcpy(&fee,ulog.info+24,8);
            logentry.put("profit_div",print_amount(div));
            logentry.put("profit_usr",print_amount(usr));
            logentry.put("profit_get",print_amount(get));
            logentry.put("fee",print_amount(fee));}
          logtree.push_back(std::make_pair("",logentry));
          continue;}
        if(txst==TXSTYPE_UOK){ //creare remote account
          logentry.put("node",ulog.node);
          logentry.put("node_block",ulog.mpos);
          if(ulog.user){
            logentry.put("account",ulog.user);
            logentry.put("address",acnt);
            if(ulog.umid){
              logentry.put("request","accepted");}
            else{
              logentry.put("request","late");}}
          else{
            logentry.put("request","failed");
            logentry.put("amount",print_amount(ulog.weight));}
          logentry.put("public_key",info);
          logtree.push_back(std::make_pair("",logentry));
          continue;}
        if(txst==TXSTYPE_BNK){
          logentry.put("node_block",ulog.mpos);
          if(ulog.node){
            logentry.put("node",ulog.node);
            logentry.put("request","accepted");}
          else{
            logentry.put("request","failed");
            logentry.put("amount",print_amount(ulog.weight));}
          logentry.put("public_key",info);
          logtree.push_back(std::make_pair("",logentry));
          continue;}}
      logentry.put("node",ulog.node);
      logentry.put("account",ulog.user);
      logentry.put("address",acnt);
      if(!ulog.nmid && !ulog.mpos){
        logentry.put("node_block",ulog.mpos);}
      else{
        logentry.put("node_msid",ulog.nmid);
        logentry.put("node_mpos",ulog.mpos);
        logentry.put("account_msid",ulog.umid);}
      logentry.put("amount",print_amount(ulog.weight));
      //FIXME calculate fee
      if(txst==TXSTYPE_PUT){
	logentry.put("sender_fee",print_amount(TXS_PUT_FEE(ulog.weight)));
	logentry.put("sender_fee_total",print_amount(TXS_PUT_FEE(ulog.weight)));
        logentry.put("message",info);}
      else{
        int64_t weight;
        int64_t deduct;
        int64_t fee;
        uint16_t stat;
        uint8_t key[6];
        memcpy(&weight,ulog.info+ 0,8);
        memcpy(&deduct,ulog.info+ 8,8);
        memcpy(   &fee,ulog.info+16,8);
        memcpy(  &stat,ulog.info+24,2);
        memcpy(    key,ulog.info+26,6);
        char key_hex[13];key_hex[12]='\0';
        ed25519_key2text(key_hex,key,6);
        logentry.put("sender_balance",print_amount(weight));
        logentry.put("sender_amount",print_amount(deduct));
        if(txst==TXSTYPE_MPT){
          logentry.put("sender_fee",print_amount(TXS_MPT_FEE(ulog.weight)+(key[5]?TXS_MIN_FEE:0)));
          logentry.put("sender_fee_total",print_amount(fee));
          key_hex[2*5]='\0';
          logentry.put("sender_public_key_prefix_5",key_hex);}
        else{
          logentry.put("sender_fee",print_amount(fee));
          logentry.put("sender_public_key_prefix_6",key_hex);}
        logentry.put("sender_status",stat);}
      char tx_id[64];
      if(ulog.type & 0x8000){
        logentry.put("inout","in");
        sprintf(tx_id,"%04X%08X%08X",ulog.node,ulog.nmid,ulog.mpos);}
      else{
        logentry.put("inout","out");
        sprintf(tx_id,"%04X%08X%08X",sts.bank,ulog.nmid,ulog.mpos);}
      logentry.put("id",tx_id);
      logtree.push_back(std::make_pair("",logentry));}}
  if(sts.json){
    pt.add_child("log",logtree);}
}

//void print_blg(int fd,uint32_t path,boost::property_tree::ptree& pt)
void print_blg(int fd,uint32_t path,boost::property_tree::ptree& blogtree,settings& sts)
{ struct stat sb;
  fstat(fd,&sb);
  uint32_t len=sb.st_size;
  //pt.put("length",len);
  if(!len){
    return;}
  char *blg=(char*)malloc(len);
  if(blg==NULL){
    //pt.put("ERROR","broadcast file too large");
    fprintf(stderr,"ERROR, broadcast file malloc error (size:%d)\n",len);
    return;}
  if(read(fd,blg,len)!=len){
    //pt.put("ERROR","broadcast file read error");
    fprintf(stderr,"ERROR, broadcast file read error\n");
    free(blg);
    return;}
  //boost::property_tree::ptree blogtree;
  usertxs utxs;
  for(uint8_t *p=(uint8_t*)blg;p<(uint8_t*)blg+len;p+=utxs.size+32+32+4+4){
    boost::property_tree::ptree blogentry;
    blogentry.put("block_time",path);
    if(!utxs.parse((char*)p) || *p!=TXSTYPE_BRO){
      std::cerr<<"ERROR: failed to parse broadcast transaction\n";
      blogentry.put("ERROR","failed to parse transaction");
      blogtree.push_back(std::make_pair("",blogentry));
      break;}
    blogentry.put("node",utxs.abank);
    blogentry.put("account",utxs.auser);
    uint16_t suffix=sts.crc_acnt(utxs.abank,utxs.auser);
    char acnt[19];
    sprintf(acnt,"%04X-%08X-%04X",utxs.abank,utxs.auser,suffix);
    blogentry.put("address",acnt);
    blogentry.put("account_msid",utxs.amsid);
    blogentry.put("time",utxs.ttime);
    //blogentry.put("length",utxs.bbank);
    //transaction
    char* data=(char*)malloc(2*txslen[TXSTYPE_BRO]+1);
    data[2*txslen[TXSTYPE_BRO]]='\0';
    ed25519_key2text(data,p,txslen[TXSTYPE_BRO]);
    blogentry.put("data",data);
    free(data);
    //message
    char* message=(char*)malloc(2*utxs.bbank+1);
    message[2*utxs.bbank]='\0';
    ed25519_key2text(message,(uint8_t*)utxs.broadcast((char*)p),utxs.bbank);
    blogentry.put("message",message);
    free(message);
    //signature
    char sigh[129];
    sigh[128]='\0';
    ed25519_key2text(sigh,(uint8_t*)utxs.broadcast((char*)p)+utxs.bbank,64);
    blogentry.put("signature",sigh);
    //hash
    char hash[65];
    hash[64]='\0';
    ed25519_key2text(hash,p+utxs.size,32);
    blogentry.put("input_hash",hash);
    //hash
    char pkey[65];
    pkey[64]='\0';
    ed25519_key2text(pkey,p+utxs.size+32,32);
    blogentry.put("public_key",pkey);
    //check signature
    if(utxs.wrong_sig((uint8_t*)p,(uint8_t*)p+utxs.size,(uint8_t*)p+utxs.size+32)){
      blogentry.put("verify","failed");}
    else{
      blogentry.put("verify","passed");}
    //tx_id
    uint32_t nmid;
    uint32_t mpos;
    memcpy(&nmid,p+utxs.size+32+32,sizeof(uint32_t));
    memcpy(&mpos,p+utxs.size+32+32+sizeof(uint32_t),sizeof(uint32_t));
    char tx_id[64];
    sprintf(tx_id,"%04X%08X%08X",utxs.abank,nmid,mpos);
    blogentry.put("node_msid",nmid);
    blogentry.put("node_mpos",mpos);
    blogentry.put("id",tx_id);
    //FIXME calculate fee
    blogentry.put("fee",0);
    blogtree.push_back(std::make_pair("",blogentry));}
  //pt.add_child("broadcast",blogtree);
  free(blg);
}

void out_log(boost::property_tree::ptree& logpt,uint16_t bank,uint32_t user)
{ char filename[64];
  std::ofstream outfile;
  mkdir("out",0755);
  sprintf(filename,"out/%04X_%08X.json",bank,user);
  outfile.open(filename,std::ios::out|std::ios::app);
  boost::property_tree::write_json(outfile,logpt,false);
  outfile.close();
}

void save_log(log_t* log,int len,uint32_t from,settings& sts)
{ char filename[64];
  mkdir("log",0755);
  sprintf(filename,"log/%04X_%08X.bin",sts.bank,sts.user);
  int fd=open(filename,O_RDWR|O_CREAT|O_APPEND,0644);
  if(fd<0){
    fprintf(stderr,"ERROR, failed to open log file %s\n",filename);
    return;}
  std::vector<log_t> logv;
  off_t end=lseek(fd,0,SEEK_END);
  uint32_t maxlasttime=from;
  if(end>0){
    off_t start=end;
    off_t tseek=0;
    while((start=lseek(fd,(start>(off_t)(sizeof(log_t)*32)?-sizeof(log_t)*32-tseek:-start-tseek),SEEK_CUR))>0){
      uint32_t ltime=0;
      tseek=read(fd,&ltime,sizeof(uint32_t));
      if(ltime<from){ // tollerate 1s difference
        lseek(fd,sizeof(log_t)-sizeof(uint32_t),SEEK_CUR);
        break;}}
    log_t ulog;
    while(read(fd,&ulog,sizeof(log_t))==sizeof(log_t)){
      if(ulog.time>=from){
        if(ulog.time>maxlasttime){
          maxlasttime=ulog.time;}
        logv.push_back(ulog);}}}
  for(int l=0;l<len;l+=sizeof(log_t),log++){
    if(log->time>=from && log->time<=maxlasttime){
      for(auto ulog : logv){
        if(!memcmp(&ulog,log,sizeof(log_t))){
          goto NEXT;}}}
    write(fd,log,sizeof(log_t));
    NEXT:;}
  close(fd);
}

bool node_connect(boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,boost::asio::ip::tcp::socket& socket)
{ static bool connected=false;
  boost::system::error_code error = boost::asio::error::host_not_found;
  try{
    if(!connected){
      boost::asio::ip::tcp::resolver::iterator end;
      while (endpoint_iterator != end){
        std::cerr<<"CONNECTING\n";
        socket.connect(*endpoint_iterator, error);
        if(!error){
          break;}
        socket.close();
        endpoint_iterator++;}
      if(error){
        throw boost::system::system_error(error);}
      std::cerr<<"CONNECTED\n";
      connected=true;}
    else{
      socket.connect(*endpoint_iterator, error);
      if(error){
        throw boost::system::system_error(error);}}}
  catch (std::exception& e){
    std::cerr << "Connect Exception: " << e.what() << "\n";
    socket.close();
    return(false);}
  return(true);
}

void talk(boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,boost::asio::ip::tcp::socket& socket,settings& sts,usertxs_ptr txs) //len can be deduced from txstype
{ char buf[0xff];
  try{
    boost::property_tree::ptree pt;
    boost::property_tree::ptree logpt;
    if(!sts.json){
      txs->print_head();
      txs->print();}
    // tx_data
    char* tx_data=(char*)malloc(2*txs->size+1);
    tx_data[2*txs->size]='\0';
    ed25519_key2text(tx_data,txs->data,txs->size);
    pt.put("tx.data",tx_data);
    logpt.put("tx.data",tx_data);
    free(tx_data);
    // account_msid
    pt.put("tx.account_msid",sts.msid);
    logpt.put("tx.account_msid",sts.msid);
    // tx_user_hashin
    // calculate tx_user_hashout
    uint8_t hashout[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    if(txs->ttype<TXSTYPE_INF){
      char tx_user_hashin[65];tx_user_hashin[64]='\0';
      ed25519_key2text(tx_user_hashin,sts.ha,32);
      pt.put("tx.account_hashin",tx_user_hashin);
      logpt.put("tx.account_hashin",tx_user_hashin);
      char tx_user_hashout[65];tx_user_hashout[64]='\0';
      SHA256_CTX sha256;
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,txs->get_sig((char*)txs->data),64);
      SHA256_Final(hashout,&sha256);
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,sts.ha,32);
      SHA256_Update(&sha256,hashout,32);
      SHA256_Final(hashout,&sha256);
      ed25519_key2text(tx_user_hashout,hashout,32);
      pt.put("tx.account_hashout",tx_user_hashout);
      //FIXME calculate fee
      pt.put("tx.fee",0);
    }
    if(sts.msid==1){
      char tx_user_public_key[65];tx_user_public_key[64]='\0';
      ed25519_key2text(tx_user_public_key,sts.pk,32);
      logpt.put("tx.account_public_key",tx_user_public_key);}
    if(sts.drun){
      boost::property_tree::write_json(std::cout,pt,sts.nice);
      return;}
    if(txs->ttype==TXSTYPE_BLG){
      uint32_t path=txs->ttime;
      uint32_t to=time(NULL)-2*BLOCKSEC;
      to+=-(to%BLOCKSEC);
      if(!path){
        path=to;}
      char blockhex[9];
      blockhex[8]='\0';
      sprintf(blockhex,"%08X",to);
      pt.put("to_block_hex",blockhex);
      pt.put("to_block",to);
      sprintf(blockhex,"%08X",path);
      pt.put("from_block_hex",blockhex);
      pt.put("from_block",path);
      boost::property_tree::ptree blogtree;
      for(;path<=to;to-=BLOCKSEC){
        if(txs->ttime!=to){ // straszny burdel :-( tu nie powinno byc tworzenia transakcji
          txs->change_time(to);
          txs->sign(sts.ha,sts.sk,sts.pk);}
        char filename[64];
        sprintf(filename,"bro/%08X.bin",to);
        int fd=open(filename,O_RDONLY);
        if(fd<0){
          try{
            fprintf(stderr,"DEBUG request broadcast from %08X\n",to);
            if(!node_connect(endpoint_iterator,socket)){
              break;}
            boost::asio::write(socket,boost::asio::buffer(txs->data,txs->size));
            uint32_t head[2];
            if(2*sizeof(uint32_t)!=boost::asio::read(socket,boost::asio::buffer(head,2*sizeof(uint32_t)))){
              std::cerr<<"ERROR reading broadcast log length\n";
              socket.close();
              break;}
            to=head[0];
            int len=(int)head[1];
            mkdir("bro",0755);
            int fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0644);
            char* blg=NULL;
            if(len){
              blg=(char*)malloc(len);//last 4 bytes: the block time of the broadcast log file
              if(blg==NULL){
                fprintf(stderr,"ERROR allocating %08X bytes\n",len);
                socket.close();
                break;}
              if(len!=(int)boost::asio::read(socket,boost::asio::buffer(blg,len))){ // exception will ...
                std::cerr<<"ERROR reading broadcast log\n";
                free(blg);
                socket.close();
                break;}
              write(fd,(char*)blg,len);
              free(blg);}
            else{
              fprintf(stderr,"WARNING broadcast for block %08X is empty\n",to);}
            socket.close();}
          catch (std::exception& e){
            std::cerr << "Read Broadcast Exception: " << e.what() << "\n";
            break;}}
        if(fd>=0){
          //print_blg(fd,path,pt);
          print_blg(fd,to,blogtree,sts);
          close(fd);}}
        //else{
        //  pt.put("ERROR","broadcast file missing");}
      pt.add_child("broadcast",blogtree);}
    else{
      if(!node_connect(endpoint_iterator,socket)){
        return;}
      boost::asio::write(socket,boost::asio::buffer(txs->data,txs->size));
      int len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
      if(len!=sizeof(user_t)){
        std::cerr<<"ERROR reading confirmation\n";}
      else{
        user_t myuser;
        memcpy(&myuser,buf,sizeof(user_t));
        if(txs->ttype==TXSTYPE_INF){
          print_user(myuser,pt,sts.json,true,txs->bbank,txs->buser,sts);}
        else{
          print_user(myuser,pt,sts.json,true,sts.bank,sts.user,sts);}
        if(txs->ttype!=TXSTYPE_INF || (txs->buser==sts.user && txs->bbank==sts.bank)){
          if(txs->ttype!=TXSTYPE_INF && txs->ttype!=TXSTYPE_LOG && (uint32_t)sts.msid+1!=myuser.msid){
            std::cerr<<"ERROR transaction failed (bad msid)\n";}
          if(memcmp(sts.pk,myuser.pkey,32)){
            if(txs->ttype==TXSTYPE_KEY){
              std::cerr<<"PKEY changed\n";}
            else{
              char tx_user_public_key[65];tx_user_public_key[64]='\0';
              ed25519_key2text(tx_user_public_key,myuser.pkey,32);
              logpt.put("tx.account_public_key_new",tx_user_public_key);
              std::cerr<<"PKEY differs\n";}}
          if(txs->ttype!=TXSTYPE_INF && txs->ttype!=TXSTYPE_LOG){
            //TODO, validate hashout, check if correct
            char tx_user_hashout[65];tx_user_hashout[64]='\0';
            ed25519_key2text(tx_user_hashout,myuser.hash,32);
            logpt.put("tx.account_hashout",tx_user_hashout);
            if(memcmp(myuser.hash,hashout,32)){
              pt.put("error.text","hash mismatch");}}
          sts.msid=myuser.msid;
          memcpy(sts.ha,myuser.hash,SHA256_DIGEST_LENGTH);}}
      if(txs->ttype==TXSTYPE_INF){
        len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
        if(len!=sizeof(user_t)){
          std::cerr<<"ERROR reading info\n";}
        else{
          user_t myuser;
          memcpy(&myuser,buf,sizeof(user_t));
          print_user(myuser,pt,sts.json,false,txs->bbank,txs->buser,sts);}}
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
          save_log(log,len,txs->ttime,sts);
          free(log);}
        print_log(pt,sts);}
      else{
        struct {uint32_t msid;uint32_t mpos;} m;
        if(sizeof(m)!=boost::asio::read(socket,boost::asio::buffer(&m,sizeof(m)))){
          std::cerr<<"ERROR reading transaction confirmation\n";
          socket.close();
          return;}
        else{
          //sts.msid=m.msid;
          // save outgoing message log
          logpt.put("tx.node_msid",m.msid);
          logpt.put("tx.node_mpos",m.mpos);
          out_log(logpt,sts.bank,sts.user);
          if(sts.json){
            char tx_id[64];
            sprintf(tx_id,"%04X%08X%08X",sts.bank,m.msid,m.mpos);
            pt.put("tx.node_msid",m.msid);
            pt.put("tx.node_mpos",m.mpos);
            pt.put("tx.id",tx_id);}
          else{
            fprintf(stderr,"MSID:%08X MPOS:%08X\n",m.msid,m.mpos);}}}
      socket.close();}
    if(sts.json){
      boost::property_tree::write_json(std::cout,pt,sts.nice);}}
  catch (std::exception& e){
    std::cerr << "Talk Exception: " << e.what() << "\n";
    // exit with error code to enable sane bash scripting
    if(!isatty(fileno(stdin))) {
	    exit(1);
    }
  }
}

//TODO add timeout
//http://www.boost.org/doc/libs/1_52_0/doc/html/boost_asio/example/timeouts/blocking_tcp_client.cpp
int main(int argc, char* argv[])
{ settings sts;
  sts.get(argc,argv);
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query(sts.host,std::to_string(sts.port).c_str());
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
  boost::asio::ip::tcp::socket socket(io_service);
  try{
    if(!sts.exec.empty()){
      usertxs_ptr txs=run(sts,sts.exec.c_str(),sts.exec.length());
      if(txs==NULL){
        std::cerr << "ERROR\n";}
      talk(endpoint_iterator,socket,sts,txs);
      return(0);}
    //std::string line;
    char line[0x10000];line[0xffff]='\0';
    usertxs_ptr txs;
    //while (std::getline(std::cin,line)){
    while(NULL!=fgets(line,0xffff,stdin)){
      if(line[0]=='\n' || line[0]=='#'){
        continue;}
      int len=strlen(line);
      if(!sts.json && isalpha(line[0])){
        sts.json=false;
        if(line[len-1]=='\n'){
          line[len-1]='\0';
          len--;}
        txs=run(sts,line,len);}
      else{
        sts.json=true;
        if(sts.mlin){
          while(len && 0xffff-len>1 && NULL!=fgets(line+len,0xffff-len,stdin)){
            //if(line[len]==',' && line[len+1]=='\n'){
            //  line[len]=' ';
            //  break;}
            len+=strlen(line+len);}}
        txs=run_json(sts,line);}
      if(txs==NULL){
        break;}
      talk(endpoint_iterator,socket,sts,txs);}}
  catch (std::exception& e){
    std::cerr << "Main Exception: " << e.what() << "\n";}
  return 0;
}
