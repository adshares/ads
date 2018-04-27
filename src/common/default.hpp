#ifndef DEFAULT_HPP
#define DEFAULT_HPP

#include <openssl/sha.h>
#include <boost/thread/mutex.hpp>

#include "default.h"

#define SERVER_DBL 0x1 /* closed node */
#define SERVER_VIP 0x2 /* VIP node */
#define SERVER_UNO 0x4 /* single master node */

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
typedef struct txspath {
    uint32_t path;
    uint32_t msid;
    uint16_t node;
    uint16_t tnum;
    uint16_t len; //64k lentgh limit !
    uint16_t hnum;
} txspath_t;
typedef struct user_s { // 4+4+32+32+2+2+4+4+4+8+32=96+32=128 bytes
    uint32_t msid; // id of last transaction, id==1 is the creation.
    uint32_t time; // original time of transaction (used as lock initiation)
    uint8_t pkey[SHA256_DIGEST_LENGTH]; //public key
    uint8_t hash[SHA256_DIGEST_LENGTH]; //users block hash
    uint32_t lpath; // block time of local transaction, used for dividends and locks // changed position !!!
    uint32_t user; // target user, ==user_id at creation // changed position !!!
    uint16_t node; // target node, ==bank_id at creation
    uint16_t stat; // includes status and account type
    uint32_t rpath; // block time of incomming transaction, used for dividends, MUST BE AFTER stat !
    int64_t weight; // balance, MUST BE AFTER rpath !
    uint64_t csum[4]; //sha256 hash of user account, MUST BE AFTER weight !
} user_t;
typedef struct log_s { //TODO, consider reducing the log to only txid (uint64_t)
    uint32_t time;
    uint16_t type;
    uint16_t node;
    uint32_t nmid; // peer msid, could overwrite this also with info
    uint32_t mpos; // changing to txs-num in file (previously: position in file or remote user in 'get')
    uint32_t user;
    uint32_t umid; // user msid
    int64_t weight; //
    uint8_t info[32];
} log_t;
typedef uint8_t svsi_t[2+(2*SHA256_DIGEST_LENGTH)]; // server_id + signature
typedef union {
    uint64_t v64;
    uint32_t v32[2];
    uint16_t v16[4];
} ppi_t;
//typedef union {uint64_t v64;uint16_t v16[4];} ppi_t;
typedef struct {
    uint32_t auser;
    uint32_t buser;
    uint8_t pkey[32];
} get_t;
typedef struct {
    uint32_t auser;
    uint16_t node;
    uint32_t user;
    uint32_t time;
    int64_t delta;
} gup_t;
typedef struct {
    uint32_t auser;
    int64_t weight;
} dep_t;
typedef struct {
    uint64_t deposit;
    int16_t sus;
    uint16_t uus;
} dsu_t;
typedef struct {
    uint32_t auser;
    uint16_t bbank;
    uint8_t pkey[32];
} usr_t;
typedef struct {
    uint32_t auser;
    uint16_t bbank;
    uint32_t buser;
    uint8_t pkey[32];
} uok_t;
typedef struct {
    uint16_t bbank;
    uint16_t abank;
    uint32_t auser;
    uint8_t pkey[32];
} uin_t;
typedef unsigned char hash_t[32]; // consider reducing this to uint64_t[4]
typedef union {
    hash_t hash;
    uint32_t hxor[8];
} hash_s;
typedef struct handshake_s { //maybe this should be just header_t + peer_msid
    header_t head; // last header
    int do_sync; // 0: in sync; 1: not in sync
    uint32_t msid; // peer msid
    uint8_t msha[SHA256_DIGEST_LENGTH]; // hash of last peer message
} handshake_t;
typedef struct msidsvidhash_s {
    uint32_t msid;
    uint16_t svid;
    hash_t sigh;
} msidsvidhash_t;
typedef struct msidhash_s {
    uint32_t msid;
    hash_t sigh;
} msidhash_t;
typedef struct nodekey_s {
    hash_t skey;
    hash_t pkey;
} nodekey_t;
#pragma pack()

typedef struct uin_cmp {
    bool operator()(const uin_t& i,const uin_t& j) const {
        int k=memcmp(&i,&j,sizeof(uin_t));
        return(k<0);
    }
} uin_c;
typedef struct hash_cmp {
    bool operator()(const hash_s& i,const hash_s& j) const {
        int k=memcmp(i.hash,j.hash,sizeof(hash_t));
        return(k<0);
    }
} hash_cmp_t;

#define RETURN_VAL_ON_SHUTDOWN(val) {extern bool finish;if(finish){return(val);}}
#define RETURN_ON_SHUTDOWN() {extern bool finish;if(finish){return;}}
#define SHUTDOWN_AND_RETURN_VAL(val) {std::raise(SIGQUIT);return(val);}
#define SHUTDOWN_AND_RETURN() {std::raise(SIGQUIT);return;}
#define RESTART_AND_RETURN() {std::raise(SIGUSR1);return;}
#ifndef ELOG
#define ELOG(...) {extern boost::mutex flog;extern FILE* stdlog;flog.lock();uint32_t logtime=time(NULL);fprintf(stderr, "%s:", __TIME__);fprintf(stderr,__VA_ARGS__);fprintf(stdlog,"%08X ",logtime);fprintf(stdlog,__VA_ARGS__);flog.unlock();}
#endif
#ifndef NDEBUG
//consider printing thread id
#ifndef DLOG
#define DLOG(...) {extern boost::mutex flog;extern FILE* stdlog;flog.lock();uint32_t logtime=time(NULL);fprintf(stderr, "%s:", __TIME__);fprintf(stderr,__VA_ARGS__);fprintf(stdlog,"%08X ",logtime);fprintf(stdlog,__VA_ARGS__);flog.unlock();}
#endif
#else
#define DLOG(...)
#endif

#endif // DEFAULT_HPP
