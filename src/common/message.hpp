#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <fcntl.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include "hash.hpp"
#include "user.hpp"


class message :
  public boost::enable_shared_from_this<message>
{
public:
  enum { header_length = 8 }; // should be minimum message size, probably 8bytes
  enum { max_msid = 0xffffffff }; // ignore this later
  enum { max_length = 0xffffff }; // ignore this later
  enum { data_offset = 4+64+10 }; // replace this later

  uint8_t sigh[SHA256_DIGEST_LENGTH]; // hash of signature
  uint8_t preh[SHA256_DIGEST_LENGTH]; // hash of previous signature
  uint32_t len;		// length of data
  uint32_t msid;	// msid from the server
  uint32_t now;		// time message created
  uint32_t got;		// time message received, updated with every download request (busy_insert)
  uint32_t path;	// path == block_id
  uint16_t svid;	// server id of message author
  uint16_t peer;	// server id of peer sending message
  uint8_t* data;	// data pointer
  union {uint64_t num; uint8_t dat[8];} hash; // header hash, TODO change this name to 'head'
  uint8_t status; // |0x1:data |0x2:saved |0x4:valid |0x8:invalid
  //std::set<uint16_t> know; // peers that know about this item
  //boost::container::flat_set<uint16_t> know; // peers that know about this item, flat_set to enable random selection
  std::vector<uint16_t> know; // peers that know about this item, ordered by insert time
  std::set<uint16_t> busy; // peers downloading this item
  std::set<uint16_t> sent; // peers that downloaded this item
  boost::mutex mtx_;	// to update the sets and data

  // can be a request for data , an info , data ...
  // assume head size 8b
  message() :
	len(header_length),
	msid(0),
	now(0),
	got(0),
	path(0),
	svid(0),
        peer(0),
	status(0)
  { data=(uint8_t*)std::malloc(len); //len=8
    hash.num=0;
  }

  message(uint16_t* svidp,uint32_t* msidp,char* sighp,uint16_t mysvid,uint32_t mypath) :
	len(header_length),
	msid(*msidp),
	now(0),
	got(0),
	path(mypath),
	svid(*svidp),
        peer(0),
	status(0)
  { data=(uint8_t*)std::malloc(len); //len=8
    memcpy(sigh,sighp,SHA256_DIGEST_LENGTH);
    memcpy(hash.dat+2,&msid,4);
    memcpy(hash.dat+6,&svid,2);
    memcpy(data+2,&msid,4);
    memcpy(data+6,&svid,2);
    if(msid==0xffffffff){
      hash.dat[0]=0;
      hash.dat[1]=MSGTYPE_DBL;
      *data=MSGTYPE_DBG;}
    else{
      hash.dat[0]=hashval(mysvid);
      hash.dat[1]=MSGTYPE_MSG;
      *data=MSGTYPE_GET;}
    data[1]=hash.dat[0];
  }

  message(uint32_t l) :
	len(l),
	msid(0),
	now(0),
	got(0),
	path(0),
	svid(0),
        peer(0),
	status(MSGSTAT_DAT)
  { data=(uint8_t*)std::malloc(len);
    hash.num=0;
  }

  message(uint32_t l,uint8_t* d) :
	len(l),
	msid(0),
	now(0),
	got(0),
	path(0),
	svid(0),
        peer(0),
	status(MSGSTAT_DAT)
  { data=(uint8_t*)std::malloc(len);
    memcpy(data,d,len);
    hash.num=0;
  }

  message(uint8_t text_type,const uint8_t* text,int text_len,uint16_t mysvid,uint32_t mymsid,ed25519_secret_key mysk,uint8_t* mypk,hash_t msha) : // create from terminal/rpc
	len(data_offset+text_len),
	msid(mymsid),
	path(0),
	svid(mysvid),
	peer(mysvid),
	status(MSGSTAT_DAT)
  { data=(uint8_t*)std::malloc(len);
    now=time(NULL);
    path=now-(now%BLOCKSEC);
    got=now;
    assert(mymsid<=max_msid); // this server can not send any more messages
    assert(len<=max_length);
    assert(data!=NULL);
    assert(text_type==MSGTYPE_INI || mysvid); // READONLY ok
    data[0]=text_type;
    memcpy(data+1,&len,3); //assume bigendian :-)
    //this will be the signature
    memcpy(data+4+64+0,&mysvid,2);
    memcpy(data+4+64+2,&mymsid,4);
    memcpy(data+4+64+6,&now,4);
    memcpy(data+4+64+10,text,text_len);
    if(text_type==MSGTYPE_BLK){
      if(mysk==NULL){ // creating message from network
        memcpy(data+4,mypk,64);}
      else{ //not signing vok and vno in header_t
        ed25519_sign(data+4+64+10,sizeof(header_t)-4,mysk,mypk,data+4);} // consider signing also svid,msid,0
      char hash[4*SHA256_DIGEST_LENGTH];
      ed25519_key2text(hash,data+4,2*SHA256_DIGEST_LENGTH);
      hash_signature();
      DLOG("BLOCK SIGNATURE created %.*s (%d)\n",4*SHA256_DIGEST_LENGTH,hash,mysvid);}
    else if(text_type==MSGTYPE_CND){
      ed25519_sign(data+4+64,10+sizeof(hash_t),mysk,mypk,data+4);
      hash_signature();}
    else if(text_type==MSGTYPE_INI){
      //DLOG("INI:%016lX:%016lX\n",*(uint64_t*)mypk,*(uint64_t*)mysk);
      if(mysvid){ // READONLY ok
        ed25519_sign(data+4+64,10+text_len,mysk,mypk,data+4);}
      else{
        bzero(data+4,64);}
      hash_signature();}
    else{
      assert(text_type==MSGTYPE_MSG);
      if(!insert_user()){
        ELOG("ERROR insert_user error, FATAL\n");
        exit(-1);}
      if(!hash_tree()){
        ELOG("ERROR hash_tree error, FATAL\n");
        exit(-1);}
      ed25519_sign2(msha,32,sigh,32,mysk,mypk,data+4);}
    hash.num=dohash(mysvid);
  }

  message(uint8_t type,uint32_t mpath,uint16_t msvid,uint32_t mmsid,hash_t svpk,hash_t msha) : //recycled message
	len(header_length),
	msid(mmsid),
	path(mpath),
	svid(msvid),
	peer(msvid),
	status(0)
  { data=NULL;
    hash.dat[1]=type;
    if(!load(msvid)){ //sets len ... assume this is invoked only by server during sync
      ELOG("ERROR, failed to load recycled message %04X:%08X [len:%d]\n",svid,msid,len);
      return;}
    //load should get hash_tree
    memcpy(&now,data+4+64+6,4);
    got=now; //TODO, if You plan resubmission check if the message is not too old and recreate if needed
    assert(mmsid<=max_msid);
    assert(len<=max_length);
    assert(data!=NULL);
    if(check_signature(svpk,msvid,msha)){
      ELOG("ERROR, failed to confirm signature of recycled message %04X:%08X [len:%d]\n",svid,msid,len);
      status=0;}
  }

  message(uint8_t type,uint32_t mpath,uint16_t msvid,uint32_t mmsid) : //loaded message
	len(header_length),
	msid(mmsid),
	path(mpath),
	svid(msvid),
	peer(msvid),
	status(0)
  { data=NULL;
    hash.dat[1]=type;
  }

  ~message()
  { if(data!=NULL){
      free(data);
      data=NULL;}
  }

  //TODO, compute hashsvid,hashmsid,hashtnum as in hash_tree()
  bool hash_tree_fast(uint8_t* outsigh,uint8_t* indata,uint32_t inlen,uint16_t insvid,uint32_t inmsid)
  { assert(indata[0]==MSGTYPE_MSG); //FIXME, maybe killed by unload !!!
    assert(insvid); //FIXME, insvid==0 for block message
    assert(inmsid);
    hash_t hash; //strict aliasing ???
    uint16_t* hashsvid=(uint16_t*)hash;
    uint32_t* hashmsid=(uint32_t*)(&hash[4]);
    uint16_t* hashtnum=(uint16_t*)(&hash[8]);
    hashtree tree;
    usertxs utxs;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,indata+4+64,10);
    SHA256_Final(hash,&sha256);
    tree.update(hash);
    uint8_t* p=indata+data_offset;
    uint8_t* end=indata+inlen;
    uint32_t l;
    assert(p<end);
    uint16_t tnum=1;
    for(;p<end;p+=l,tnum++){
      l=utxs.get_size((char*)p);
      if(l==0xFFFFFFFF){
        return(false);}
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,p,l);
      SHA256_Final(hash,&sha256);
      *hashsvid^=insvid;
      *hashmsid^=inmsid;
      *hashtnum^=tnum;
      tree.update(hash);}
    if(!tnum){
      DLOG("ERROR empty hash_tree %04X:%08X\n",insvid,inmsid);
      return(false);}
    if(p!=end){
      DLOG("ERROR parsing transactions for hash_tree %04X:%08X\n",insvid,inmsid);
      return(false);}
    tree.finish(outsigh);
    return(true);
  }

  bool get_user(uint32_t user,user_t& u) //expected to be used rarely, not optimised (fd could be provided)
  { char filename[64];
    sprintf(filename,"usr/%04X.dat",svid);
    int fd=open(filename,O_RDONLY);
    if(fd<0){
      return(false);}
    lseek(fd,user*sizeof(user_t),SEEK_SET);
    read(fd,&u,sizeof(user_t));
    close(fd);
    uint32_t block=now-now%BLOCKSEC;
    for(;block<=path;block+=BLOCKSEC){
      sprintf(filename,"blk/%03X/%05X/und/%04X.dat",block>>20,block&0xFFFFF,svid);
      int fd=open(filename,O_RDONLY);
      if(fd<0){
        continue;}
      user_t ou;
      lseek(fd,user*sizeof(user_t),SEEK_SET);
      read(fd,&ou,sizeof(user_t));
      close(fd);
      if(ou.msid){
        memcpy(&u,&ou,sizeof(user_t));
        break;}}
    return((bool)u.msid);
  }

  bool insert_user()
  { usertxs utxs;
    uint8_t* p=data+data_offset; // data_offset = 4+64+10
    uint8_t* end=data+len;
    uint32_t l;
    assert(p<end);
    for(;p<end;p+=l){
      l=utxs.get_size((char*)p);
      if(*p==TXSTYPE_SAV){
        uint32_t auser=*(uint32_t*)(p+3); // could be a user.hpp function
        //if(!get_user(auser,*(user_t*)(p+txslen[TXSTYPE_SAV]+64)))
        if(!get_user(auser,*(user_t*)utxs.usr((char*)p))){
          return(false);}}}
    assert(p==end);
    return(true);
  }

  bool hash_tree()
  { assert(data!=NULL);
    assert(data[0]==MSGTYPE_MSG); //FIXME, maybe killed by unload !!!
    assert(svid); //FIXME, svid==0 for block message
    assert(msid);
    hash_s ha;
    hashtree tree;
    usertxs utxs;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,data+4+64,10);
    SHA256_Final(ha.hash,&sha256);
    std::vector<uint32_t> tpos;
    std::vector<hash_s> firsthashes;
    tpos.push_back(4+64);
    firsthashes.push_back(ha);
    tree.update(ha.hash);
    uint8_t* p=data+data_offset; // data_offset = 4+64+10
    uint8_t* end=data+len;
    uint32_t l;
    assert(p<end);
    uint16_t tnum=1;
    for(;p<end;p+=l,tnum++){
      tpos.push_back(p-data);
      l=utxs.get_size((char*)p);
      if(l==0xFFFFFFFF){
        return(false);}
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,p,l);
      SHA256_Final(ha.hash,&sha256);
      //xor with txid
      ha.hxor[0]^=svid;
      ha.hxor[1]^=msid;
      ha.hxor[2]^=tnum;
      firsthashes.push_back(ha);
      tree.update(ha.hash);}
    if(!tnum){
      DLOG("ERROR empty hash_tree %04X:%08X\n",svid,msid);
      return(false);}
    if(p!=end){
      DLOG("ERROR parsing transactions for hash_tree %04X:%08X\n",svid,msid);
      return(false);}
    tree.finish(sigh);
    uint32_t tpos_size=tpos.size();
    uint32_t hashes_size=tree.hashes.size()-1; // last is msghash
    if(!tree.hashes.size()){
      hashes_size=0;}
    uint32_t total=len+32+4+4+4+(4+32)*tpos_size+32*hashes_size;
    data=(uint8_t*)std::realloc(data,total);
    memcpy(data+len,sigh,32);
    bzero(data+len+32,4); //will be overwritten later with mnum (id of message in block)
    memcpy(data+len+32+4,&total,4);
    memcpy(data+len+32+4+4,&tpos_size,4);
    for(uint32_t i=0;i<tpos_size;i++){
      memcpy(data+len+32+4+4+4+(4+32)*i,&tpos[i],4);
      memcpy(data+len+32+4+4+4+(4+32)*i+4,firsthashes[i].hash,32);}
    for(uint32_t i=0;i<hashes_size;i++){
      memcpy(data+len+32+4+4+4+(4+32)*tpos_size+32*i,&tree.hashes[i],32);}
    return(true);
  }

  bool hash_tree_get(uint32_t tnum,std::vector<hash_s>& hashes,uint32_t& mnum)
  {
    char filename[128];
    assert(hashtype()==MSGTYPE_MSG);
    makefilename(filename,path,"msg");
    //sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.msg",path>>20,path&0xFFFFF,MSGTYPE_MSG,svid,msid);
    int fd=open(filename,O_RDONLY);
    if(fd<0){
      DLOG("ERROR %s not found\n",filename);
      return(false);}
    uint32_t mlen;
    read(fd,&mlen,4);
    mlen>>=8;
    if(!mlen){
      DLOG("ERROR %s failed to read message length\n",filename);
      close(fd);
      return(false);}
    hash_t mhash;
    uint32_t ttot;
    uint32_t tmax;
    lseek(fd,mlen,SEEK_SET);
    read(fd,mhash,32);
    read(fd,&mnum,4);
    read(fd,&ttot,4); // not needed
    read(fd,&tmax,4);
    //struct stat sb;
    //fstat(fd,&sb);
    //assert(sb.st_size==ttot);
    if(!tmax){
      DLOG("ERROR %s failed to read number of hashes\n",filename);
      close(fd);
      return(false);}
    if(tnum>=tmax){
      DLOG("ERROR %s pos too high (%d>=%d)\n",filename,tnum,tmax);
      close(fd);
      return(false);}
    uint32_t pos;
    if(tnum%2){
      uint32_t tmp[8+1+8+1];
      lseek(fd,mlen+32+4+4+4+(4+32)*tnum-32,SEEK_SET);
      read(fd,tmp,32+4+32+4);
      pos=tmp[8];
      if(tnum==tmax-1){
        len=mlen-pos;}
      else{
        len=tmp[8+1+8]-pos;}
      hashes.push_back(*(hash_s*)(&tmp[8+1])); //add message hash to hashes
      DLOG("HASHTREE start %d + %d [max:%d mlen:%d ttot:%d len:%d]\n",tnum,tnum-1,tmax,mlen,ttot,len);
      hashes.push_back(*(hash_s*)(&tmp[0]));}
    else{
      uint32_t tmp[1+8+1+8];
      lseek(fd,mlen+32+4+4+4+(4+32)*tnum,SEEK_SET);
      read(fd,tmp,4+32+4+32);
      pos=tmp[0];
      hashes.push_back(*(hash_s*)(&tmp[1])); //add message hash to hashes
      if(tnum==tmax-1){
        len=mlen-pos;
        DLOG("HASHTREE start %d [max:%d len:%d]\n",tnum,tmax,len);}
      else{
        len=tmp[1+8]-pos;
        DLOG("HASHTREE start %d + %d [max:%d mlen:%d ttot:%d len:%d]\n",tnum,tnum+1,tmax,mlen,ttot,len);
        hashes.push_back(*(hash_s*)(&tmp[1+8+1]));}}
    if(data!=NULL){ // assume there is no concurent access to this message
      free(data);}
    assert(len>0 && len<mlen);
    data=(uint8_t*)std::malloc(len);
    lseek(fd,pos,SEEK_SET);
    read(fd,data,len); //read message
    std::vector<uint32_t>add;
    hashtree tree;
    tree.hashpath(tnum/2,(tmax+1)/2,add);
    for(auto n : add){
      DLOG("HASHTREE add %d\n",n);
      if(n*2==tmax-1){ //special case for last uneven hash
        lseek(fd,mlen+32+4+4+4+(4+32)*tmax-32,SEEK_SET);}
      else{
        assert(mlen+32+4+4+4+(4+32)*tmax+32*n<ttot);
        lseek(fd,mlen+32+4+4+4+(4+32)*tmax+32*n,SEEK_SET);}
      hash_s phash;
      read(fd,phash.hash,32);
      hashes.push_back(phash);}
    close(fd);
    //DEBUG only, confirm hash 
    hash_t nhash;
    tree.hashpathrun(nhash,hashes);
    if(memcmp(mhash,nhash,32)){
      DLOG("HASHTREE failed (path len:%d)\n",(int)hashes.size());
      return(false);}
    return(true);
  }

  void signnewtime(uint32_t ntime,ed25519_secret_key mysk,ed25519_public_key mypk,hash_t msha)
  { assert(data[0]==MSGTYPE_MSG);
    memcpy(data+4+64+6,&ntime,4);
    now=ntime;
    if(!insert_user()){
      ELOG("ERROR insert_user error, FATAL\n");
      exit(-1);}
    if(!hash_tree()){
      ELOG("ERROR hash_tree error, FATAL\n");
      exit(-1);}
    ed25519_sign2(msha,32,sigh,32,mysk,mypk,data+4);
    hash.num=dohash(svid); //assert svid==opts_.svid
  }

  void hashtype(uint8_t type)
  { hash.dat[1]=type;
  }

  uint8_t hashtype()
  { return((uint8_t)hash.dat[1]);
  }

  uint8_t hashval(uint16_t mysvid)
  { //return(data[4+(mysvid%64)]);
    assert(0xffffffffffffffff!=*((uint64_t*)(sigh))||0xffffffffffffffff!=*((uint64_t*)(sigh+8))||0xffffffffffffffff!=*((uint64_t*)(sigh+16))||0xffffffffffffffff!=*((uint64_t*)(sigh+24)));//FIXME, remove later
    return(sigh[mysvid%SHA256_DIGEST_LENGTH]);
  }

  uint64_t dohash(void) // default double_spend type
  { union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0;
    h.dat[1]=MSGTYPE_DBL;
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    return(h.num);
  }

  uint64_t dohash(uint16_t mysvid)
  { assert(data!=NULL);
    union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=hashval(mysvid);
    h.dat[1]=data[0];
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    return(h.num);
  }

  uint64_t dohash(uint8_t* d)
  { union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=d[1];
    h.dat[1]=*d;
    if(*d==MSGTYPE_PUT||*d==MSGTYPE_GET){
      h.dat[1]=MSGTYPE_MSG;}
    if(*d==MSGTYPE_CNP||*d==MSGTYPE_CNG){
      h.dat[1]=MSGTYPE_CND;}
    if(*d==MSGTYPE_BLP||*d==MSGTYPE_BLG){
      h.dat[1]=MSGTYPE_BLK;}
    if(*d==MSGTYPE_USG){
      h.dat[1]=MSGTYPE_USR;}
    if(*d==MSGTYPE_DBP||*d==MSGTYPE_DBG){
      h.dat[1]=MSGTYPE_DBL;}
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    return(h.num);
  }

  void dblhash(uint16_t mysvid)
  { svid=mysvid;
    msid=0xffffffff;
    hash.num=0x0000FFFFFFFF0000L;
    //hash.dat[0]=0;
    //hash.dat[1]=0;
    //memcpy(hash.dat+2,&msid,4);
    memcpy(hash.dat+6,&svid,2);
    bzero(sigh,32);
    memcpy(sigh,hash.dat,8);
  }

  // parse header, this should go to peer so that peer can react faster
  int header(uint32_t peer_svid)
  { 
    assert(know.empty());
    assert(busy.empty());
    assert(sent.empty());
    got=time(NULL);
    assert(data!=NULL);
    peer=peer_svid; // set source of message
    if(data[0]==MSGTYPE_INI){
      len=0;
      memcpy(&len,data+1,3);
      if(len!=4+64+10+sizeof(handshake_t)){
        DLOG("ERROR: no handshake \n"); // TODO, ban ip
        return 0;}
      data=(uint8_t*)std::realloc(data,len);
      if(data==NULL){
        ELOG("ERROR: realloc failed \n");
        return 0;}
      return 2;}
    //do these checks later
    //if(!peer_svid){ // peer not authenticated yet READONLY ok
    //  DLOG("ERROR: peer not authenticated\n"); // TODO, ban ip
    //  return 0;}
    if( data[0]==MSGTYPE_PUT||
        data[0]==MSGTYPE_GET||
        data[0]==MSGTYPE_CNP||
        data[0]==MSGTYPE_CNG||
        data[0]==MSGTYPE_BLP||
        data[0]==MSGTYPE_BLG||
        data[0]==MSGTYPE_USG||
        data[0]==MSGTYPE_DBP||
        data[0]==MSGTYPE_DBG){
      memcpy(&msid,data+2,4);
      memcpy(&svid,data+6,2);
      hash.num=dohash(data);
      len=header_length;
      return 1;} // short message
    if(data[0]==MSGTYPE_MSG||data[0]==MSGTYPE_DBL||data[0]==MSGTYPE_CND||data[0]==MSGTYPE_BLK){
      len=0;
      memcpy(&len,data+1,3);
      if((data[0]==MSGTYPE_MSG && len>max_length) || (data[0]==MSGTYPE_DBL && len>4+2*max_length) || len<=4+64+10){ // bad format
        ELOG("ERROR in message format >>%016lX>>\n",*(uint64_t*)data);
        return 0;}
      data=(uint8_t*)std::realloc(data,len);
      if(data==NULL){
        ELOG("ERROR: failed to allocate memory\n");
        return 0;}
      return 2;}
    if(data[0]==MSGTYPE_USR){
      msid=0;
      len=0;
      memcpy(&len,data+1,3); // this is number of users (max 0x10000)
      memcpy(&msid,data+4,2); // this is the chunk id
      memcpy(&svid,data+6,2); // this is the bank id
      if(len>MESSAGE_CHUNK){
        uint64_t h=*((uint64_t*)data);
        ELOG("ERROR USR HEADER:%016lX too long (%u>%u)\n",h,len,MESSAGE_CHUNK);
        return 0;}
      data=(uint8_t*)std::realloc(data,8+len*sizeof(user_t));
      if(data==NULL){
        DLOG("ERROR: failed to allocate memory\n");
        return 0;}
      return 2;}
    if(data[0]==MSGTYPE_STP){
      DLOG("STOP header received\n");
      svid=peer_svid;
      len=SHA256_DIGEST_LENGTH+1;
      data=(uint8_t*)std::realloc(data,len);
      return 2;}
    if(data[0]==MSGTYPE_SER){
      DLOG("SERVERS request header received\n");
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_HEA){
      DLOG("HEADERS request header received\n");
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_PAT){
      DLOG("SYNCBLOCK time received\n");
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_MSL){
      DLOG("TXSLIST request header received\n");
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_MSP){
      DLOG("TXSLIST data header received\n");
      svid=peer_svid;
      len=0;
      memcpy(&len,data+1,3);
      data=(uint8_t*)std::realloc(data,len);
      return 1;}
    if(data[0]==MSGTYPE_NHR){
      DLOG("NEXT header request received\n");
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_NHD){
      DLOG("NEXT header data received\n");
      svid=peer_svid;
      len=SHA256_DIGEST_LENGTH+sizeof(headlink_t)+8;
      data=(uint8_t*)std::realloc(data,len);
      return 1;}
    if(data[0]==MSGTYPE_SOK){
      DLOG("SYNC OK received\n");
      svid=peer_svid;
      len=header_length;
      return 1;} // short message
    ELOG("ERROR: unknown message header %016lX\n",(uint64_t)(*(uint64_t*)data));
    return 0;
  }

  int sigh_check() // check signature of loaded message
  { if(hash.dat[1]==MSGTYPE_MSG){
      assert(data[0]==MSGTYPE_MSG);
      return(memcmp(sigh,data+len,SHA256_DIGEST_LENGTH));}
      //if(!hash_tree((uint8_t*)h)){
      //  return(-1);}}
    uint64_t h[4];
    uint64_t g[8];
    assert(data!=NULL);
    memcpy(g,data+4,8*sizeof(uint64_t));
    h[0]=g[0]^g[4];
    h[1]=g[1]^g[5];
    h[2]=g[2]^g[6];
    h[3]=g[3]^g[7];
    return(memcmp(sigh,h,SHA256_DIGEST_LENGTH));
  }
  void null_signature()
  { bzero(sigh,SHA256_DIGEST_LENGTH); 
  }
  void hash_signature() // FIXME, do a different signature for _MSG
  { uint64_t h[4];
    uint64_t g[8];
    //memcpy(g,sig,8*sizeof(uint64_t));
    assert(data!=NULL);
    assert(data[0]!=MSGTYPE_MSG);
    if(data[0]==MSGTYPE_DBL){
      null_signature();
      return;}
    memcpy(g,data+4,8*sizeof(uint64_t));
    h[0]=g[0]^g[4];
    h[1]=g[1]^g[5];
    h[2]=g[2]^g[6];
    h[3]=g[3]^g[7];
    memcpy(sigh,h,SHA256_DIGEST_LENGTH);
  }

  void read_head(void)
  { assert(data!=NULL);
    if(data[0]==MSGTYPE_MSG || data[0]==MSGTYPE_INI || data[0]==MSGTYPE_CND || data[0]==MSGTYPE_DBL || data[0]==MSGTYPE_BLK){
      memcpy(&svid,data+4+64+0,2);
      memcpy(&msid,data+4+64+2,4);
      memcpy( &now,data+4+64+6,4);
      return;}
    //if(data[0]==MSGTYPE_USR){ // double message //TODO untested !!!
    //  memcpy(&svid,data+4+0,2);
    //  memcpy(&msid,data+4+2,4);}
    if(data[0]==MSGTYPE_DBL){ // double message //TODO untested !!!
      memcpy(&svid,data+4+32+4+64+0,2);
      memcpy(&msid,data+4+32+4+64+2,4);
      memcpy( &now,data+4+32+4+64+6,4);
      return;}
    ELOG("ERROR, getting unexpected message type in read_head :-( \n");
    //exit(-1);
  }

  int check_signature(const uint8_t* svpk,uint16_t mysvid,const uint8_t* msha)
  { assert(data!=NULL);
    status|=MSGSTAT_DAT; // have data
    if(data[0]==MSGTYPE_MSG){
      //hash_tree() calculate in handle_read_body()
      hash.num=dohash(mysvid);
      return(ed25519_sign_open2(msha,32,sigh,32,svpk,data+4));}
    if(data[0]==MSGTYPE_INI || data[0]==MSGTYPE_CND || data[0]==MSGTYPE_BLK){
      //hash_signature(); calculated in handle_read_body()
      hash.num=dohash(mysvid);
      if(data[0]==MSGTYPE_BLK){
	if(memcmp(data+4+64+2,data+4+64+10,4)){ //WARNING, 'now' must be first element of header_t
	  ELOG("ERROR, BLK message %04X:%08X msid error\n",svid,msid);
	  return(1);}
        return(ed25519_sign_open(data+4+64+10,sizeof(header_t)-4,svpk,data+4));}
      if(data[0]==MSGTYPE_CND){ //FIXME, consider changing the signature format
        return(ed25519_sign_open(data+4+64,10+sizeof(hash_t),svpk,data+4));}
      if(data[0]==MSGTYPE_INI){
DLOG("INI:%016lX\n",*(uint64_t*)svpk);
        return(ed25519_sign_open(data+4+64,len-4-64,svpk,data+4));}
      assert(0);}
    if(data[0]==MSGTYPE_DBL){ // double message //TODO untested !!!
      hash.num=dohash();
      null_signature();
      uint32_t len1=0,len2=0,msid1,msid2,now1,now2;
      uint16_t svid1,svid2;
      uint8_t *data1,*data2; 
      hash_t dblmsha;
      memcpy(dblmsha,data+4,32);
      data1=data+4+32;
      memcpy(&len1,data1+1,3);
      if(len<4+32+len1+4){
        ELOG("DBL message short len\n");
        return(-1);}
      data2=data1+len1;
      memcpy(&len2,data2+1,3);
      if(4+32+len1+len2!=len){
        ELOG("DBL message bad len 4+32+%d+%d!=%d\n",len1,len2,len);
        return(-1);}
      if(*data1!=*data2){
        ELOG("DBL message type mismatch\n");
        return(-1);}
      memcpy(&svid1,data1+4+64+0,2); //was processed before by read_head()
      memcpy(&msid1,data1+4+64+2,4); //was processed before by read_head()
      memcpy( &now1,data1+4+64+6,4); //was processed before by read_head()
      memcpy(&svid2,data2+4+64+0,2);
      memcpy(&msid2,data2+4+64+2,4);
      memcpy( &now2,data2+4+64+6,4);
      if((svid1!=svid2)||(msid1!=msid2)){ // bad format
        ELOG("DBL message msid/svid mismatch\n");
        return(-1);}
      if(!memcmp(data1+4,data2+4,64)){ // equal messages
        ELOG("DBL message equal signatures\n");
        return(-1);}
      if(*data1==MSGTYPE_CND || *data1==MSGTYPE_BLK){
        if(len1!=len2){
          ELOG("DBL message len mismatch\n");
          return(-1);}
        if(*data1==MSGTYPE_CND){
          if(len1!=4+64+10+32){
            ELOG("DBL message bad CND len\n");
            return(-1);}
          const uint8_t* m[2]={data1+4+64,data2+4+64};
          size_t mlen[2]={10+32,10+32};
          const uint8_t* pk[2]={svpk,svpk};
          const uint8_t* rs[2]={data1+4,data2+4};
          int valid[2];
          return(ed25519_sign_open_batch(m,mlen,pk,rs,2,valid));}
        if(*data1==MSGTYPE_BLK){
          if(len1!=4+64+10+sizeof(header_t)-4){
            ELOG("DBL message bad BLK len\n");
            return(-1);}
          uint32_t hpath1=((header_t*)(data1+4+64+10))->now;
          uint32_t hpath2=((header_t*)(data1+4+64+10))->now;
          if(hpath1!=hpath2){
            ELOG("DBL message header mistamch\n");
            return(-1);}
          if(hpath1!=msid1){
            ELOG("DBL bad message header\n");
            return(-1);}
          const uint8_t* m[2]={data1+4+64+10,data2+4+64+10};
          size_t mlen[2]={sizeof(header_t)-4,sizeof(header_t)-4};
          const uint8_t* pk[2]={svpk,svpk};
          const uint8_t* rs[2]={data1+4,data2+4};
          int valid[2];
          return(ed25519_sign_open_batch(m,mlen,pk,rs,2,valid));}}
      if(*data1==MSGTYPE_MSG){
        if((now1>now2+3*BLOCKSEC) || (now2>now1+3*BLOCKSEC)){ // time based protection for fixed messages
          ELOG("DBL bad message times %08X vs %08X\n",now1,now2);
          return(-1);}
        hash_t sigh1;
        hash_t sigh2;
        if(!hash_tree_fast(sigh1,data1,len1,svid1,msid1)){
          ELOG("DBL bad message hash_tree_fast 1 failed\n");
          return(-1);}
        if(!hash_tree_fast(sigh2,data2,len2,svid2,msid2)){
          ELOG("DBL bad message hash_tree_fast 1 failed\n");
          return(-1);}
        return(
          ed25519_sign_open2(dblmsha,32,sigh1,32,svpk,data1+4) ||
          ed25519_sign_open2(dblmsha,32,sigh2,32,svpk,data2+4));}
      ELOG("DBL message illegal type\n");
      return(-1);}
    return(1); //return error
  }

  void print_text(const char* suffix)
  { 
#ifndef NDEBUG
    assert(data!=NULL);
    char text[16];
    ed25519_key2text((char*)text,sigh,8);
    DLOG("%04X [%04X:%08X] [l:%d] (%08X) %.16s blk/%03X/%05X/%02x_%04x_%08x.msg %s\n",peer,svid,msid,len,now,text,path>>20,path&0xFFFFF,hashtype(),svid,msid,suffix);
#endif
  }

  void print(const char* suffix) const
  { DLOG("%04X [%04X-%08X] [l:%d] (%08X) %s\n",peer,svid,msid,len,now,suffix);
  }

  void print_header()
  { assert(data!=NULL);
    char hash[2*SHA256_DIGEST_LENGTH];
    header_t* h=(header_t*)(data+4+64+10);
    ELOG("HEADER: now:%08x msg:%08x nod:%d\n",h->now,h->msg,h->nod);
    ed25519_key2text(hash,h->oldhash,32);
    ELOG("OLDHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    ed25519_key2text(hash,h->msghash,32);
    ELOG("TXSHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    ed25519_key2text(hash,h->nodhash,32);
    ELOG("NODHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    ed25519_key2text(hash,h->nowhash,32);
    ELOG("NOWHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
  }

  uint32_t load(int16_t who) //FIXME, this is not processing the data correctly, check scenarios
  { 
    mtx_.lock();
    busy.insert(who);
    if(!path){
      mtx_.unlock();
#ifndef NDEBUG
      if(data==NULL){
        char filename[128];
        makefilename(filename,path,"msg");
        DLOG("%s lost [len:%d]\n",filename,len);}
#endif
      return(data!=NULL);}
    if(data!=NULL && len!=header_length){
      mtx_.unlock();
#ifndef NDEBUG
      char filename[128];
      makefilename(filename,path,"msg");
      DLOG("%s full [len:%d]\n",filename,len);
#endif
      return(1);}
    char filename[128];
    makefilename(filename,path,"msg");
    //sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.msg",path>>20,path&0xFFFFF,(uint32_t)hashtype(),svid,msid);
    int fd=open(filename,O_RDONLY);
    if(fd<0){
      busy.erase(who); //FIXME, check if this is not a problem :-(
      mtx_.unlock();
      DLOG("%s open failed [len:%d]\n",filename,len);
      return(0);}
    if(data!=NULL){
      free(data);
      data=NULL;}
    struct stat sb;
    fstat(fd,&sb);
    data=(uint8_t*)malloc(sb.st_size); //do not throw
    read(fd,data,sb.st_size);
    close(fd);
    if(len==header_length){
      len=*((uint32_t*)data)>>8;
      //if(hashtype()==MSGTYPE_MSG && !*((uint64_t*)sigh)){
      if(hashtype()==MSGTYPE_MSG){
        memcpy(sigh,data+len,SHA256_DIGEST_LENGTH);}
      else{
        hash_signature();}} //FIXME, check if message is not MSG,INI,CND,BLK (DBL for example)
    status|=MSGSTAT_DAT | MSGSTAT_SAV; //load() succeeded so massage is saved
    mtx_.unlock();
    DLOG("%04X LOAD %04X:%08X (len:%d) %s\n",who,svid,msid,len,filename);
    return((uint32_t)sb.st_size);
  }

  void unload(int16_t who)
  { if((status & (MSGSTAT_DAT|MSGSTAT_SAV)) != (MSGSTAT_DAT|MSGSTAT_SAV)){
      busy_erase(who);
      return;}
    if(!path){
      busy_erase(who);
      return;}
    mtx_.lock();
    busy.erase(who);
    if(busy.empty()){
      if(len==header_length){
        DLOG("WARNING !!! trying to unload short message (%02x_%04x_%08x [len:%d])\n",
          (uint32_t)hashtype(),svid,msid,len);}
      else{
        if(data!=NULL && (status & MSGSTAT_SAV)){ //will only unload messages that are saved
          DLOG("%04X UNLOAD %04X:%08X (len:%d, path:%08X)\n",who,svid,msid,len,path);
          free(data);
          data=NULL;}}}
    mtx_.unlock();
  }

  void save_mnum(uint32_t mnum)
  { if(hashtype()!=MSGTYPE_MSG){
      return;}
    if(!(status & MSGSTAT_SAV)){
      return;}
    char filename[128];
    makefilename(filename,path,"msg");
    //sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.msg",path>>20,path&0xFFFFF,(uint32_t)hashtype(),svid,msid);
    if(!(status & MSGSTAT_VAL)){
      ELOG("ERROR, save_mnum for invalid message %04X:%08X (%s)\n",svid,msid,filename);
      assert(0);}
    int fd=open(filename,O_WRONLY);
    if(fd<0){
      ELOG("ERROR, saving mnum %d in %s\n",mnum,filename);
      return;}
    lseek(fd,len+32,SEEK_SET);
    write(fd,&mnum,4);
    close(fd);
  }

  void makefilename(char* filename,uint32_t where,const char* suffix)
  { if(status & MSGSTAT_BAD){
      char hash[2*SHA256_DIGEST_LENGTH+1]; hash[2*SHA256_DIGEST_LENGTH]='\0';
      ed25519_key2text(hash,sigh,SHA256_DIGEST_LENGTH);
      sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x_%s.%s",
        where>>20,where&0xFFFFF,(uint32_t)hashtype(),svid,msid,hash,suffix);}
    else{
      sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.%s",
        where>>20,where&0xFFFFF,(uint32_t)hashtype(),svid,msid,suffix);}
  }


  //FIXME, check again the time of saving, consider free'ing data after save
  int save()
  { if(!(status & MSGSTAT_DAT)){
      return(0);}
    if(!path){
      ELOG("ERROR, saving %04X:%08X [l:%d], no path\n",svid,msid,len);
      return(0);}
    boost::lock_guard<boost::mutex> lock(mtx_);
    char filename[128];
    makefilename(filename,path,"msg");
    //assert(data!=NULL);
    if(data==NULL){
      ELOG("ASSERT in save data==NULL: %04X:%08X %016lX %s len:%d\n",svid,msid,hash.num,filename,len);
      assert(0);}
    int fd=open(filename,O_WRONLY|O_CREAT,0644);
    if(fd<0){
      ELOG("ERROR, saving %s\n",filename);
      return(0);}
    if(hashtype()==MSGTYPE_MSG){
//FIXME, if MSGTYPE_MSG, write hashtree and index
      uint32_t total=*((uint32_t*)(&data[len+32+4])); //FIXME, do not use length from data! (maybe missing !!!) 
      assert(total);
      write(fd,data,total);}
    else{
      write(fd,data,len);}
    close(fd);
    if(!(status & MSGSTAT_BAD)){
      save_path();}
    status|=MSGSTAT_SAV;
    return(1);
  }

  int save_path()
  { if(hashtype()!=MSGTYPE_MSG){
      return(2);}
    char filename[64];
    sprintf(filename,"inx/%04X.inx",svid);
    int fd=open(filename,O_WRONLY|O_CREAT,0644);
    if(fd<0){
      return(0);}
    lseek(fd,msid*sizeof(uint32_t),SEEK_SET);
    write(fd,&path,sizeof(uint32_t));
    close(fd);
    return(1);
  }

  int erase_path()
  { if(hashtype()!=MSGTYPE_MSG){
      return(2);}
    char filename[64];
    sprintf(filename,"inx/%04X.inx",svid);
    int fd=open(filename,O_WRONLY|O_CREAT,0644);
    if(fd<0){
      return(0);}
    uint32_t zero=0;
    lseek(fd,msid*sizeof(uint32_t),SEEK_SET);
    write(fd,&zero,sizeof(uint32_t));
    close(fd);
    return(1);
  }

  uint32_t load_path()
  { if(path){
      return(path);}
    char filename[64];
    sprintf(filename,"inx/%04X.inx",svid);
    int fd=open(filename,O_RDONLY);
    if(fd<0){
      return(0);}
    lseek(fd,msid*sizeof(uint32_t),SEEK_SET);
    read(fd,&path,sizeof(uint32_t));
    close(fd);
    return(path);
  }

  void save_undo(std::map<uint32_t,user_t>& undo,uint32_t users,uint64_t* csum,int64_t& weight,int64_t& fee,uint8_t* msha,uint32_t& mtim) // assume no errors :-) FIXME
  { char filename[128];
    makefilename(filename,path,"und");
    //sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.und",path>>20,path&0xFFFFF,(uint32_t)hashtype(),svid,msid);
    int fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0644);
    if(fd<0){
      ELOG("ERROR failed to open %s, fatal\n",filename);
      exit(-1);}
    write(fd,csum,4*sizeof(uint64_t));
    write(fd,&weight,sizeof(int64_t));
    write(fd,&fee,sizeof(int64_t));
    write(fd,msha,SHA256_DIGEST_LENGTH);
    write(fd,&mtim,sizeof(int32_t));
    for(auto it=undo.begin();it!=undo.end();it++){
      write(fd,&it->first,sizeof(uint32_t));
      write(fd,&it->second,sizeof(user_t));}
    write(fd,&users,sizeof(uint32_t));
    close(fd);
  }

  uint32_t load_undo(std::map<uint32_t,user_t>& undo,uint64_t* csum,int64_t& weight,int64_t& fee,uint8_t* msha,uint32_t& mtim)
  { char filename[128];
    makefilename(filename,path,"und");
    //sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.und",path>>20,path&0xFFFFF,(uint32_t)hashtype(),svid,msid);
    int fd=open(filename,O_RDONLY);
    if(fd<0){
      ELOG("ERROR failed to open %s, fatal\n",filename);
      exit(-1);}
    read(fd,csum,4*sizeof(uint64_t));
    read(fd,&weight,sizeof(int64_t));
    read(fd,&fee,sizeof(int64_t));
    read(fd,msha,SHA256_DIGEST_LENGTH);
    read(fd,&mtim,sizeof(int32_t));
    for(;;){
      uint32_t i;
      user_t u;
      if(read(fd,&i,sizeof(uint32_t))!=sizeof(uint32_t)){
        close(fd);
        return(0);}
      if(read(fd,&u,sizeof(user_t))!=sizeof(user_t)){
        close(fd);
        return(i);}
      undo[i]=u;}
    close(fd);
    return 0;
  }

  int move(uint32_t nextpath)
  { char oldname[128];
    char newname[128];
    if(!path || path==nextpath){
      makefilename(oldname,path,"msg");
      DLOG("NO MOVE of %s from %08X to %08X\n",oldname,path,nextpath);
      return(0);}
    boost::lock_guard<boost::mutex> lock(mtx_);
    int r=0;
    if(status & MSGSTAT_SAV){
      r=-1;
      makefilename(oldname,path,"und");
      makefilename(newname,nextpath,"und");
      rename(oldname,newname); //does not exist before validation
      makefilename(oldname,path,"msg");
      makefilename(newname,nextpath,"msg");
      if((r=rename(oldname,newname))){
        ELOG("FAILED to move %s to %s, %s\n",oldname,newname,strerror(errno));
        exit(-1);}} //FIXME, do not exit later
    path=nextpath;
    save_path();
    return(r);
  }

  bool bad_recover()
  { assert((status & (MSGSTAT_SAV|MSGSTAT_BAD))==(MSGSTAT_SAV|MSGSTAT_BAD));
    char moldname[128];
    char mnewname[128];
    char uoldname[128];
    char unewname[128];
    makefilename(moldname,path,"msg");
    makefilename(uoldname,path,"und");
    status &= ~MSGSTAT_BAD;
    makefilename(mnewname,path,"msg");
    makefilename(unewname,path,"und");
    unlink(unewname);
    if(rename(uoldname,unewname)){ //does not exist before validation
      DLOG("RECOVER %s to %s failed\n",uoldname,unewname);}
    else{
      DLOG("RECOVER %s to %s succeeded\n",uoldname,unewname);}
    unlink(mnewname);
    if(rename(moldname,mnewname)){ //does not exist before validation
      DLOG("RECOVER %s to %s failed\n",moldname,mnewname);
      return(false);}
    DLOG("RECOVER %s to %s succeeded\n",moldname,mnewname);
    return(true);
  }

  bool bad_insert()
  { assert((status & (MSGSTAT_SAV|MSGSTAT_BAD))==(MSGSTAT_SAV));
    char moldname[128];
    char mnewname[128];
    char uoldname[128];
    char unewname[128];
    makefilename(moldname,path,"msg");
    makefilename(uoldname,path,"und");
    status |= MSGSTAT_BAD;
    makefilename(mnewname,path,"msg");
    makefilename(unewname,path,"und");
    unlink(unewname);
    if(rename(uoldname,unewname)){ //does not exist before validation
      DLOG("BAD insert %s to %s failed\n",uoldname,unewname);}
    else{
      DLOG("BAD insert %s to %s succeeded\n",uoldname,unewname);}
    unlink(mnewname);
    if(rename(moldname,mnewname)){ //does not exist before validation
      DLOG("BAD insert %s to %s failed\n",moldname,mnewname);
      return(false);}
    DLOG("BAD insert %s to %s succeeded\n",moldname,mnewname);
    return(true);
  }

  void remove_undo() //TODO, consider locking
  { char filename[128];
    if(!path || !(status & MSGSTAT_SAV)){
      return;}
    makefilename(filename,path,"und");
    //sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.und",path>>20,path&0xFFFFF,(uint32_t)hashtype(),svid,msid);
    unlink(filename);
    return;
  }

  void remove() //TODO, consider locking
  { char filename[128];
    if(!path || !(status & MSGSTAT_SAV)){
      return;}
    remove_undo();
    makefilename(filename,path,"msg");
    //sprintf(filename,"blk/%03X/%05X/%02x_%04x_%08x.msg",path>>20,path&0xFFFFF,(uint32_t)hashtype(),svid,msid);
    unlink(filename);
    return;
  }

  void update(boost::shared_ptr<message>& msg) // should be called swap or copy
  { uint32_t l=msg->len;
    uint8_t *d=msg->data;
    mtx_.lock();
    msg->len=len;
    msg->data=data;
    len=l;
    data=d;
    know_insert_(msg->peer);
    memcpy(sigh,msg->sigh,SHA256_DIGEST_LENGTH);
    peer=msg->peer;
    now=msg->now;
    got=msg->got;
    if(len>header_length){ //TODO, is this needed ?
      status|=MSGSTAT_DAT;}
    mtx_.unlock();
  }

  void busy_insert(uint16_t p)
  { mtx_.lock();
    got=time(NULL);
    busy.insert(p);
    mtx_.unlock();
  }

  void busy_erase(uint16_t p)
  { mtx_.lock();
    busy.erase(p);
    mtx_.unlock();
  }

  bool not_busy(uint16_t p)
  { mtx_.lock();
    if(busy.find(p)==busy.end()){
      mtx_.unlock();
      return(true);}
    mtx_.unlock();
    return(false);
  }

  bool know_find_(uint16_t p)
  { for(auto k=know.begin();k!=know.end();k++){
      if(*k==p){
        return(true);}}
    return(false);
  }

  void know_erase_(uint16_t p)
  { for(auto k=know.begin();k!=know.end();k++){
      if(*k==p){
        *k=0;}} // will cause limited memory leak 
  }

  void know_insert_(uint16_t p)
  { auto n=know.end();
    for(auto k=know.begin();k!=know.end();k++){
      if(*k==p){
        return;}
      if(*k==0){
        n=k;}}
    if(n!=know.end()){
      *n=p;}
    else{
      know.push_back(p);}
  }

  void know_insert(uint16_t p)
  { mtx_.lock();
    know_insert_(p);
    mtx_.unlock();
  }

  bool know_emplace(uint16_t p)
  { mtx_.lock();
    if(know_find_(p)){
      mtx_.unlock();
      return(false);}
    know_insert_(p);
    mtx_.unlock();
    return(true);
  }

  bool sent_emplace(uint16_t p)
  { mtx_.lock();
    if(sent.find(p)!=sent.end()){
      mtx_.unlock();
      return(false);}
    know_insert_(p);
    sent.insert(p);
    busy.insert(p);
    mtx_.unlock();
    return(true);
  }

  void sent_insert(uint16_t p)
  { mtx_.lock();
    sent.insert(p);
    mtx_.unlock();
  }

  void sent_erase(uint16_t p)
  { mtx_.lock();
    //if(busy.find(p)==busy.end()) //why was this here ??? ... maybe to prevent assert failure ...
    sent.erase(p);
    mtx_.unlock();
  }

  void know_sent_erase(uint16_t p)
  { mtx_.lock();
    know_erase_(p);
    sent.erase(p);
    mtx_.unlock();
  }

  bool cansend(uint16_t p, uint32_t mynow, uint32_t maxwait)
  { //assert(data!=NULL); // failes !!!
    if((status & MSGSTAT_DAT) || len!=header_length){
      DLOG("IGNORING REQUEST for %04X:%08X\n",svid,msid);
      return(false);}
    mtx_.lock();
    if(mynow<=got+MAX_MSGWAIT+maxwait){
      mtx_.unlock();
      return(false);}
    if(sent.find(p)==sent.end()){
      got=mynow;
      mtx_.unlock();
      DLOG("REQUEST for %04X:%08X from %04X\n",svid,msid,p);
      return(true);}
    mtx_.unlock();
    return(false);
  }

  uint16_t request() //find a peer from which we will request the message
  { //assert(data!=NULL); // failes !!!
    if((status & MSGSTAT_DAT) || len!=header_length){
      DLOG("IGNORING REQUEST for %04X:%08X\n",svid,msid);
      return(0);}
    uint32_t mynow=time(NULL);
    mtx_.lock();
    //if(mynow<=got+MAX_MSGWAIT+(data[0]==MSGTYPE_USR?MAX_USRWAIT:0))
    if(mynow<=got+MAX_MSGWAIT){
      mtx_.unlock();
      return(0);}
    std::vector<uint16_t> peers;
    for(auto k=know.begin();k!=know.end();k++){
      auto s=sent.find(*k);
      if(s==sent.end()){
        peers.push_back(*k);}}
    mtx_.unlock();
    if(peers.empty()){
      return(0);}
    uint16_t peer=peers[((uint64_t)random())%peers.size()];
    got=mynow;
    //sent.insert(*k); //will be also inserted by peer::handle_write after message submitted + deliver(,)
    //FIXME, will not be inserted if peer has dicsonnected
    DLOG("REQUEST for %04X:%08X from %04X\n",svid,msid,peer);
    return(peer);
  }

  uint32_t data_len()
  { uint32_t dlen=len;
    mtx_.lock();
    if(len>64){
      dlen=0;
      memcpy(&dlen,data+1,3);}
    mtx_.unlock();
    return(dlen);
  }

private:
};

typedef boost::shared_ptr<message> message_ptr;
typedef std::deque<message_ptr> message_queue;
typedef std::map<uint64_t,message_ptr> message_map;

#endif // MESSAGE_HPP
