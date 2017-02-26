#ifndef USER_HPP
#define USER_HPP

#define TXSTYPE_STP 0	/* die/stop processing bank */	/* restart requires a new BKY transaction */
#define TXSTYPE_CON 1	/* connected */			/* inform peers about location */
#define TXSTYPE_BRO 2	/* broadcast */			/* send ads to network */
#define TXSTYPE_PUT 3	/* send funds */	
#define TXSTYPE_MPT 4	/* send funds to many accounts */	
#define TXSTYPE_USR 5	/* create new user */		
#define TXSTYPE_BNK 6	/* create new bank */
#define TXSTYPE_GET 7	/* get funds */			/* retreive funds from remote/dead bank */
#define TXSTYPE_KEY 8	/* change account key */
#define TXSTYPE_BKY 9	/* change bank key */
#define TXSTYPE_INF 10	/* strating from this message only messages between wallet and client.hpp */
#define TXSTYPE_LOG 11	/* return user status and log */
#define TXSTYPE_BLG 12	/* return broadcast log */
#define TXSTYPE_MAX 13

const int txslen[TXSTYPE_MAX+1]={ //length does not include variable part and input hash
	0,			//0:STP not defined yet
	1+2+4+  4,		//1:CON
	1+2+4+4+4+2,		//2:BRO 'bbank' is message length
	1+2+4+4+4+2+4+8+8,	//3:PUT, extra parameter = official message (8 byte)
	1+2+4+4+4+2,		//4:MPT 'bbank' is number of to_accounts
	1+2+4+4+4+2,		//5:USR
	1+2+4+4+4,		//6:BNK
	1+2+4+4+4+2+4,		//7:GET,
	1+2+4+4+4+32,		//8:KEY
	1+2+4+4+4+32,		//9:BKY, old key appended to undo message
	1+2+4+2+4+4,		//10:INF
	1+2+4+2+4+4,		//11:LOG
	1+2+4+  4,		//12:BLG
	1+2+4+4+4+32};		//13:MAX fixed buffer size
	
#define USER_CLOSED 0x0001;

#pragma pack(1)
typedef struct user_s { // 8+32+32+4+4+8+4+2+2=96 bytes
	uint32_t msid; // id of last transaction, id==1 is the creation.
	uint32_t time; // original time of transaction (used as lock initiaiton)
	uint8_t pkey[SHA256_DIGEST_LENGTH]; //public key
	uint8_t hash[SHA256_DIGEST_LENGTH]; //users block hash
	uint16_t stat; // includes status and account type
	uint16_t node; // target node, ==bank_id at creation
	uint32_t user; // target user, ==user_id at creation
	uint32_t lpath; // block time of local transaction
	uint32_t rpath; // block time of incomming transaction
	 int64_t weight; // balance
} user_t;
typedef struct log_s {
	uint32_t time;
	uint16_t type;
	uint16_t node;
	uint32_t user;
	uint32_t umid; // user msid
	uint32_t nmid; // peer msid, could overwrite this also with info
	uint32_t mpos; // position in file or remote user in 'get', could overwrite this with info
	 int64_t weight; // value
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
	uint64_t tinfo;
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
		bbank(0),
		buser(0),
		data(NULL)
	{
	}

	usertxs(uint8_t nttype,uint16_t nabank,uint32_t nauser,uint16_t nbbank,uint32_t nbuser,uint32_t nttime) :
		ttype(nttype),
		abank(nabank),
		auser(nauser),
		amsid(0),
		ttime(nttime),
		bbank(nbbank),
		buser(nbuser),
		data(NULL)
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
			memcpy(data+1+6,&ttime,4);
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
	usertxs(uint8_t nttype,uint16_t nabank,uint32_t nauser,uint32_t namsid,uint32_t nttime,uint16_t nbbank,uint32_t nbuser,int64_t ntmass,uint64_t ntinfo,const char* text) :
		ttype(nttype),
		abank(nabank),
		auser(nauser),
		amsid(namsid),
		ttime(nttime),
		bbank(nbbank),
		buser(nbuser),
		tmass(ntmass),
		tinfo(ntinfo),
		data(NULL)
	{	if(ttype>=TXSTYPE_MAX){
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
			memcpy(data+1+6,&ttime,4);
			return;}
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
		if(len>=1+2+4+4+4+2+4+8+8){
			memcpy(data+1+20,&tmass,8);
			memcpy(data+1+28,&tinfo,8);}
	}

	uint32_t get_size(char* txs) // returns network size
	{	if(*txs==TXSTYPE_CON){
			return(txslen[TXSTYPE_CON]);} // no signature
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
			memcpy(&ttime,txs+1+6,4);
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
		memcpy(&buser,txs+1+16,4);
		memcpy(&tmass,txs+1+20,8);
		memcpy(&tinfo,txs+1+28,8);
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
			return((uint8_t*)buf+txslen[TXSTYPE_BRO]+bbank*(6+8));}
		return((uint8_t*)buf+txslen[TXSTYPE_BRO]);
	}

	void sign(uint8_t* hash,uint8_t* sk,uint8_t* pk)
	{	if(ttype==TXSTYPE_CON){
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
	{	fprintf(stdout,"MSG: %1X %04X:%08X m:%08X t:%08X b:%04X u:%08X v:%016lX (l:%d)\n",
			ttype,abank,auser,amsid,ttime,bbank,buser,tmass,size);
	}

	void print()
	{	char msgtxt[0x200];
	 	if(ttype==TXSTYPE_KEY){
			assert((txslen[ttype]+64+64)*2<0x200);
			ed25519_key2text(msgtxt,data,txslen[ttype]+64+64); // do not send last hash
			fprintf(stdout,"%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
		else{
			assert((txslen[ttype]+64)*2<0x200);
			ed25519_key2text(msgtxt,data,txslen[ttype]+64); // do not send last hash
			fprintf(stdout,"%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
	}

	char* key(char* buf) //return new key in message
	{	return(buf+1+2+4+4+4);
	}

	void print_broadcast(char* buf)
	{	fprintf(stdout,"BRO:%.*s\n",bbank,broadcast(buf));
	}

	char* broadcast(char* buf) //return broadcast buffer in message
	{	return(buf+1+2+4+4+4+2);
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
#endif // USER_HPP
