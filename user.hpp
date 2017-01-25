#ifndef USER_HPP
#define USER_HPP

#define TXSTYPE_STP 0	/* die/stop processing bank */	/* restart requires a new BKY transaction */
#define TXSTYPE_CON 1	/* connected */			/* inform peers about location */
#define TXSTYPE_BRO 2	/* broadcast */			/* send ads to network */
#define TXSTYPE_PUT 3	/* send funds */	
#define TXSTYPE_USR 4	/* create new user */		
#define TXSTYPE_BNK 5	/* create new bank */
#define TXSTYPE_GET 6	/* get funds */			/* retreive funds from remote/dead bank */
#define TXSTYPE_KEY 7	/* change account key */
#define TXSTYPE_BKY 8	/* change bank key */
#define TXSTYPE_INF 9	/* strating from this message only messages between wallet and client.hpp */
#define TXSTYPE_MAX 10

const int txslen[TXSTYPE_MAX+1]={ //length does not include variable part and input hash
	1+2+4+2+4+4,		//INF
	0,			//CON not defined yet
	1+2+4+4+4+2,		//BRO 'bbank' is message length
	1+2+4+4+4+2+4+8+8,	//PUT, extra parameter = official message (8 byte)
	1+2+4+4+4+2,		//USR
	1+2+4+4+4,		//BNK
	1+2+4+4+4+2+4,		//GET,
	1+2+4+4+4+32,		//KEY
	1+2+4+4+4+32+32,	//BKY
	0, 			//CON not defined yet
	1+2+4+4+4+32+32};	//MAX fixed buffer size
	
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
#pragma pack()

class usertxs : public boost::enable_shared_from_this<usertxs>
{
public:
	uint8_t  ttype;
	uint16_t abank;
	uint32_t auser;
	uint32_t amsid;
	uint32_t ttime;
	uint16_t bbank; // also broadcast message len
	uint32_t buser;
	 int64_t tmass;
	uint64_t tinfo;
	char* data;
	int size;

	usertxs() :
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
		bbank(nbbank),
		buser(nbuser),
		ttime(nttime),
		data(NULL)
	{	assert(ttype==TXSTYPE_INF);
		size=32+txslen[TXSTYPE_INF]+64;
		data=(char*)std::malloc(size);
		data[32]=TXSTYPE_INF;
		memcpy(data+32+1,&abank,2);
		memcpy(data+32+3,&auser,4);
		memcpy(data+32+7,&bbank,2);
		memcpy(data+32+9,&buser,4);
		memcpy(data+32+13,&ttime,4);
	}

