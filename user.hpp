#ifndef USER_HPP
#define USER_HPP

//bank only
#define TXSTYPE_STP 0	/* die/stop processing bank */	/* restart requires a new BKY transaction */
#define TXSTYPE_CON 1	/* connected */			/* inform peers about location */
#define TXSTYPE_UOK 2	/* accept remote account request */
			/* consider leaving space for more transactions */
//bank & user
#define TXSTYPE_BRO 3	/* broadcast */			/* send ads to network */
#define TXSTYPE_PUT 4	/* send funds */	
#define TXSTYPE_MPT 5	/* send funds to many accounts */	
#define TXSTYPE_USR 6	/* create new user */		
#define TXSTYPE_BNK 7	/* create new bank */
#define TXSTYPE_GET 8	/* get funds */			/* retreive funds from remote/dead bank */
#define TXSTYPE_KEY 9	/* change account key */
#define TXSTYPE_BKY 10	/* change bank key */
			/* consider leaving space for more transactions */
//user only
#define TXSTYPE_INF 11	/* strating from this message only messages between wallet and client.hpp */
#define TXSTYPE_LOG 12	/* return user status and log */
#define TXSTYPE_BLG 13	/* return broadcast log */
			/* request save status in log */
//overwriting user only TX in log
#define TXSTYPE_DIV 11	/* user dividend payment tx code */
#define TXSTYPE_FEE 12	/* bank fee payment tx code */
//end
#define TXSTYPE_MAX 14

const char* txsname[TXSTYPE_MAX]={
	"stop",			//0
	"dividend",		//1
	"account_created",	//2
	"broadcast",		//3
	"send_one",		//4
	"send_many",		//5
	"create_account",	//6
	"create_node",		//7
	"retrieve_funds",	//8
	"change_account_key",	//9
	"change_node_key",	//10
	"get_account",		//11 also 'get_me'
	"get_log",		//12
	"get_broadcast"};	//13

const char* logname[TXSTYPE_MAX]={
	"node_started",		//0
	"unknown",		//1
	"account_created",	//2
	"broadcast",		//3
	"send_one",		//4
	"send_many",		//5
	"create_account",	//6
	"create_node",		//7
	"retrieve_funds",	//8
	"change_account_key",	//9
	"change_node_key",	//10
	"dividend",		//11
	"bank_profit",		//12
	"unknown"};		//13

const int txslen[TXSTYPE_MAX+1]={ //length does not include variable part and input hash
	0,			//0:STP not defined yet
	1+2+4,			//1:CON (port,ipv4)
	1+2+4+4+4+2+4+32,	//2:UOK new account, not signed
	1+2+4+4+4+2,		//3:BRO 'bbank' is message length
	1+2+4+4+4+2+4+8+32,	//4:PUT, extra parameter = official message (32 byte)
	1+2+4+4+4+2,		//5:MPT 'bbank' is number of to_accounts
	1+2+4+4+4+2,		//6:USR
	1+2+4+4+4,		//7:BNK
	1+2+4+4+4+2+4,		//8:GET,
	1+2+4+4+4+32,		//9:KEY
	1+2+4+4+4+32,		//10:BKY, old key appended to undo message
	1+2+4+2+4+4,		//11:INF
	1+2+4+2+4+4,		//12:LOG
	1+2+4+  4,		//13:BLG
	1+2+4+4+4+2+4+8+32};	//14:MAX fixed buffer size
	
#define USER_CLOSED 0x0001;

