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
const char* const TYPE = "txn.type";
const char* const SRC_NODE = "txn.node";
const char* const SRC_USER = "txn.user";
const char* const MSGID = "txn.msg_id";
const char* const NODE_MSGID = "txn.node_msg_id";
const char* const TIME = "txn.time";
const char* const DST_NODE = "txn.destination_node";
const char* const DST_USER = "txn.destination_user";
const char* const MSG = "txn.message";
const char* const MSG_LEN = "txn.message_length";
const char* const OLD_PKEY = "txn.old_public_key";
const char* const PKEY = "txn.public_key";
const char* const NEW_PKEY = "txn.new_public_key";
const char* const PKEY_SIGN = "txn.public_key_signature";
const char* const BLOCK = "txn.block_number";
const char* const FROM = "txn.from";
const char* const TXN_COUNTER = "txn.transactions_conuter";
const char* const AMOUNT = "txn.amount";
const char* const DEDUCT = "txn.deduct";
const char* const SIGN = "txn.signature";
}

void print_user(user_t& u, boost::property_tree::ptree& pt, bool local, uint32_t bank, uint32_t user);
bool parse_amount(int64_t& amount,std::string str_amount);
char* print_amount(int64_t amount);
char* mydate(uint32_t now);
int check_csum(user_t& u,uint16_t peer,uint32_t uid);
void printErrorJson(const char* errorMsg);
void print_log(boost::property_tree::ptree& pt, uint16_t bank, uint32_t user, uint32_t lastlog, int txnType);
void save_log(log_t* log, int len, uint32_t from, uint16_t bank, uint32_t user);
int getLogTxnTypeId(const char* txnName);
}

#endif // JSON_H
