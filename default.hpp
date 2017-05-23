#ifndef DEFAULT_HPP
#define DEFAULT_HPP

#ifdef DEBUG
# define BLOCKSEC 0x20 /* block period in seconds */
# define BLOCKDIV 0x4 /* number of blocks for dividend update */
#else
# error define correct BLOCKSEC and BLOCKDIV
#endif
#define MAX_UNDO 8 /* maximum history of block undo files in blocks */
#define VOTE_DELAY 4 /*increase later (maybe monitor network delay)!!!*/
#define VOTES_MAX 127
#define VIP_MAX 127
#define MIN_MSGNUM 2 /*minimum number of messages to prevent hallo world, change to higher number later*/
#define MAX_CHECKQUE 8 /*maximum check queue size for emidiate message requests*/
#define MAX_MSGWAIT 2 /* start with 2 and chenge to 8: wait no more than 8s for a message */
#define MAX_USRWAIT 64 /*wait no more than 64s for a usr file*/
#define MAX_ELEWAIT (BLOCKSEC/2) /*wait no more than this for candidate votes, FIXME, + time after last vote*/
#define SYNC_WAIT 4 /* wait before another attempt to download servers */
#define MAXLOSS (BLOCKSEC*128) /*do not expect longer history from peers*/
#define TOTALMASS 0x4000000000000000 /*total weight of moon in MoonBlocks (16TonsOfMoon) or in seconds*/
#define MAX_USERS 0x40000000
//#define LOCK_TIME (0x80*BLOCKSEC) /*time needed for lock to start; 2*LOCK_TIME => allow withdrawal*/
#define LOCK_TIME (0x2*BLOCKSEC) /*time needed for lock to start; 2*LOCK_TIME => allow withdrawal*/
#define MAX_ACCOUNT 0x10000 /* maximum number of accounts in the "blacklist" */
#define LOG_PURGE_START (4096+2048) /* when this log size is reached try purging */
#define MAX_LOG_AGE (0x800*BLOCKSEC) /* purge first 4096 bytes if older than this age */
#define MAX_BLG_SIZE 0xFFFFFFF0 /* blog size larger than this will not be submitted to clients */
#define MIN_PEERS 8 /* keep at least 8 peers connected */
#define MAX_PEERS 32 /* keep not more than 16 peers connected */

#define SERVER_TYPE 1
#define OFFICE_PORT "9080"
#define SERVER_PORT "9090"
//#define PATHSHIFT 8
#define PATHSHIFT 5
#define MAXCLIENTS 128
#define CLIENT_POOL 2	/* do not offer more threads that are used for network message validation */

#define SERVER_DBL 0x1
#define SERVER_VIP 0x2
#define MESSAGE_MAXAGE 100

//#define MSGSTAT_INF 0x0
#define MSGSTAT_DAT 0x1 /* downloaded */
#define MSGSTAT_SAV 0x2 /* saved */
#define MSGSTAT_COM 0x4 /* commited */
#define MSGSTAT_VAL 0x8 /* validated */
#define MSGSTAT_BAD 0x10 /* invalid */
#define MSGSTAT_SIG 0x20 /* signature failure */

#define MSGTYPE_DBL 0	/* double spend proof, maybe we should start with 1 */
#define MSGTYPE_DBP 1
#define MSGTYPE_DBG 2
#define MSGTYPE_MSG 3	/* transaction */
#define MSGTYPE_PUT 4
#define MSGTYPE_GET 5
#define MSGTYPE_INI 6	/* connect */
#define MSGTYPE_ERR 7
#define MSGTYPE_STP 8	/* snapshot */
#define MSGTYPE_BAD 9
#define MSGTYPE_CND 10	/* candidate */
#define MSGTYPE_CNP 11	/* candidate put */
#define MSGTYPE_CNG 12	/* candidate get */
#define MSGTYPE_BLK 13	/* new block */
#define MSGTYPE_BLP 14	/* new block put */
#define MSGTYPE_BLG 15	/* new block get */
#define MSGTYPE_SER 16  /* servers request */
#define MSGTYPE_HEA 17  /* headers request */
#define MSGTYPE_MSL 18  /* msglist request */
#define MSGTYPE_MSP 19  /* msglist data */
#define MSGTYPE_PAT 20  /* current sync path */
#define MSGTYPE_USR 21  /* bank file */
#define MSGTYPE_USG 22  /* bank file request */
#define MSGTYPE_NHR 23  /* next header request */
#define MSGTYPE_NHD 24  /* next header data */
#define MSGTYPE_SOK 99  /* peer synced */

