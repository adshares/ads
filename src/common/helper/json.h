#ifndef JSON_H
#define JSON_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include "default.hpp"

class settings;

namespace Helper{

void print_user(user_t& u, boost::property_tree::ptree& pt, bool local,uint32_t bank, uint32_t user, settings& sts);
bool parse_amount(int64_t& amount,std::string str_amount);
char* print_amount(int64_t amount);
char* mydate(uint32_t now);
int check_csum(user_t& u,uint16_t peer,uint32_t uid);

}

#endif // JSON_H
