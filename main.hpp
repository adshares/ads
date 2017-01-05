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
#include <vector>

#define BLOCKSEC 0x30
#define VOTE_DELAY 4 /*increase later (maybe monitor network delay)!!!*/
#define VOTES_MAX 127
#define VIP_MAX 127
#define MIN_MSGNUM 1 /*minimum number of messages to prevent hallo world, change to higher number later*/
#define MAX_CHECKQUE 8 /*maximum check queue size for emidiate message requests*/
#define MAX_MSGWAIT 4 /*wait no more than 4s for a message*/
#define SYNC_WAIT 4 /* wait before another attempt to download servers */
#define MAXLOSS (BLOCKSEC*128) /*do not expect longer history from peers*/
#define TOTALMASS 0x8fffffffffffffff /*total weight of moon in MoonBlocks (8TonsOfMoon) or in seconds*/

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
#define MSGTYPE_SOK 99  /* peer synced */

#define TXSTYPE_PER 1	/* peer connected */
#define TXSTYPE_BRO 2	/* broadcast */
#define TXSTYPE_CNP 3	/* create new peer */
#define TXSTYPE_CNU 4	/* create new user (by bank) */
#define TXSTYPE_RNU 5	/* request new user (by user) */
#define TXSTYPE_ANU 6	/* accept new user (by bank) */
#define TXSTYPE_SEN 7	/* send MoonBlocks */
#define TXSTYPE_WIT 8	/* withdraw MoonBlocks */
#define TXSTYPE_INF 99  /* get info */

#define TXS_SEN_FEE(x) (0x1000+0.0001*(x)) /* minimum 1hour fee */
#define TIME_FEE(x) (x) /* seconds */
#define MIN_MASS (0x800000) /* minimum 97days fee */

#define BANK_PROFIT(x) ((x)>>3) /* 1/8 of fees */
#define BANK_USER_FEE(x) ((x)>>6) /* 1s per 64 accounts (bank makes 8s on these accounts) */
#define BANK_TIME_FEE(x) (0x40*(x)) /* must have minimum of 512 accounts to make profits */
#define BANK_MIN_MASS (0x20000000) /* minimum 97days fee */

#define MESSAGE_TOO_LONG 0x800000
#define MESSAGE_LEN_OK 0x10000
#define MESSAGE_WAIT 5

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
#pragma pack()

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
