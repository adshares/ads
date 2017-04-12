#ifndef MAIN_HPP
#define MAIN_HPP

//#define _GNU_SOURCE
#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
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
#include <dirent.h>
#include <fcntl.h>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <set>
#include <stack>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>


#define BLOCKSEC 0x20 /* block period in seconds */
#define BLOCKDIV 0x4 /* number of blocks for dividend update */
#define MAX_UNDO 8 /* maximum history of block undo files in blocks */
#define VOTE_DELAY 4 /*increase later (maybe monitor network delay)!!!*/
#define VOTES_MAX 127
#define VIP_MAX 127
#define MIN_MSGNUM 2 /*minimum number of messages to prevent hallo world, change to higher number later*/
#define MAX_CHECKQUE 8 /*maximum check queue size for emidiate message requests*/
#define MAX_MSGWAIT 8 /*wait no more than 8s for a message*/
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

#define MSGSTAT_INF 0
#define MSGSTAT_DAT 1
#define MSGSTAT_VAL 2
#define MSGSTAT_SAV 4

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
#define MESSAGE_LEN_OK 0x10000
#define MESSAGE_WAIT 5
#define MESSAGE_TOO_OLD (60*60*24*7)
#define MESSAGE_CHUNK 0x100000
#define MESSAGE_FEE      (0x1000) /* fee for each bank message */

#pragma pack(1)
typedef struct headlink_s { // header links sent when syncing
        uint32_t msg; // number of transactions in block, FIXME, should be uint16_t
        uint32_t nod; // number of nodes in block, this could be uint16_t later, FIXME, should be uint16_t
        uint32_t div; // dividend
        uint8_t msghash[SHA256_DIGEST_LENGTH]; // hash of transactions
        //uint8_t txshash[SHA256_DIGEST_LENGTH]; // hash of transactions
        uint8_t nodhash[SHA256_DIGEST_LENGTH]; // hash of nodes
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
	uint8_t nowhash[SHA256_DIGEST_LENGTH]; // current hash
	uint16_t vok; // vip ok votes stored by server, not signed !!! MUST BE LAST
	uint16_t vno; // vip no votes stored by server, not signed !!! MUST BE LAST
} header_t;
typedef union {uint64_t v64;uint32_t v32[2];uint16_t v16[4];} ppi_t;
typedef struct {uint32_t auser;uint32_t buser;uint8_t pkey[32];} get_t;
typedef struct {uint32_t auser;uint16_t node;uint32_t user;uint32_t time;int64_t delta;} gup_t;
typedef struct {uint32_t auser;int64_t weight;} dep_t;
typedef struct {uint32_t auser;uint16_t bbank;uint8_t pkey[32];} usr_t;
typedef struct {uint32_t auser;uint16_t bbank;uint32_t buser;uint8_t pkey[32];} uok_t;
typedef struct {uint16_t bbank;uint16_t abank;uint32_t auser;uint8_t pkey[32];} uin_t;
typedef struct uin_cmp {
  bool operator()(const uin_t& i,const uin_t& j) const {int k=memcmp(&i,&j,sizeof(uin_t));return(k<0);}} uin_c;
#pragma pack()

#define LOG(...) {extern boost::mutex flog;extern FILE* stdlog;flog.lock();fprintf(stderr,__VA_ARGS__);fprintf(stdlog,__VA_ARGS__);flog.unlock();}

#include "ed25519/ed25519.h"
#include "hash.hpp"
#include "user.hpp"
#include "options.hpp"
#include "message.hpp"
#include "servers.hpp"
#include "candidate.hpp"
#include "server.hpp"
#include "peer.hpp"
#include "office.hpp"
#include "client.hpp"

#endif // MAIN_HPP
