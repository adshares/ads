#include "tokendb.h"

#include <string>
#include <iostream>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>

namespace Database
{

const char* const kHeaderFileName = "token_header.db";
const char* const kDBFileName = "token.db";

struct Token
{
    Token(uint32_t _userid, uint32_t _tokenid, uint64_t _balance, uint32_t _next)
        : user_id(_userid), token_id(_tokenid), balance(_balance), next(_next)
    {
    }
    uint32_t user_id;
    uint32_t token_id;
    uint64_t balance;
    uint32_t next;
}__attribute__((packed));

const int kTokenDataSize = sizeof(Token::user_id) + sizeof(Token::token_id) + sizeof(Token::balance);

TokenDB::TokenDB()
{
    if (!boost::filesystem::exists(kHeaderFileName))
    {
        // initial db files create
        std::ofstream file(kHeaderFileName, std::ofstream::out | std::ofstream::app);
        file.close();
        file.open(kDBFileName, std::ofstream::out | std::ofstream::app);
        file.close();
    }

    tokens_header.open(kHeaderFileName, std::ofstream::out | std::ifstream::in | std::fstream::binary | std::fstream::ate);
    token_db.open(kDBFileName, std::ofstream::out | std::ifstream::in | std::fstream::binary | std::fstream::ate);
}

TokenDB::~TokenDB()
{
    tokens_header.close();
    token_db.close();
}

bool TokenDB::create_token(uint32_t user_id, uint32_t token_id, uint64_t balance)
{
    Token token(user_id, token_id, balance, 0);
    uint32_t index = get_user_next_record_id(user_id);
    token_db.seekp(index);
    token_db.write((char*)&token, sizeof(Token));
    tokens_header.flush();
    token_db.flush();
    return true;
}

bool TokenDB::add_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_add)
{
    return true;
}

bool TokenDB::remove_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_remove)
{
    return true;
}

bool TokenDB::move_tokens(uint32_t src_user_id, uint32_t dest_user_id, uint32_t token_id, uint64_t value)
{
    return true;
}

bool TokenDB::remove_token_account(uint32_t user_id, uint32_t token_id)
{
    return true;
}

uint32_t TokenDB::get_user_next_record_id(uint32_t user_id)
{
    uint32_t next_free_record = get_next_free_token_index();
    struct stat header_stat = {};
    ::stat(kHeaderFileName, &header_stat);
    if (header_stat.st_size / sizeof(uint32_t) >= (user_id + 1)) // user exists
    {
        token_db.seekp(get_user_last_record_id(user_id) + kTokenDataSize, std::ios_base::beg);
        token_db.write((char*)&next_free_record, sizeof(uint32_t));
    } else // new user
    {
        tokens_header.seekp(user_id * sizeof(uint32_t), std::ios_base::beg);
        tokens_header.write((char*)&next_free_record, sizeof(uint32_t));
    }
    return next_free_record;
}

uint32_t TokenDB::get_user_last_record_id(uint32_t user_id)
{
    uint32_t db_size = get_next_free_token_index();
    uint32_t index, next_index;
    tokens_header.seekg(user_id * sizeof(uint32_t), std::ios_base::beg);
    tokens_header.read((char*)&index, sizeof(uint32_t));
    if (!index)
    {
        // no user yet
        return index;
    }

    next_index = index;
    while (next_index != 0)
    {
        index = next_index;
        token_db.seekg(next_index + kTokenDataSize, std::ios_base::beg);
        token_db.read((char*)&next_index, sizeof(uint32_t));
        assert(next_index < db_size);
    }
    return index;
}

uint32_t TokenDB::get_tokens_count()
{
    struct stat token_stat = {};
    ::stat(kDBFileName, &token_stat);
    return (token_stat.st_size/sizeof(Token));
}

uint32_t TokenDB::get_next_free_token_index()
{
    struct stat token_stat = {};
    ::stat(kDBFileName, &token_stat);
    return (token_stat.st_size);
}

}
