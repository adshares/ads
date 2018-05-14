#ifndef JSON_H
#define JSON_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include "default.hpp"

class settings;

namespace Helper {

namespace TAG {
const char* const TYPE = "type";
const char* const SRC_NODE = "node";
const char* const SRC_USER = "user";
const char* const SRC_ADDRESS = "sender_address";
const char* const MSGID = "msg_id";
const char* const NODE_MSGID = "node_msg_id";
const char* const TIME = "time";
const char* const DST_NODE = "destination_node";
const char* const DST_USER = "destination_user";
const char* const DST_ADDRESS = "target_address";
const char* const MSG = "message";
const char* const MSG_LEN = "message_length";
const char* const OLD_PKEY = "old_public_key";
const char* const PKEY = "public_key";
const char* const NEW_PKEY = "new_public_key";
const char* const PKEY_SIGN = "public_key_signature";
const char* const BLOCK = "block_number";
const char* const FROM = "from";
const char* const TXN_COUNTER = "transactions_conuter";
const char* const SENDER_FEE = "sender_fee";
const char* const AMOUNT = "amount";
const char* const DEDUCT = "deduct";
const char* const POSITION = "position";
const char* const SIGN = "signature";
const char* const PORT = "port";
const char* const IP_ADDRESS = "ip_address";
const char* const STATUS = "status";
}

void print_user(user_t& u, boost::property_tree::ptree& pt, bool local, uint32_t bank, uint32_t user);
bool parse_amount(int64_t& amount,std::string str_amount);
char* print_amount(int64_t amount);
const std::string print_address(uint16_t node, uint32_t user, int32_t _suffix = -1);
const std::string print_msg_id(uint16_t node, uint32_t user, int32_t _suffix = -1);
char* mydate(uint32_t now);
int check_csum(user_t& u,uint16_t peer,uint32_t uid);
void printErrorJson(const char* errorMsg);
void print_log(boost::property_tree::ptree& pt, uint16_t bank, uint32_t user, uint32_t lastlog, int txnType);
void save_log(log_t* log, int len, uint32_t from, uint16_t bank, uint32_t user);
int getLogTxnTypeId(const char* txnName);
}

#endif // JSON_H
