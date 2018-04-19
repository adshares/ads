#ifndef JSON_H
#define JSON_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include "default.hpp"

class settings;

namespace Helper {

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