#define TXS_MIN_FEE      (0x1000) /* minimum fee per transaction */
#define TXS_DIV_FEE      (0x100000)  /* dividend fee collected every BLOCKDIV blocks ( *8_years=MIN_MASS ) */
#define TXS_KEY_FEE      (0x1000) /* */
#define TXS_BRO_FEE(x)   (0x1000  +0x10000*(x)) /* + len_fee (length) MAX_BLG_SIZE=1G */
#define TXS_PUT_FEE(x)   (0x1000  +((x)>>13)) /* local wires fee (weight) (/8192) */
#define TXS_LNG_FEE(x)   (         ((x)>>13)) /* additional remote wires fee (weight) */
#define TXS_MPT_FEE(x)   (0x100   +((x)>>13)) /* + MIN_FEE !!! */
#define TXS_GET_FEE      (0x100000) /* get initiation fee */
#define TXS_GOK_FEE(x)   (         ((x)>>12)) /* get wire fee (allways remote) */
#define TXS_USR_FEE      (0x100000) /* 0x0.001G only for remote applications, otherwise MIN_FEE */
#define TXS_BNK_FEE      (0x10000000) /* 0x0.1 G */
#define TXS_BKY_FEE      (0x10000000) /* 0x0.1 G */
#define USER_MIN_MASS    (0x10000000) /* 0x0.1 G minimum user account mass to send transaction */
#define USER_MIN_AGE     (BLOCKSEC*4) /* wait at least 4 blocks before deleting an account */
#define BANK_MIN_UMASS   (0x10000000) /* 0x0.1G, minimum admin account mass to send transaction */
#define BANK_MIN_TMASS   (0x1000000000) /* 0x10G, if bank total mass below this value, bank can be taken over */
#define BANK_MIN_MTIME   (0x200*BLOCKSEC) /* if no transaction in this period bank can be taken over */
#define BANK_MAX         (0xffff)
#define BANK_PROFIT(x)   ((x)>>4) /* 1/16 of fees */
#define BANK_USER_FEE(x) (0x1000 + ((BANK_PROFIT(((uint64_t)(x))*(TXS_DIV_FEE/BLOCKDIV)))>>2)) /* every block */

#define USER_STAT_DELETED 0x1 /* account can be deleted and reused */

#define MESSAGE_TOO_LONG 0x800000
#define MESSAGE_LEN_OK   0x10000
#define MESSAGE_TNUM_OK  0x1000
#define MESSAGE_TNUM_MAX 0xFFF0
#define MESSAGE_WAIT 5
#define MESSAGE_TOO_OLD (60*60*24*7)
#define MESSAGE_CHUNK    0x100000
#define MESSAGE_FEE      (0x1000) /* fee for each bank message */

