#include "json.h"
#include <string.h>
#include <assert.h>
#include <cstdlib>
#include <iomanip>
#include "settings.hpp"

namespace Helper{

void print_user(user_t& u, boost::property_tree::ptree& pt, bool local, uint32_t bank, uint32_t user, settings& sts)
{
    char pkey[65];
    char hash[65];
    ed25519_key2text(pkey,u.pkey,32);
    ed25519_key2text(hash,u.hash,32);
  pkey[64]='\0';
  hash[64]='\0';
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
//    pt.put("account.date",mydate(u.time));
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
    pt.put("network_account.date",mydate(u.time));
    pt.put("network_account.status",u.stat);
    pt.put("network_account.paired_node",u.node);
    pt.put("network_account.paired_id",u.user);
    if((u.node || u.user) && (u.node != bank || u.user != user)){
      pt.put("network_account.paired_address",ucnt);}
    pt.put("network_account.local_change",u.lpath);
    pt.put("network_account.remote_change",u.rpath);
    pt.put("network_account.balance",print_amount(u.weight));
    pt.put("network_account.public_key",pkey);
    pt.put("network_account.hash",hash);
    if(!check_csum(u,bank,user)){
      pt.put("network_account.checksum","true");}
    else{
      pt.put("network_account.checksum","false");}}
}

#if INTPTR_MAX == INT64_MAX
bool parse_amount(int64_t& amount,std::string str_amount)
{
    long double val;
    if(1!=sscanf(str_amount.c_str(),"%Lf",&val))
    {
        return(false);
    }
    amount=llroundl(val*1000000000.0);
    return(true);
}

char* print_amount(int64_t amount)
{
    static char text[32];
    const long double div=(long double)1.0/(long double)1000000000.0;
    long double val=amount;
    sprintf(text,"%.9Lf",val*div);
    return(text);
}
#elif INTPTR_MAX == INT32_MAX
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
    sprintf(text,"%ld.%09ld",a/1000000000,a%1000000000);}
  else{
    sprintf(text,"-%ld.%09ld",a/1000000000,a%1000000000);}
  //fprintf(stderr,"INT:%20ld STR:%s\n",amount,text);
  return(text);
}
#else
#error Unknown pointer size or missing size macros!
#endif

char* mydate(uint32_t now)
{
    time_t      lnow=now;
    static char text[64];
    struct tm   *tmp;
    tmp=localtime(&lnow);

    if(!strftime(text,64,"%F %T",tmp)){
    text[0]='\0';
    }
    return(text);
}

int check_csum(user_t& u,uint16_t peer,uint32_t uid)
{
    hash_t csum;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,&u,sizeof(user_t)-4*sizeof(uint64_t));
    SHA256_Update(&sha256,&peer,sizeof(uint16_t));
    SHA256_Update(&sha256,&uid,sizeof(uint32_t));
    SHA256_Final(csum,&sha256);
    return(memcmp(csum,u.csum,sizeof(hash_t)));
}


}