#pragma pack(1)
typedef struct user_s { // 4+4+32+32+2+2+4+4+4+8+32=96+32=128 bytes
	uint32_t msid; // id of last transaction, id==1 is the creation.
	uint32_t time; // original time of transaction (used as lock initiation)
	uint8_t pkey[SHA256_DIGEST_LENGTH]; //public key
	uint8_t hash[SHA256_DIGEST_LENGTH]; //users block hash
	uint16_t stat; // includes status and account type
	uint16_t node; // target node, ==bank_id at creation
	uint32_t user; // target user, ==user_id at creation
	uint32_t lpath; // block time of local transaction, used for dividends and locks
	uint32_t rpath; // block time of incomming transaction, used for dividends
	 int64_t weight; // balance, MUST BE AFTER rpath !
	uint64_t csum[4]; //sha256 hash of user account, MUST BE AFTER rpath !
} user_t;
typedef struct log_s {
	uint32_t time;
	uint16_t type;
	uint16_t node;
	uint32_t nmid; // peer msid, could overwrite this also with info
	uint32_t mpos; // position in file or remote user in 'get', could overwrite this with info
	uint32_t user;
	uint32_t umid; // user msid
	 int64_t weight; // value, or info (use type to determine)
	uint8_t info[32];
} log_t;
#pragma pack()

