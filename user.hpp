#ifndef USER_HPP
#define USER_HPP

#define TXSTYPE_INF 0	/* info */			/* no broadcast, just for office comunication */
#define TXSTYPE_CON 1	/* connected */			/* inform peers about location */
#define TXSTYPE_BRO 2	/* broadcast */			/* send ads to network */
#define TXSTYPE_PUT 3	/* send funds */	
#define TXSTYPE_USR 4	/* create new user */		
#define TXSTYPE_BNK 5	/* create new bank */
#define TXSTYPE_GET 6	/* get funds */			/* retreive funds from remote/dead bank */
#define TXSTYPE_KEY 7	/* change account key */
#define TXSTYPE_BKY 8	/* change bank key */
#define TXSTYPE_STP 9	/* die/stop processing bank */	/* restart requires a new BKY transaction */
#define TXSTYPE_MAX 10
const int txslen[TXSTYPE_MAX]={ //length does not include variable part and input hash
	1+2+4+4,		//INF 'amsid' is current time
	0,			//CON not defined yet
	1+2+4+4+4+2,		//BRO 'bbank' is message length
	1+2+4+4+4+2+4+8,	//PUT
	1+2+4+4+4+2,		//USR
	1+2+4+4+4,		//BNK
	1+2+4+4+4+2+4+8,	//GET
	1+2+4+4+4+32,		//KEY
	1+2+4+4+4+32,		//BKY
	0}; 			//CON not defined yet
	

#pragma pack(1)
typedef struct user_s { // 8+32+32+4+4+8+4+2+2=96 bytes
	uint32_t id; // id of last transaction, id==1 is the creation.
	uint32_t block; // last txs block time [to find the transaction]
	 int64_t weight; // balance
	uint8_t pkey[SHA256_DIGEST_LENGTH]; //public key
	uint8_t hash[SHA256_DIGEST_LENGTH]; //users block hash
	uint64_t withdraw; //amount to withdraw to target
	uint32_t user; // target user
	uint16_t node; // target node
	uint16_t status; // includes status and account type
} user_t;
#pragma pack()

class usertxs
{
public:
	uint8_t  ttype;
	uint16_t abank;
	uint32_t auser;
	uint32_t amsid;
	uint32_t ttime;
	uint16_t bbank;
	uint32_t buser;
	 int64_t tmass;

	usertxs()
	{
	}
	
	bool parse(char* txs)
	{	ttype=*txs;
		if(ttype>=TXSTYPE_MAX){
			return(false);}
		memcpy(&abank,txs+1+0 ,2);
		memcpy(&auser,txs+1+2 ,4);
		memcpy(&amsid,txs+1+6 ,4);
		memcpy(&ttime,txs+1+10,4);
		int len=txslen[ttype];
		if(len>=1+2+4+4+4+32){
			return(true);}
		if(len>=1+2+4+4+4+2){
			memcpy(&bbank,txs+1+14,2);}
		if(len>=1+2+4+4+4+2+4+8){
			memcpy(&buser,txs+1+16,4);
			memcpy(&tmass,txs+1+20,2);}
                return(true);
	}




private:
};
#endif