	usertxs(uint8_t nttype,uint16_t nabank,uint32_t nauser,uint32_t namsid,uint32_t nttime,uint16_t nbbank,uint32_t nbuser,int64_t ntmass,uint64_t ntinfo,char* text,char* okey) :
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
			return(false);}
		int len=txslen[ttype];
		if(ttype==TXSTYPE_BRO){
			size=32+len+bbank+64;}
	 	else if(ttype==TXSTYPE_KEY){ //user,newkey
			size=32+len+64+64;}
	 	else if(ttype==TXSTYPE_BKY){ //user0,newkey,oldkey
			size=32+len+64+64+64;}
		else{
			size=32+len+64;}
		data=(char*)std::malloc(size);
		data[32]=ttype;
		memcpy(data+32+1   ,&abank,2);
		memcpy(data+32+1+2 ,&auser,4);
		memcpy(data+32+1+6 ,&amsid,4);
		memcpy(data+32+1+10,&ttime,4);
		if(len==1+2+4+4+4)
			return;}
		if(ttype==TXSTYPE_KEY){
			memcpy(data+32+1+14,text,32);
			return;}
		if(ttype==TXSTYPE_BKY){
			memcpy(data+32+1+14,text,32);
			memcpy(data+32+1+14+32,text2,32);
			return;}
		memcpy(data+32+1+14,&bbank,2);
		if(ttype==TXSTYPE_BRO){
			memcpy(data+32+1+16,text,bbank);
			return;}
		if(len>=1+2+4+4+4+2+4){
			memcpy(data+32+1+16,&buser,4);
		if(len>=1+2+4+4+4+2+4+8+8){
			memcpy(data+32+1+20,&tmass,8);
			memcpy(data+32+1+28,&tinfo,8);}
	}

	uint32_t get_size(char* txs)
	{	if(*txs==TXSTYPE_USR){
			return(txslen[*txs]+64+4+32);}
                if(*txs==TXSTYPE_BRO){
			uint16_t len;
			memcpy(&len,txs+1+14,2);
			return(txslen[*txs]+64+len);}
                return(txslen[*txs]+64);
	}

	bool parse(char* txs) //TODO, should return parsed buffer length
	{	ttype=*txs;
		if(ttype>=TXSTYPE_MAX){
			return(false);}
		size=txslen[ttype]+64;
		memcpy(&abank,txs+1+0 ,2);
		memcpy(&auser,txs+1+2 ,4);
		if(ttype==TXSTYPE_INF){
			memcpy(&bbank,txs+1+6 ,2);
			memcpy(&buser,txs+1+8 ,4);
			memcpy(&ttime,txs+1+12,4);
			return(true);}
		memcpy(&amsid,txs+1+6 ,4);
		memcpy(&ttime,txs+1+10,4);
		memcpy(&bbank,txs+1+14,2);
		memcpy(&buser,txs+1+16,4);
		memcpy(&tmass,txs+1+20,8);
		memcpy(&tinfo,txs+1+28,8);
                if(ttype==TXSTYPE_USR){
                  size+=4+32;}
                else if(ttype==TXSTYPE_BRO){
                  size+=bbank;}
                return(true);
	}

	~usertxs()
	{	free(data);
	}

	void sign(uint8_t* hash,uint8_t* sk,uint8_t* pk)
	{	if(hash==NULL){
			assert(ttype==TXSTYPE_INF);
			ed25519_sign(data+32,txslen[TXSTYPE_INF],sk,pk,data+32+txslen[TXSTYPE_INF]);
			return;}
		memcpy(data,hash,32);
		if(ttype==TXSTYPE_BRO){
			ed25519_sign(data,32+txslen[TXSTYPE_BRO]+bbank,sk,pk,data+32+txslen[TXSTYPE_BRO]+bbank);
			return;}
		ed25519_sign(data,32+txslen[ttype],sk,pk,data+32+txslen[ttype]);
	}

	void sign2(uint8_t* sk,uint8_t* pk2) // additional signature with client, no need to supply pk (is in data)
	{	assert(ttype==TXSTYPE_KEY || ttype==TXSTYPE_BKY);
		ed25519_sign(data,32+txslen[ttype],sk,pk2,data+32+txslen[ttype]+64);
	}

	void sign3(uint8_t* sk,uint8_t* pk3) // additional signature with client, no need to supply pk (is in data)
	{	assert(ttype==TXSTYPE_BKY);
		ed25519_sign(data,32+txslen[ttype],sk,pk3,data+32+txslen[ttype]+64+64);
	}

	int wrong_sig(char* buf,uint8_t* hash,uint8_t* pk)
	{	if(ttype==TXSTYPE_INF){
			return(ed25519_sign_open(buf,txslen[TXSTYPE_INF],pk,buf+txslen[TXSTYPE_INF]));}
		if(ttype==TXSTYPE_BRO){
			return(ed25519_sign_open2(hash,32,buf,txslen[TXSTYPE_BRO]+bbank,pk,buf+txslen[TXSTYPE_BRO]+bbank));}
		return(ed25519_sign_open2(hash,32,buf,txslen[ttype],pk,buf+txslen[ttype]));
	}

	int wrong_sig2(char* buf,uint8_t* hash) // additional signature with client
	{	assert(ttype==TXSTYPE_KEY || ttype==TXSTYPE_BKY);
		return(ed25519_sign_open2(hash,32,buf,txslen[ttype],buf+1+2+4+4+4,buf+txslen[ttype]+64));
	}

	int wrong_sig3(char* buf,uint8_t* hash) // additional signature with client
	{	assert(ttype==TXSTYPE_BKY);
		return(ed25519_sign_open2(hash,32,buf,txslen[ttype],buf+1+2+4+4+4+32,buf+txslen[ttype]+64+64));
	}

	void print()
	{	char msgtxt[0x200];
	 	if(ttype==TXSTYPE_KEY || ttype==TXSTYPE_BKY){
			assert((txslen[ttype]+64+64)*2<0x200);
			ed25519_key2text(msgtxt,data+32,txslen[ttype]+64+64); // do not send last hash
			fprintf(stdout,"%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
		else{
			assert((txslen[ttype]+64)*2<0x200);
			ed25519_key2text(msgtxt,data+32,txslen[ttype]+64); // do not send last hash
			fprintf(stdout,"%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
	}

	/*char* message() //FIXME, don't use 32byte prefix
	{	assert(data!=NULL);
		return(data+32);
	}

	int length() //FIXME, don't use 32byte prefix
	{	assert(data!=NULL);
		return(size-32);
	}*/

	char* key(char* buf) //return new key in message
	{	return(buf+1+2+4+4+4);
	}

	char* key2(char* buf) //return old key in message
	{	return(buf+1+2+4+4+4+32);
	}

	char* broadcast(char* buf) //return broadcast buffer in message
	{	return(buf+1+2+4+4+4+2);
	}

	uint32_t nuser(char* buf) //return second user in message
	{	return((uint32_t*)(buf+1+2+4+4+4+2));
	}

	char* npkey(char* buf) //return second user key in message
	{	return(buf+1+2+4+4+4+2+4);
	}

private:
};
typedef boost::shared_ptr<usertxs> usertxs_ptr;
#endif