#pragma pack(1)
typedef struct headlink_s { // header links sent when syncing
        uint32_t msg; // number of transactions in block, FIXME, should be uint16_t
        uint32_t nod; // number of nodes in block, this could be uint16_t later, FIXME, should be uint16_t
        uint32_t div; // dividend
        uint8_t msghash[SHA256_DIGEST_LENGTH]; // hash of transactions
        //uint8_t txshash[SHA256_DIGEST_LENGTH]; // hash of transactions
        uint8_t nodhash[SHA256_DIGEST_LENGTH]; // hash of nodes
        uint8_t viphash[SHA256_DIGEST_LENGTH]; // hash of vip public keys
} headlink_t;
typedef struct header_s {
	uint32_t now; // start time of block, MUST BE FIRST ELEMENT
	uint32_t msg; // number of transactions in block, FIXME, should be uint16_t
	uint32_t nod; // number of nodes in block, this could be uint16_t later, FIXME, should be uint16_t
	uint32_t div; // dividend
	uint8_t oldhash[SHA256_DIGEST_LENGTH]; // previous hash
	uint8_t msghash[SHA256_DIGEST_LENGTH]; // hash of messages
	//uint8_t txshash[SHA256_DIGEST_LENGTH]; // hash of transactions
	uint8_t nodhash[SHA256_DIGEST_LENGTH]; // hash of nodes
	uint8_t viphash[SHA256_DIGEST_LENGTH]; // hash of vip public keys
	uint8_t nowhash[SHA256_DIGEST_LENGTH]; // current hash
	uint16_t vok; // vip ok votes stored by server, not signed !!! MUST BE LAST FOR CORRECT SIGNATURE
	uint16_t vno; // vip no votes stored by server, not signed !!! MUST BE LAST FOR CORRECT SIGNATURE
	//uint16_t vtot; // any use ???
} header_t;
typedef struct node_s {
	uint8_t pk[SHA256_DIGEST_LENGTH]; // public key
	uint8_t hash[SHA256_DIGEST_LENGTH]; // hash of accounts (XOR of checksums)
	uint8_t msha[SHA256_DIGEST_LENGTH]; // hash of last message
	uint32_t msid; // last message, server closed if msid==0xffffffff
	uint32_t mtim; // time of last message
	 int64_t weight; // weight in units of 8TonsOfMoon (-1%) (in MoonBlocks)
	uint32_t status; // placeholder for future status settings, can include hash type
	uint32_t users; // placeholder for users (size)
	uint32_t port;
	uint32_t ipv4;
} node_t;
typedef struct txspath{
	uint32_t path;
	uint32_t msid;
	uint16_t node;
	uint16_t tnum;
	uint16_t len; //64k lentgh limit !
	uint16_t hnum;
} txspath_t;
typedef uint8_t svsi_t[2+(2*SHA256_DIGEST_LENGTH)]; // server_id + signature
typedef union {uint64_t v64;uint32_t v32[2];uint16_t v16[4];} ppi_t;
typedef struct {uint32_t auser;uint32_t buser;uint8_t pkey[32];} get_t;
typedef struct {uint32_t auser;uint16_t node;uint32_t user;uint32_t time;int64_t delta;} gup_t;
typedef struct {uint32_t auser;int64_t weight;} dep_t;
typedef struct {uint32_t auser;uint16_t bbank;uint8_t pkey[32];} usr_t;
typedef struct {uint32_t auser;uint16_t bbank;uint32_t buser;uint8_t pkey[32];} uok_t;
typedef struct {uint16_t bbank;uint16_t abank;uint32_t auser;uint8_t pkey[32];} uin_t;
typedef unsigned char hash_t[32]; // consider reducing this to uint64_t[4]
typedef struct {hash_t hash;} hash_s;
typedef struct handshake_s { //maybe this should be just header_t + peer_msid
	//uint16_t type; // version of server, not needed (is in servers.hpp)
	//uint16_t srvn; // number of legal servers
	//uint32_t ipv4; // ip of connecting server
	//uint32_t port; // port of connecting server
	//uint32_t path; // last block
	//uint8_t hash[SHA256_DIGEST_LENGTH]; // hash of last block
	header_t head; // last header
	int do_sync; // 0: in sync; 1: not in sync
	uint32_t msid; // peer msid
	uint8_t msha[SHA256_DIGEST_LENGTH]; // hash of last peer message
} handshake_t;
//typedef struct svidmsidhash_s {
//	uint16_t svid;
//	uint32_t msid;
//	hash_t sigh;
//} svidmsidhash_t;
typedef struct msidsvidhash_s {
	uint32_t msid;
	uint16_t svid;
	hash_t sigh;
} msidsvidhash_t;
typedef struct msidhash_s {
	uint32_t msid;
	hash_t sigh;
} msidhash_t;
#pragma pack()

typedef struct uin_cmp {
  bool operator()(const uin_t& i,const uin_t& j) const {int k=memcmp(&i,&j,sizeof(uin_t));return(k<0);}
} uin_c;
typedef struct hash_cmp {
  bool operator()(const hash_s& i,const hash_s& j) const {int k=memcmp(i.hash,j.hash,sizeof(hash_t));return(k<0);}
} hash_cmp_t;

#define LOG(...) {extern boost::mutex flog;extern FILE* stdlog;flog.lock();uint32_t logtime=time(NULL);fprintf(stderr,__VA_ARGS__);fprintf(stdlog,"%08X ",logtime);fprintf(stdlog,__VA_ARGS__);flog.unlock();}

#endif // DEFAULT_HPP