class usertxs :
  public boost::enable_shared_from_this<usertxs>
{
public:
	uint8_t  ttype;
	uint16_t abank;
	uint32_t auser;
	uint32_t amsid;
	uint32_t ttime;
	uint16_t bbank; // also broadcast message len OR number of to_addresses in MPT transaction
	uint32_t buser;
	 int64_t tmass;
	uint8_t  tinfo[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t* data;
	int size;	// differs on the network and in office :-( !!!
			// get_size returns network size, fix names !!!

	~usertxs()
	{	free(data);
	}

	usertxs() :
		ttype(TXSTYPE_MAX),
		abank(0),
		auser(0),
		amsid(0),
		ttime(0),
		bbank(0),
		buser(0),
		tmass(0),
		data(NULL)
	{
	}

	usertxs(uint8_t* din,int len) :
		ttype(*data),
		bbank(0),
		buser(0),
		size(len)
	{	//assert(malloc_usable_size(din)>=len && malloc_usable_size(din)<len+0x100); //test for malloc
		data=(uint8_t*)std::malloc(size);
		memcpy(data,din,size);
		memcpy(&abank,data+1,2);
		memcpy(&auser,data+3,4);
	}

	usertxs(uint8_t nttype,uint16_t nabank,uint32_t nauser,uint16_t nbbank,uint32_t nbuser,uint32_t nttime) :
		ttype(nttype),
		abank(nabank),
		auser(nauser),
		amsid(0),
		ttime(nttime),
		bbank(nbbank),
		buser(nbuser)
	{	assert(ttype==TXSTYPE_INF);
		size=txslen[TXSTYPE_INF]+64;
		data=(uint8_t*)std::malloc(size);
		data[0]=TXSTYPE_INF;
		memcpy(data+1,&abank,2);
		memcpy(data+3,&auser,4);
		memcpy(data+7,&bbank,2);
		memcpy(data+9,&buser,4);
		memcpy(data+13,&ttime,4);
	}

	usertxs(uint8_t nttype,uint16_t nabank,uint32_t nauser,uint32_t nttime) :
		ttype(nttype),
		abank(nabank),
		auser(nauser),
		amsid(0),
		ttime(nttime),
		data(NULL)
	{	if(ttype==TXSTYPE_CON){
	 		size=txslen[TXSTYPE_CON];
			data=(uint8_t*)std::malloc(size);
			data[0]=TXSTYPE_CON;
			memcpy(data+1  ,&abank,2);
			memcpy(data+1+2,&auser,4);
			//memcpy(data+1+6,&ttime,4);
			return;}
	 	if(ttype==TXSTYPE_BLG){
	 		size=txslen[TXSTYPE_BLG]+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=TXSTYPE_BLG;
			memcpy(data+1  ,&abank,2);
			memcpy(data+1+2,&auser,4);
			memcpy(data+1+6,&ttime,4);
			return;}
	 	if(ttype==TXSTYPE_LOG){
			size=txslen[TXSTYPE_LOG]+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=TXSTYPE_LOG;
			memcpy(data+1,&abank,2);
			memcpy(data+3,&auser,4);
			memcpy(data+7,&abank,2);
			memcpy(data+9,&auser,4);
			memcpy(data+13,&ttime,4);
			return;}
		fprintf(stderr,"BAD MSG: %X\n",ttype);
	}

	//usertxs(uint8_t nttype,uint16_t nabank,uint32_t nauser,uint32_t namsid,uint32_t nttime,uint16_t nbbank,uint32_t nbuser,int64_t ntmass,uint64_t ntinfo,const char* text,const char* okey) :
	usertxs(uint8_t nttype,uint16_t nabank,uint32_t nauser,uint32_t namsid,uint32_t nttime,uint16_t nbbank,uint32_t nbuser,int64_t ntmass,uint8_t* ntinfo,const char* text) :
		ttype(nttype),
		abank(nabank),
		auser(nauser),
		amsid(namsid),
		ttime(nttime),
		bbank(nbbank),
		buser(nbuser),
		tmass(ntmass),
		//tinfo(ntinfo),
		data(NULL)
	{	if(ntinfo!=NULL){
			memcpy(tinfo,ntinfo,32);}
		if(ttype>=TXSTYPE_MAX){
			fprintf(stderr,"ERROR, creating message\n");
			size=0;
			return;}
		int len=txslen[ttype];
		if(ttype==TXSTYPE_CON){
			size=len;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1  ,&abank,2);
			memcpy(data+1+2,&auser,4);
			//memcpy(data+1+6,&ttime,4);
			return;}
		else if(ttype==TXSTYPE_UOK){
			size=len;}
		else if(ttype==TXSTYPE_BRO){
			size=len+bbank+64;}
		else if(ttype==TXSTYPE_MPT){
			size=len+bbank*(6+8)+64;}
	 	else if(ttype==TXSTYPE_KEY){ //user,newkey
			size=len+64+64;} // size on the peer network is reduced back to len+64 !!!
	 	//else if(ttype==TXSTYPE_BKY){ //user0,newkey,oldkey
		//	size=len+64+64+64;}
		else{
			size=len+64;}
		data=(uint8_t*)std::malloc(size);
		data[0]=ttype;
		memcpy(data+1   ,&abank,2);
		memcpy(data+1+2 ,&auser,4);
		memcpy(data+1+6 ,&amsid,4);
		memcpy(data+1+10,&ttime,4);
		if(len==1+2+4+4+4){
			return;}
		if(ttype==TXSTYPE_KEY || ttype==TXSTYPE_BKY){ // for BKY, office adds old key
			memcpy(data+1+14,text,32);
			return;}
		//if(ttype==TXSTYPE_BKY){
		//	memcpy(data+1+14,text,32);
		//	memcpy(data+1+14+32,okey,32);
		//	return;}
		memcpy(data+1+14,&bbank,2);
		if(ttype==TXSTYPE_BRO){
			memcpy(data+1+16,text,bbank);
			return;}
		if(ttype==TXSTYPE_MPT){
			memcpy(data+1+16,text,bbank*(6+8));
			return;}
		if(len>=1+2+4+4+4+2+4){
			memcpy(data+1+16,&buser,4);}
		if(ttype==TXSTYPE_UOK){
			memcpy(data+1+20,text,32);
			return;}
		if(len>=1+2+4+4+4+2+4+8+32){
			memcpy(data+1+20,&tmass,8);
			memcpy(data+1+28,tinfo,32);}
	}

	uint32_t get_size(char* txs) // returns network size
	{	if(*txs>=TXSTYPE_MAX){
                        fprintf(stderr,"ERROR, parsing message type\n");
                        return(0xFFFFFFFF);}
		if(*txs==TXSTYPE_CON){
			return(txslen[TXSTYPE_CON]);} // no signature
		if(*txs==TXSTYPE_UOK){
			return(txslen[TXSTYPE_UOK]);} // no signature
	 	if(*txs==TXSTYPE_BKY){
			return(txslen[TXSTYPE_BKY]+64+32);} // additional 4 bytes on network !!!
	 	if(*txs==TXSTYPE_USR){
			return(txslen[TXSTYPE_USR]+64+4+32);} // additional 4+32 bytes on network !!!
		if(*txs==TXSTYPE_BRO){
			uint16_t len;
			memcpy(&len,txs+1+14,2);
			return(txslen[TXSTYPE_BRO]+64+len);}
		if(*txs==TXSTYPE_MPT){
			uint16_t len;
			memcpy(&len,txs+1+14,2);
			return(txslen[TXSTYPE_MPT]+64+len*(6+8));}
	 	//if(*txs==TXSTYPE_KEY || *txs==TXSTYPE_BKY){ //user,newkey
		//	return(txslen[(int)*txs]+64+64);} // no need to send the second signature to the network
		return(txslen[(int)*txs]+64);
	}

	bool parse(char* txs) //TODO, should return parsed buffer length
	{	ttype=*txs;
		if(ttype>=TXSTYPE_MAX){
			fprintf(stderr,"ERROR, parsing message\n");
			return(false);}
		size=txslen[ttype]+64;
		memcpy(&abank,txs+1+0 ,2);
		memcpy(&auser,txs+1+2 ,4);
		if(ttype==TXSTYPE_CON){
			//memcpy(&ttime,txs+1+6,4);
			size=txslen[TXSTYPE_CON]; // no signature !
			return(true);}
		if(ttype==TXSTYPE_BLG){
			memcpy(&ttime,txs+1+6,4);
			return(true);}
		if(ttype==TXSTYPE_INF || ttype==TXSTYPE_LOG){
			memcpy(&bbank,txs+1+6,2);
			memcpy(&buser,txs+1+8,4);
			memcpy(&ttime,txs+1+12,4);
			return(true);}
		memcpy(&amsid,txs+1+6 ,4);
		memcpy(&ttime,txs+1+10,4);
		memcpy(&bbank,txs+1+14,2);
                if(ttype==TXSTYPE_GET){
			memcpy(&buser,txs+1+16,4);
			return(true);}
                if(ttype==TXSTYPE_UOK){
			memcpy(&buser,txs+1+16,4);
			size=txslen[TXSTYPE_UOK]; // no signature !
			return(true);}
                if(ttype==TXSTYPE_PUT){
			memcpy(&buser,txs+1+16,4);
			memcpy(&tmass,txs+1+20,8);
			memcpy(tinfo,txs+1+28,32);}
		//else{
		//	bzero(tinfo,32);}
		if(ttype==TXSTYPE_BKY){
			size+=32;}
		else if(ttype==TXSTYPE_USR){
			size+=4+32;}
		else if(ttype==TXSTYPE_BRO){
			size+=bbank;}
		else if(ttype==TXSTYPE_MPT){
			size+=bbank*(6+8);}
		//else if(ttype==TXSTYPE_BKY || ttype==TXSTYPE_KEY){ // not needed on the network !!!
		//	size+=64;}
		return(true);
	}

	uint8_t* get_sig(char* buf)
	{	if(*buf==TXSTYPE_BRO){
			return((uint8_t*)buf+txslen[TXSTYPE_BRO]+bbank);}
	 	if(*buf==TXSTYPE_MPT){
			return((uint8_t*)buf+txslen[TXSTYPE_MPT]+bbank*(6+8));}
		return((uint8_t*)buf+txslen[(int)*buf]);
		//return((uint8_t*)buf+txslen[TXSTYPE_BRO]);
	}

	void sign(uint8_t* hash,uint8_t* sk,uint8_t* pk)
	{	if(ttype==TXSTYPE_CON){
			return;}
	 	if(ttype==TXSTYPE_UOK){
			return;}
		if(ttype==TXSTYPE_INF){
			ed25519_sign(data,txslen[TXSTYPE_INF],sk,pk,data+txslen[TXSTYPE_INF]);
			return;}
		if(ttype==TXSTYPE_LOG){
			ed25519_sign(data,txslen[TXSTYPE_LOG],sk,pk,data+txslen[TXSTYPE_LOG]);
			return;}
		if(ttype==TXSTYPE_BLG){
			ed25519_sign(data,txslen[TXSTYPE_BLG],sk,pk,data+txslen[TXSTYPE_BLG]);
			return;}
		if(ttype==TXSTYPE_BRO){
			ed25519_sign2(hash,32,data,txslen[TXSTYPE_BRO]+bbank,sk,pk,data+txslen[TXSTYPE_BRO]+bbank);
			return;}
		if(ttype==TXSTYPE_MPT){
			ed25519_sign2(hash,32,data,txslen[TXSTYPE_MPT]+bbank*(6+8),sk,pk,data+txslen[TXSTYPE_MPT]+bbank*(6+8));
			return;}
		ed25519_sign2(hash,32,data,txslen[ttype],sk,pk,data+txslen[ttype]);
	}

	//void sign2(uint8_t* hash,uint8_t* sk) // additional signature, no need to supply pk (is in data)
	//{	assert(ttype==TXSTYPE_KEY); // || ttype==TXSTYPE_BKY
	//	ed25519_sign2(hash,32,data,txslen[ttype],sk,data+1+2+4+4+4,data+txslen[ttype]+64);
	//} // FIXME, sign A FIXED STRING with the second signature !!!

	int sign2(uint8_t* sig) // additional signature to validate public key
	{	assert(ttype==TXSTYPE_KEY);
		memcpy(data+txslen[ttype]+64,sig,64);
		return(wrong_sig2(data));
	}

	int wrong_sig(uint8_t* buf,uint8_t* hash,uint8_t* pk)
	{	if(ttype==TXSTYPE_CON){
			return(0);}
	 	if(ttype==TXSTYPE_UOK){
			return(0);}
		if(ttype==TXSTYPE_INF){
			return(ed25519_sign_open(buf,txslen[TXSTYPE_INF],pk,buf+txslen[TXSTYPE_INF]));}
		if(ttype==TXSTYPE_LOG){
			return(ed25519_sign_open(buf,txslen[TXSTYPE_LOG],pk,buf+txslen[TXSTYPE_LOG]));}
		if(ttype==TXSTYPE_BLG){
			return(ed25519_sign_open(buf,txslen[TXSTYPE_BLG],pk,buf+txslen[TXSTYPE_BLG]));}
		if(ttype==TXSTYPE_BRO){
			return(ed25519_sign_open2(hash,32,buf,txslen[TXSTYPE_BRO]+bbank,pk,buf+txslen[TXSTYPE_BRO]+bbank));}
		if(ttype==TXSTYPE_MPT){
			return(ed25519_sign_open2(hash,32,buf,txslen[TXSTYPE_MPT]+bbank*(6+8),pk,buf+txslen[TXSTYPE_MPT]+bbank*(6+8)));}
		return(ed25519_sign_open2(hash,32,buf,txslen[ttype],pk,buf+txslen[ttype]));
	}

	//int wrong_sig2(uint8_t* buf,uint8_t* hash) // additional signature with client
	//{	assert(ttype==TXSTYPE_KEY); // || ttype==TXSTYPE_BKY
	//	return(ed25519_sign_open2(hash,32,buf,txslen[ttype],buf+1+2+4+4+4,buf+txslen[ttype]+64));
	//} // FIXME, sign only the new key with the second signature !!!

	int wrong_sig2(uint8_t* buf) // additional signature to validate public key
	{	assert(ttype==TXSTYPE_KEY); // || ttype==TXSTYPE_BKY
		return(ed25519_sign_open((uint8_t*)NULL,0,buf+1+2+4+4+4,buf+txslen[ttype]+64));
	}

	void print_head()
	{	fprintf(stderr,"MSG: %1X %04X:%08X m:%08X t:%08X b:%04X u:%08X v:%016lX (l:%d)\n",
			ttype,abank,auser,amsid,ttime,bbank,buser,tmass,size);
	}

	void print()
	{	char msgtxt[0x200];
	 	if(ttype==TXSTYPE_KEY){
			assert((txslen[ttype]+64+64)*2<0x200);
			ed25519_key2text(msgtxt,data,txslen[ttype]+64+64); // do not send last hash
			fprintf(stderr,"%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
		else{
			assert((txslen[ttype]+64)*2<0x200);
			ed25519_key2text(msgtxt,data,txslen[ttype]+64); // do not send last hash
			fprintf(stderr,"%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
	}

	void change_time(uint32_t nttime)
	{	assert(ttype==TXSTYPE_BLG);
		ttime=nttime;
		memcpy(data+1+6,&ttime,4);
		return;
	}


	char* key(char* buf) //return new key in message
	{	return(buf+1+2+4+4+4);
	}

	char* upkey(char* buf) //return new key in UOK message
	{	return(buf+1+2+4+4+4+2+4);
	}

	void print_broadcast(char* buf)
	{	fprintf(stderr,"BRO:%.*s\n",bbank,broadcast(buf));
	}

	char* broadcast(char* buf) //return broadcast buffer in message
	{	return(buf+1+2+4+4+4+2);
	}

	char* broadcast_sigh(char* buf,uint16_t bbank) //not used
	{	return(buf+1+2+4+4+4+2+bbank);
	}

	void print_toaddresses(char* buf,uint16_t bbank)
	{	char* tbuf=toaddresses(buf);
		for(int i=0;i<bbank;i++,tbuf+=6+8){
			uint16_t tbank;
			uint32_t tuser;
			 int64_t tmass;
			memcpy(&tbank,tbuf+0,2);
			memcpy(&tuser,tbuf+2,4);
			memcpy(&tmass,tbuf+6,8);
			fprintf(stderr,"MPT to %04X %08X %016lX\n",tbank,tuser,tmass);}
	}

	char* toaddresses(char* buf) //return to-addresses buffer in message
	{	return(buf+1+2+4+4+4+2);
	}

	uint32_t nuser(char* buf) //return second user in message
	{	return(*((uint32_t*)(buf+1+2+4+4+4+2+64)));
	}

	char* npkey(char* buf) //return second user key in message
	{	return(buf+1+2+4+4+4+2+64+4);
	}

	char* opkey(char* buf) //return old key in message
	{	return(buf+1+2+4+4+4+32+64);
	}

private:
};
typedef boost::shared_ptr<usertxs> usertxs_ptr;

/*uint16_t crc16(const uint8_t* data_p, uint8_t length)
{ uint8_t x;
  uint16_t crc = 0x1D0F; //differet initial checksum !!!

  while(length--){
    x = crc >> 8 ^ *data_p++;
    x ^= x>>4;
    crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);}
  return crc;
}
uint16_t crc_acnt(uint16_t to_bank,uint32_t to_user)
{ uint8_t data[6];
  uint8_t* bankp=(uint8_t*)&to_bank;
  uint8_t* userp=(uint8_t*)&to_user;
  //change endian
  data[0]=bankp[1];
  data[1]=bankp[0];
  data[2]=userp[3];
  data[3]=userp[2];
  data[4]=userp[1];
  data[5]=userp[0];
  return(crc16(data,6));
}

//bool parse_acnt(uint16_t& to_bank,uint32_t& to_user,boost::optional<std::string>& json_acnt)
//{ std::string str_acnt=json_acnt.get();
bool parse_acnt(uint16_t& to_bank,uint32_t& to_user,std::string str_acnt)
{ uint16_t to_csum=0;
  uint16_t to_crc16;
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
}*/

#endif // USER_HPP
