#ifndef USER_HPP
#define USER_HPP

//TODO, change the structure of this data and covert it to an array of structures or array of classes

//bank only
#define TXSTYPE_NON 0	/* placeholder for failed transactions */
#define TXSTYPE_CON 1	/* connected */			/* inform peers about location */
#define TXSTYPE_UOK 2	/* accept remote account request */
//bank & user
#define TXSTYPE_BRO 3	/* broadcast */			/* send ads to network */
#define TXSTYPE_PUT 4	/* send funds */	
#define TXSTYPE_MPT 5	/* send funds to many accounts */	
#define TXSTYPE_USR 6	/* create new user */		
#define TXSTYPE_BNK 7	/* create new bank */
#define TXSTYPE_GET 8	/* get funds */			/* retreive funds from remote/dead bank */
#define TXSTYPE_KEY 9	/* change account key */
#define TXSTYPE_BKY 10	/* change bank key */
#define TXSTYPE_SUS 11	/* NEW set user status bits (OR) */
#define TXSTYPE_SBS 12	/* NEW set bank status bits (OR) */
#define TXSTYPE_UUS 13	/* NEW unset user status bits (AND) */
#define TXSTYPE_UBS 14	/* NEW unset bank status bits (AND) */
#define TXSTYPE_SAV 15  /* NEW request account status confirmation in chain */
//overwriting user only TX in log
#define TXSTYPE_DIV 16	/* user dividend payment tx code */
#define TXSTYPE_FEE 17	/* bank fee payment tx code */
//user only
#define TXSTYPE_INF 16	/* strating from this message only messages between wallet and client.hpp */
#define TXSTYPE_LOG 17	/* return user status and log */
#define TXSTYPE_BLG 18	/* return broadcast log */
#define TXSTYPE_BLK 19	/* return block list */
#define TXSTYPE_TXS 20	/* return transaction + hash path */
#define TXSTYPE_VIP 21	/* return vip public keys */
#define TXSTYPE_SIG 22	/* return block signatures */
#define TXSTYPE_NDS 23	/* return nodes (servers.srv) */
#define TXSTYPE_NOD 24	/* return users of a node */
#define TXSTYPE_MGS 25	/* return list of messages */
#define TXSTYPE_MSG 26	/* return message */
//end
#define TXSTYPE_MAX 27  /* should be 0xFE, with txslen[0xFE]=max_fixed_transaction_size */

const char* txsname[TXSTYPE_MAX]={
	"empty",		//0
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
	"set_account_status",	//11 NEW
	"set_node_status",	//12 NEW
	"unset_account_status",	//13 NEW
	"unset_node_status",	//14 NEW
	"log_account",		//15 NEW
	"get_account",		//16 also 'get_me'
	"get_log",		//17
	"get_broadcast",	//18
	"get_blocks",		//19
	"get_transaction",	//20
	"get_vipkeys",		//21
	"get_signatures",	//22
	"get_block",		//23
	"get_accounts",		//24
	"get_message_list",	//25
	"get_message"};		//26

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
	"set_account_status",	//11 NEW
	"set_node_status",	//12 NEW
	"unset_account_status",	//13 NEW
	"unset_node_status",	//14 NEW
	"log_account",		//15 NEW
	"dividend",		//16
	"bank_profit",		//17
	"unknown",		//18
	"unknown",		//19
	"unknown",		//20
	"unknown",		//21
	"unknown",		//22
	"unknown",		//23
	"unknown",		//24
	"unknown",		//25
	"unknown"};		//26

