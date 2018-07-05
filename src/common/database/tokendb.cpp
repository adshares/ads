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

struct Header
{
    Header(uint32_t _time) : timestamp(_time) {}
    uint32_t timestamp;
}__attribute__((packed));

struct Token
{
    Token() = default;
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
        Header header(time(NULL));
        file.write((char*)&header, sizeof(Header));
        file.close();
    }

    m_tokensHeader.exceptions (std::ifstream::eofbit | std::fstream::badbit);
    m_tokenDB.exceptions (std::ifstream::eofbit | std::fstream::badbit);

    m_tokensHeader.open(kHeaderFileName, std::ofstream::out | std::ifstream::in | std::fstream::binary | std::fstream::ate);
    m_tokenDB.open(kDBFileName, std::ofstream::out | std::ifstream::in | std::fstream::binary | std::fstream::ate);
}

TokenDB::~TokenDB()
{
    m_tokensHeader.close();
    try {
        m_tokenDB.seekp(0, std::ios_base::beg);
        Header header(time(NULL));
        m_tokenDB.write((char*)&header, sizeof(Header));
        m_tokenDB.close();
    } catch (std::exception)
    {}
}

bool TokenDB::create_token(uint32_t user_id, uint32_t token_id, uint64_t balance)
{
    Token token(user_id, token_id, balance, 0);
    try {
        uint32_t index = get_user_next_record_id(user_id);
        m_tokenDB.seekp(index);
        m_tokenDB.write((char*)&token, sizeof(Token));
    } catch (std::exception)
    {
        return false;
    }
    m_tokensHeader.flush();
    m_tokenDB.flush();
    return true;
}

bool TokenDB::add_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_add)
{
    return edit_tokens(EditType::ADD, user_id, token_id, value_to_add);
}

bool TokenDB::remove_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_remove)
{
    return edit_tokens(EditType::REMOVE, user_id, token_id, value_to_remove);
}

bool TokenDB::edit_tokens(EditType mod, uint32_t user_id, uint32_t token_id, uint64_t value)
{
    uint32_t index = get_user_token_record_id(user_id, token_id);
    if (!index) return false;
    Token token = {};
    try {
        m_tokenDB.seekg(index, std::ios_base::beg);
        m_tokenDB.read((char*)&token, sizeof(Token));
        if (mod == EditType::ADD)
        {
            token.balance += value;
        } else
        {
            if (value > token.balance) return false;
            token.balance -= value;
        }
        m_tokenDB.seekp(index, std::ios_base::beg);
        m_tokenDB.write((char*)&token, sizeof(Token));
    } catch (std::exception)
    {
        return false;
    }
    return true;
}

uint32_t TokenDB::get_user_token_record_id(uint32_t user_id, uint32_t token_id)
{
    try {
        uint32_t index, db_size = get_next_free_token_index();
        m_tokensHeader.seekg(user_id * sizeof(uint32_t), std::ios_base::beg);
        m_tokensHeader.read((char*)&index, sizeof(uint32_t));
        if (!index) return 0;

        while(index != 0)
        {
            Token token = {};
            m_tokenDB.seekg(index, std::ios_base::beg);
            m_tokenDB.read((char*)&token, sizeof(Token));
            if (token.user_id == user_id && token.token_id == token_id)
            {
                return index;
            }
            index = token.next;
            if (index >= db_size) break;
        }

    } catch (std::exception)
    {
        return 0;
    }
    return 0;
}

bool TokenDB::move_tokens(uint32_t src_user_id, uint32_t dest_user_id, uint32_t token_id, uint64_t value)
{
    int src_index = get_user_token_record_id(src_user_id, token_id);
    if (!src_index) return false;
    int dst_index = get_user_token_record_id(dest_user_id, token_id);
    if (!dst_index) return false;

    try {
        Token src_token = {}, dst_token = {};
        m_tokenDB.seekg(src_index, std::ios_base::beg);
        m_tokenDB.read((char*)&src_token, sizeof(Token));
        if (src_token.balance < value) return false;
        src_token.balance -= value;
        m_tokenDB.seekp(src_index, std::ios_base::beg);
        m_tokenDB.write((char*)&src_token, sizeof(Token));

        m_tokenDB.seekg(dst_index, std::ios_base::beg);
        m_tokenDB.read((char*)&dst_token, sizeof(Token));
        dst_token.balance += value;
        m_tokenDB.seekp(dst_index, std::ios_base::beg);
        m_tokenDB.write((char*)&dst_token, sizeof(Token));
    } catch (std::exception)
    {
        return false;
    }
    return true;
}

