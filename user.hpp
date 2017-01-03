#ifndef USER_HPP
#define USER_HPP

#include <algorithm>
#include <atomic>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
//#include <boost/archive/text_iarchive.hpp>
//#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
//#include <boost/bind.hpp>
//#include <boost/date_time/posix_time/posix_time.hpp>
//#include <boost/enable_shared_from_this.hpp>
//#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
//#include <boost/serialization/list.hpp>
//#include <boost/serialization/vector.hpp>
//#include <boost/shared_ptr.hpp>
//#include <boost/thread/thread.hpp>
//#include <boost/container/flat_set.hpp>
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

#define TXSTYPE_PER 1	/* peer connected */
#define TXSTYPE_BRO 2	/* broadcast */
#define TXSTYPE_CNP 3	/* create new peer */
#define TXSTYPE_CNU 4	/* create new user (by bank) */
#define TXSTYPE_RNU 5	/* request new user (by user) */
#define TXSTYPE_ANU 6	/* accept new user (by bank) */
#define TXSTYPE_SEN 7	/* send MoonBlocks */
#define TXSTYPE_WIT 8	/* withdraw MoonBlocks */
#define TXSTYPE_INF 99	/* get info */

#pragma pack(1)
typedef struct user_s { // 8+32+32+4+4+8+4+2+2=96 bytes
	uint32_t id; // id of last transaction, id==1 is the creation.
	uint32_t block; // last txs block time [to find the transaction]
	uint64_t weight; // balance
	uint8_t pkey[SHA256_DIGEST_LENGTH]; //public key
	uint8_t hash[SHA256_DIGEST_LENGTH]; //users block hash
	uint64_t withdraw; //amount to withdraw to target
	uint32_t user; // target user
	uint16_t node; // target node
	uint16_t status; // includes status and account type
} user_t;
#pragma pack()

#include "ed25519/ed25519.h"
#include "settings.hpp"

#endif