const int txslen[TXSTYPE_MAX+1]={ //length does not include variable part and input hash
	1+3,			//0:NON placeholder for failed trsnactions (not defined yet)
	1+2+4,			//1:CON (port,ipv4), (port==0) => bank shutting down , should add office addr:port
	1+2+4+4+4+2+4+32,	//2:UOK new account, not signed
	1+2+4+4+4+2,		//3:BRO 'bbank' is message length
	1+2+4+4+4+2+4+8+32,	//4:PUT, extra parameter = official message (32 byte)
	1+2+4+4+4+2,		//5:MPT 'bbank' is number of to_accounts
	1+2+4+4+4+2,		//6:USR
	1+2+4+4+4,		//7:BNK
	1+2+4+4+4+2+4,		//8:GET,
	1+2+4+4+4+32,		//9:KEY
	1+2+4+4+4+2+32,		//10:BKY CHANGED ! (added bank id), old key appended to undo message
	1+2+4+4+4+2+4+2,	//11:SUS NEW
	1+2+4+4+4+2+4,		//12:SBS NEW
	1+2+4+4+4+2+4+2,	//13:UUS NEW
	1+2+4+4+4+2+4,		//14:UBS NEW
	1+2+4+4+4,		//15:SAV NEW , server adds 128 bytes (sizeof(user_t))
	1+2+4+2+4+4,		//16:INF
	1+2+4+2+4+4,		//17:LOG
	1+2+4+4+4,		//18:BLG /amsid=blk_number/
	1+2+4+4+4+4,		//19:BLK /amsid=blk_from blk_to/
	1+2+4+4+4+4+2,		//20:TXS /bbank=node_ buser=node-msid_ amsid=position/
	1+2+4+4+32,		//21:VIP /tinfo=vip_hash/
	1+2+4+4+4,		//22:SIG /amsid=blk_number/
	1+2+4+4+4,		//22:NDS /amsid=blk_number/
	1+2+4+4+4+2,		//22:NOD /amsid=blk_number/
	1+2+4+4+4,		//22:MGS /amsid=blk_number/
	1+2+4+4+4+2+4,		//22:MSG /amsid=blk_number buser=node_msid/
	1+2+4+4+4+2+4+8+32};	//23:MAX fixed buffer size
	
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

	usertxs(uint32_t len) :
		ttype(TXSTYPE_NON),
		abank(0),
		auser(0),
		amsid(0),
		ttime(0),
		bbank(0),
		buser(0),
		tmass(0),
		size(len<4?4:len & 0xFFFFFF)
	{	data=(uint8_t*)std::malloc(size);
		data[0]=ttype;
		memcpy(data+1,&size,3);
		if(size>4){
			bzero(data+4,size-4);}
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
		DLOG("BAD MSG: %X\n",ttype);
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
			DLOG("ERROR, creating message\n");
			size=0;
			return;}
		int len=txslen[ttype];
		// to add new transaction set: size, data
		if(ttype==TXSTYPE_BLK){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&ttime,4);
			memcpy(data+1+10,&amsid,4); // block number
			memcpy(data+1+14,&buser,4); // block number to
			return;}
		if(ttype==TXSTYPE_TXS){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&ttime,4);
			memcpy(data+1+10,&amsid,4); // position
			memcpy(data+1+14,&buser,4); // bank-msid
			memcpy(data+1+18,&bbank,2); // 
			return;}
		if(ttype==TXSTYPE_VIP){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&ttime,4);
			memcpy(data+1+10, tinfo,32); // vip hash
			return;}
		if(ttype==TXSTYPE_SIG || ttype==TXSTYPE_NDS || ttype==TXSTYPE_MGS){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&ttime,4);
			memcpy(data+1+10,&amsid,4); // block number
			return;}
		if(ttype==TXSTYPE_NOD){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&ttime,4);
			memcpy(data+1+10,&amsid,4); // block number
			memcpy(data+1+14,&bbank,2);
			return;}
		if(ttype==TXSTYPE_MSG){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&ttime,4);
			memcpy(data+1+10,&amsid,4); // block number
			memcpy(data+1+14,&bbank,2);
			memcpy(data+1+16,&buser,4); // node-msid
			return;}
		if(ttype==TXSTYPE_BKY){ // for BKY, office adds old key
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&amsid,4);
			memcpy(data+1+10,&ttime,4);
			memcpy(data+1+14,&bbank,2);
			memcpy(data+1+16,text,32);
			return;}
		if(ttype==TXSTYPE_SUS || ttype==TXSTYPE_UUS){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&amsid,4);
			memcpy(data+1+10,&ttime,4);
			memcpy(data+1+14,&bbank,2);
			memcpy(data+1+16,&buser,4);
			memcpy(data+1+20,&tmass,2); //only 2 bytes
			return;}
		if(ttype==TXSTYPE_SBS || ttype==TXSTYPE_UBS){
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&amsid,4);
			memcpy(data+1+10,&ttime,4);
			memcpy(data+1+14,&bbank,2);
			memcpy(data+1+16,&tmass,4); //only 4 bytes
			return;}
		if(ttype==TXSTYPE_SAV){ // for SAV server adds 128 bytes of user data
			size=len+64;
			data=(uint8_t*)std::malloc(size);
			data[0]=ttype;
			memcpy(data+1   ,&abank,2);
			memcpy(data+1+2 ,&auser,4);
			memcpy(data+1+6 ,&amsid,4);
			memcpy(data+1+10,&ttime,4);
			return;}

		//TODO, process other transactions as above

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
		if(ttype==TXSTYPE_KEY){
			memcpy(data+1+14,text,32);
			return;}
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
                        DLOG("ERROR, parsing message type\n");
                        return(0xFFFFFFFF);}
		if(*txs==TXSTYPE_CON){
			return(txslen[TXSTYPE_CON]);} // no signature
		if(*txs==TXSTYPE_UOK){
			return(txslen[TXSTYPE_UOK]);} // no signature
	 	if(*txs==TXSTYPE_BKY){
			return(txslen[TXSTYPE_BKY]+64+32);} // additional 32 bytes on network !!!
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
		if(*txs==TXSTYPE_NON){
			uint32_t len=0;
			memcpy(&len,txs+1,3);
			return(len<4?4:len);}
	 	if(*txs==TXSTYPE_SAV){
			return(txslen[TXSTYPE_SAV]+64+sizeof(user_t));} // additional 128 bytes on network !!!
		return(txslen[(int)*txs]+64);
	}

	bool parse(char* txs) //TODO, should return parsed buffer length
	{	ttype=*txs;
		if(ttype>=TXSTYPE_MAX){
			DLOG("ERROR, parsing message\n");
			return(false);}
		if(ttype==TXSTYPE_NON){
			size=0;
			memcpy(&size,txs+1,3);
			if(size<4){
				size=4;}
			return(true);}
		if(ttype==TXSTYPE_SAV){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4); // "+3" is use to retrieve auser in message::insert_user()
			memcpy(&amsid,txs+1+6 ,4);
			memcpy(&ttime,txs+1+10,4);
			size=txslen[ttype]+64+sizeof(user_t);
			return(true);}
		size=txslen[ttype]+64;
		if(ttype==TXSTYPE_BLK){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&ttime,txs+1+6 ,4);
			memcpy(&amsid,txs+1+10,4); // block number from
			memcpy(&buser,txs+1+14,4); // block number to
			return(true);}
		if(ttype==TXSTYPE_TXS){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&ttime,txs+1+6 ,4);
			memcpy(&amsid,txs+1+10,4); // position
			memcpy(&buser,txs+1+14,4); // bank-msid
			memcpy(&bbank,txs+1+18,2);
			return(true);}
		if(ttype==TXSTYPE_VIP){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&ttime,txs+1+6 ,4);
			memcpy( tinfo,txs+1+10,32); // vip hash
			return(true);}
		if(ttype==TXSTYPE_SIG || ttype==TXSTYPE_NDS || ttype==TXSTYPE_MGS){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&ttime,txs+1+6 ,4);
			memcpy(&amsid,txs+1+10,4); // block number
			return(true);}
		if(ttype==TXSTYPE_NOD){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&ttime,txs+1+6 ,4);
			memcpy(&amsid,txs+1+10,4); // block number
			memcpy(&bbank,txs+1+14,2);
			return(true);}
		if(ttype==TXSTYPE_MSG){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&ttime,txs+1+6 ,4);
			memcpy(&amsid,txs+1+10,4); // block number
			memcpy(&bbank,txs+1+14,2);
			memcpy(&buser,txs+1+16,4); // node-msid
			return(true);}

		if(ttype==TXSTYPE_SUS || ttype==TXSTYPE_UUS){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&amsid,txs+1+6 ,4);
			memcpy(&ttime,txs+1+10,4);
			memcpy(&bbank,txs+1+14,2);
			memcpy(&buser,txs+1+16,4);
			memcpy(&tmass,txs+1+20,2); //2 bytes
			return(true);}
		if(ttype==TXSTYPE_SBS || ttype==TXSTYPE_UBS){
			memcpy(&abank,txs+1+0 ,2);
			memcpy(&auser,txs+1+2 ,4);
			memcpy(&amsid,txs+1+6 ,4);
			memcpy(&ttime,txs+1+10,4);
			memcpy(&bbank,txs+1+14,2);
			memcpy(&tmass,txs+1+16,4); //2 bytes
			return(true);}

		//TODO process other transactions as above
		memcpy(&abank,txs+1+0 ,2);
		memcpy(&auser,txs+1+2 ,4);
		if(ttype==TXSTYPE_CON){
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
		if(ttype==TXSTYPE_BKY){
			size+=32;}
		else if(ttype==TXSTYPE_USR){
			size+=4+32;}
		else if(ttype==TXSTYPE_BRO){
			size+=bbank;}
		else if(ttype==TXSTYPE_MPT){
			size+=bbank*(6+8);}
		return(true);
	}

	uint8_t* get_sig(char* buf)
	{	if(*buf==TXSTYPE_BRO){
			return((uint8_t*)buf+txslen[TXSTYPE_BRO]+bbank);}
	 	if(*buf==TXSTYPE_MPT){
			return((uint8_t*)buf+txslen[TXSTYPE_MPT]+bbank*(6+8));}
		return((uint8_t*)buf+txslen[(int)*buf]);
	}

	void sign(uint8_t* hash,uint8_t* sk,uint8_t* pk)
	{	switch(ttype){
			case TXSTYPE_CON:
			case TXSTYPE_UOK:
				return;
			case TXSTYPE_INF:
			case TXSTYPE_LOG:
			case TXSTYPE_BLG:
			case TXSTYPE_BLK:
			case TXSTYPE_TXS:
			case TXSTYPE_VIP:
			case TXSTYPE_SIG:
			case TXSTYPE_NDS:
			case TXSTYPE_NOD:
			case TXSTYPE_MGS:
			case TXSTYPE_MSG:
				ed25519_sign(data,txslen[ttype],sk,pk,data+txslen[ttype]);
				break;
			case TXSTYPE_BRO:
				ed25519_sign2(hash,32,data,txslen[TXSTYPE_BRO]+bbank,sk,pk,data+txslen[TXSTYPE_BRO]+bbank);
				break;
			case TXSTYPE_MPT:
				ed25519_sign2(hash,32,data,txslen[TXSTYPE_MPT]+bbank*(6+8),sk,pk,data+txslen[TXSTYPE_MPT]+bbank*(6+8));
				break;
			default:
				ed25519_sign2(hash,32,data,txslen[ttype],sk,pk,data+txslen[ttype]);}
	}
	/*{	if(ttype==TXSTYPE_CON){
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
	}*/

	int wrong_sig(uint8_t* buf,uint8_t* hash,uint8_t* pk)
	{	switch(ttype){
			case TXSTYPE_CON:
			case TXSTYPE_UOK:
				return(0);
			case TXSTYPE_INF:
			case TXSTYPE_LOG:
			case TXSTYPE_BLG:
			case TXSTYPE_BLK:
			case TXSTYPE_TXS:
			case TXSTYPE_VIP:
			case TXSTYPE_SIG:
			case TXSTYPE_NDS:
			case TXSTYPE_NOD:
			case TXSTYPE_MGS:
			case TXSTYPE_MSG:
				return(ed25519_sign_open(buf,txslen[ttype],pk,buf+txslen[ttype]));
			case TXSTYPE_BRO:
				return(ed25519_sign_open2(hash,32,buf,txslen[TXSTYPE_BRO]+bbank,pk,buf+txslen[TXSTYPE_BRO]+bbank));
			case TXSTYPE_MPT:
				return(ed25519_sign_open2(hash,32,buf,txslen[TXSTYPE_MPT]+bbank*(6+8),pk,buf+txslen[TXSTYPE_MPT]+bbank*(6+8)));
			default:
				return(ed25519_sign_open2(hash,32,buf,txslen[ttype],pk,buf+txslen[ttype]));}
	}
	/*{	if(ttype==TXSTYPE_CON){
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
	}*/

	int sign2(uint8_t* sig) // additional signature to validate public key
	{	assert(ttype==TXSTYPE_KEY);
		memcpy(data+txslen[ttype]+64,sig,64);
		return(wrong_sig2(data));
	}

	int wrong_sig2(uint8_t* buf) // additional signature to validate public key
	{	assert(ttype==TXSTYPE_KEY);
		return(ed25519_sign_open((uint8_t*)NULL,0,buf+1+2+4+4+4,buf+txslen[ttype]+64));
	}

	int sign_mlen2()
	{	if(ttype==TXSTYPE_BRO){
			return(txslen[TXSTYPE_BRO]+bbank);}
		if(ttype==TXSTYPE_MPT){
			return(txslen[TXSTYPE_MPT]+bbank*(6+8));}
		return(txslen[ttype]);
	}

	void print_head()
	{	DLOG("MSG: %1X %04X:%08X m:%08X t:%08X b:%04X u:%08X v:%016lX (l:%d)\n",
			ttype,abank,auser,amsid,ttime,bbank,buser,tmass,size);
	}

	void print()
	{	char msgtxt[0x200];
	 	if(ttype==TXSTYPE_KEY){
			assert((txslen[ttype]+64+64)*2<0x200);
			ed25519_key2text(msgtxt,data,txslen[ttype]+64+64); // do not send last hash
			DLOG("%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
		else{
			assert((txslen[ttype]+64)*2<0x200);
			ed25519_key2text(msgtxt,data,txslen[ttype]+64); // do not send last hash
			DLOG("%.*s\n",(txslen[ttype]+64)*2,msgtxt);}
	}

	void change_time(uint32_t nttime)
	{	assert(ttype==TXSTYPE_BLG);
		ttime=nttime;
		memcpy(data+1+6,&ttime,4);
		return;
	}

	char* usr(char* buf) //return user_t data in SAV message
	{	assert(*buf==TXSTYPE_SAV);
	 	return(buf+1+2+4+4+4+64);
	}

	char* vip(char* buf) //return vip hash in message
	{	return(buf+1+2+4+4);
	}

	char* key(char* buf) //return new key in message
	{	if(*buf==TXSTYPE_BKY){
			return(buf+1+2+4+4+4+2);}
		return(buf+1+2+4+4+4); // _KEY
	}

	char* opkey(char* buf) //return old bank key in message
	{	assert(*buf==TXSTYPE_BKY);
		return(buf+1+2+4+4+4+2+32+64);
	}


	char* upkey(char* buf) //return new key in UOK message
	{	return(buf+1+2+4+4+4+2+4);
	}

	void print_broadcast(char* buf)
	{	DLOG("BRO:%.*s\n",bbank,broadcast(buf));
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
			DLOG("MPT to %04X %08X %016lX\n",tbank,tuser,tmass);}
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

private:
};
typedef boost::shared_ptr<usertxs> usertxs_ptr;

#endif // USER_HPP
