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

#define SERVER_TYPE 1
#define SERVER_PORT "9090"
//#define PATHSHIFT 8
#define PATHSHIFT 5

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

#include "openssl/sha.h"
#include "ed25519/ed25519.h"
#include "options.hpp"
#include "servers.hpp"
#include "message.hpp"
#include "candidate.hpp"
#include "server.hpp"
#include "peer.hpp"

#endif
