#ifndef MAIN_HPP
#define MAIN_HPP

#include <algorithm>
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
#include <forward_list>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <openssl/sha.h>
#include <set>
#include <stack>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#define BLOCKSEC 0x30
#define MAX_UNDO 8 /* maximum history of block undo files in blocks */
#define VOTE_DELAY 4 /*increase later (maybe monitor network delay)!!!*/
#define VOTES_MAX 127
#define VIP_MAX 127
#define MIN_MSGNUM 1 /*minimum number of messages to prevent hallo world, change to higher number later*/
#define MAX_CHECKQUE 8 /*maximum check queue size for emidiate message requests*/
#define MAX_MSGWAIT 8 /*wait no more than 8s for a message*/
#define MAX_USRWAIT 64 /*wait no more than 64s for a usr file*/
#define SYNC_WAIT 4 /* wait before another attempt to download servers */
#define MAXLOSS (BLOCKSEC*128) /*do not expect longer history from peers*/
#define TOTALMASS 0x8fffffffffffffff /*total weight of moon in MoonBlocks (8TonsOfMoon) or in seconds*/
#define MAX_USERS 0x80000000
#define LOCK_TIME (0x80*BLOCKSEC) /*time needed for lock to start; 2*LOCK_TIME => allow withdrawal*/
#define MAX_ACCOUNT 0x10000 /* maximum number of accounts in the "blacklist" */
#define LOG_PURGE_START (4096+2048) /* when this log size is reached try purging */
#define MAX_LOG_AGE (0x800*BLOCKSEC) /* purge first 4096 bytes if older than this age */

#define SERVER_TYPE 1
#define OFFICE_PORT "9080"
#define SERVER_PORT "9090"
//#define PATHSHIFT 8
#define PATHSHIFT 5
#define MAXCLIENTS 128

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
#define MSGTYPE_TXS 3	/* transaction */
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
#define MSGTYPE_TXL 18  /* txslist request */
#define MSGTYPE_TXP 19  /* txslist data */
#define MSGTYPE_PAT 20  /* current sync path */
#define MSGTYPE_USR 21  /* bank file */
#define MSGTYPE_USG 22  /* bank file request */
#define MSGTYPE_SOK 99  /* peer synced */

#define TXS_BRO_FEE(x) (0x1000+1*(x)) /* minimum 1hour fee + len_fee */
#define TXS_PUT_FEE(x) (0x1000+0.0001*(x)) /* minimum 1hour fee */
#define TXS_BNK_FEE    (0x10000000) /* */
#define TXS_GET_FEE    (0x100000) /* */
#define TXS_KEY_FEE    (0x1000) /* minimum 1hour fee */
#define TXS_BKY_FEE    (0x1000000) /* */

#define START_AGE      (0x100*BLOCKSEC) /* 100 blocks fee */

#define TIME_FEE(x,y) (0x10*((x)-(y))/BLOCKSEC) /* seconds */
#define MIN_MASS (0x800000) /* minimum 97days fee */

#define BANK_MAX (0xffff)
#define BANK_PROFIT(x) ((x)>>4) /* 1/16 of fees */
#define BANK_USER_FEE(x) ((x)>>6) /* 1s per 64 accounts (bank makes 8s on these accounts) */
#define BANK_TIME_FEE(x) (0x40*(x)) /* must have minimum of 512 accounts to make profits */
#define BANK_MIN_MASS   (0x20000000) /*FIXME, reduce !!! minimum 97days fee in admin account to send transactions */
#define BANK_MIN_MTIME (0x200*BLOCKSEC) /* if not transaction in this period bank can be taken over */
#define BANK_MIN_WEIGHT (0x1000000000) /* if bank weight below this value, bank can be taken over */

#define MESSAGE_TOO_LONG 0x800000
#define MESSAGE_LEN_OK 0x10000
#define MESSAGE_WAIT 5
#define MESSAGE_TOO_OLD (60*60*24*7)
#define MESSAGE_USRCHUNK 0x10000

#pragma pack(1)
typedef struct header_s {
	uint32_t now; // start time of block, MUST BE FIRST ELEMENT
	uint32_t txs; // number of transactions in block, FIXME, should be uint16_t
	uint32_t nod; // number of nodes in block, this could be uint16_t later, FIXME, should be uint16_t
	uint8_t oldhash[SHA256_DIGEST_LENGTH]; // previous hash
	uint8_t txshash[SHA256_DIGEST_LENGTH]; // hash of transactions
	uint8_t nodhash[SHA256_DIGEST_LENGTH]; // hash of nodes
	uint8_t nowhash[SHA256_DIGEST_LENGTH]; // current hash
	uint16_t vok; // vip ok votes stored by server, not signed !!! MUST BE LAST
	uint16_t vno; // vip no votes stored by server, not signed !!! MUST BE LAST
} header_t;
typedef union {uint64_t v64;uint32_t v32[2];uint16_t v16[4];} ppi_t;
typedef struct {uint32_t auser; uint32_t buser;uint8_t pkey[32]} get_t;
#pragma pack()

#include "user.hpp"
#include "ed25519/ed25519.h"
#include "options.hpp"
#include "message.hpp"
#include "servers.hpp"
#include "candidate.hpp"
#include "server.hpp"
#include "peer.hpp"
#include "office.hpp"
#include "client.hpp"

#endif