bool TokenDB::remove_token_account(uint32_t, uint32_t )
{
    // not sure how to do that :/
    return true;
}

uint32_t TokenDB::get_user_next_record_id(uint32_t user_id)
{
    uint32_t next_free_record = get_next_free_token_index();
    if (get_users_count() >= user_id + 1) // user exists
    {
        m_tokenDB.seekp(get_user_last_record_id(user_id) + kTokenDataSize, std::ios_base::beg);
        m_tokenDB.write((char*)&next_free_record, sizeof(uint32_t));
    } else // new user
    {
        m_tokensHeader.seekp(user_id * sizeof(uint32_t), std::ios_base::beg);
        m_tokensHeader.write((char*)&next_free_record, sizeof(uint32_t));
    }
    return next_free_record;
}

//TODO: maybe search from back? file with footer similar to header?
uint32_t TokenDB::get_user_last_record_id(uint32_t user_id)
{
    uint32_t db_size = get_next_free_token_index();
    uint32_t index, next_index;
    m_tokensHeader.seekg(user_id * sizeof(uint32_t), std::ios_base::beg);
    m_tokensHeader.read((char*)&index, sizeof(uint32_t));
    if (!index)
    {
        // no user yet
        return index;
    }

    next_index = index;
    while (next_index != 0)
    {
        index = next_index;
        m_tokenDB.seekg(next_index + kTokenDataSize, std::ios_base::beg);
        m_tokenDB.read((char*)&next_index, sizeof(uint32_t));
        assert(next_index < db_size);
    }
    return index;
}

uint32_t TokenDB::get_users_count()
{
    struct stat header_stat = {};
    ::stat(kHeaderFileName, &header_stat);
    return (header_stat.st_size / sizeof(uint32_t));
}

uint32_t TokenDB::get_tokens_count()
{
    struct stat token_stat = {};
    ::stat(kDBFileName, &token_stat);
    return ((token_stat.st_size - sizeof(Header))/sizeof(Token));
}

uint32_t TokenDB::get_next_free_token_index()
{
    struct stat token_stat = {};
    ::stat(kDBFileName, &token_stat);
    return (token_stat.st_size);
}

bool TokenDB::is_exists_token(uint32_t user_id, uint32_t token_id)
{
    if (user_id >= get_users_count()) return false;

    try {
        uint32_t db_size = get_next_free_token_index();
        uint32_t index;
        m_tokensHeader.seekg(user_id * sizeof(uint32_t), std::ios_base::beg);
        m_tokensHeader.read((char*)&index, sizeof(uint32_t));

        if (!index)
        {
            return false;
        }

        while (index != 0)
        {
            Token token = {};
            m_tokenDB.seekg(index, std::ios_base::beg);
            m_tokenDB.read((char*)&token, sizeof(Token));
            if (token.user_id == user_id && token.token_id == token_id)
            {
                return true;
            }
            index = token.next;

            if (index >= db_size) return false;
        }
    } catch (std::exception)
    {
        return false;
    }

    return false;
}

uint64_t TokenDB::get_balance(uint32_t user_id, uint32_t token_id)
{
    uint32_t index = get_user_token_record_id(user_id, token_id);
    if (!index) return 0;
    m_tokenDB.seekg(index, std::ios_base::beg);
    Token token = {};
    m_tokenDB.read((char*)&token, sizeof(Token));
    return token.balance;
}

#ifdef DEBUG

#include <stdio.h>
void TokenDB::dump()
{
    uint32_t header_size = get_users_count();
    std::cout<< "****** DB DUMP ******" << std::endl;
    std::cout<<kHeaderFileName<<std::endl;
    uint32_t val;
    m_tokensHeader.seekg(0, std::ios_base::beg);
    for (unsigned int i=0; i< header_size; ++i)
    {
        m_tokensHeader.read((char*)&val, sizeof(uint32_t));
        std::cout<< val << " ";
    }
    std::cout<<std::endl;

    std::cout<<kDBFileName<<std::endl;
    m_tokenDB.seekg(0, std::ios_base::beg);
    m_tokenDB.read((char*)&val, sizeof(uint32_t));
    std::cout<<"Timestamp: "<<val<<std::endl;

    uint32_t tokens_size = get_tokens_count();
    for (unsigned int i=0; i<tokens_size; ++i)
    {
        Token token;
        m_tokenDB.read((char*)&token, sizeof(Token));
        printf("%04X %04X %ld %d\n", token.user_id, token.token_id, token.balance, token.next);
    }
    std::cout<< "****** DB DUMP END ***" << std::endl;
}
#endif /*DEBUG*/

}
