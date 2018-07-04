#include "gtest/gtest.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <fcntl.h>

#include "../common/database/tokendb.h"

const int kSizeOfToken = 20;

//copy from tokendb.cpp
struct TokenTst
{
    TokenTst(){

    }
    TokenTst(uint32_t _userid, uint32_t _tokenid, uint64_t _balance, uint32_t _next)
        : user_id(_userid), token_id(_tokenid), balance(_balance), next(_next)
    {
    }
    uint32_t user_id;
    uint32_t token_id;
    uint64_t balance;
    uint32_t next;
}__attribute__((packed));

TokenTst token[5]{ {0, 1, 1000, 44},
                   {1, 1, 200, 84},
                   {0, 2, 100, 0},
                   {2, 4, 10, 0},
                   {1, 3, 1234567890123456678, 0} };

TEST(TokensTest, create_token_account)
{
    boost::filesystem::remove("token.db");
    boost::filesystem::remove("token_header.db");
    uint32_t expected_header_jump[3] = {4, 24, 64};

    Database::TokenDB db;

    for (unsigned int i=0; i<5; ++i)
    {
        EXPECT_TRUE(db.create_token(token[i].user_id, token[i].token_id, token[i].balance));
    }

    struct stat st = {};
    ::stat("token_header.db", &st);
    EXPECT_EQ(st.st_size, 3*sizeof(uint32_t));

    std::ifstream file("token_header.db", std::ifstream::in | std::ifstream::binary);
    if(file.is_open())
    {
        for (unsigned int i=0; i<3; ++i)
        {
            int val = -1;
            file.read((char*)&val, sizeof(uint32_t));
            EXPECT_EQ(val, expected_header_jump[i]);
        }
    }
    file.close();

    st = {};
    ::stat("token.db", &st);
    EXPECT_EQ(st.st_size, sizeof(token) + 4); // size of header

    file.open("token.db", std::ifstream::in | std::ifstream::binary);
    if(file.is_open())
    {
        uint32_t timestamp;
        file.read((char*)&timestamp, sizeof(uint32_t));
        for (unsigned int i=0; i<5; ++i)
        {
            TokenTst tok = {};
            file.read((char*)&tok, sizeof(TokenTst));
            EXPECT_EQ(token[i].user_id, tok.user_id);
            EXPECT_EQ(token[i].token_id,tok.token_id);
            EXPECT_EQ(token[i].balance, tok.balance);
            EXPECT_EQ(token[i].next,    tok.next);
        }

    }
    file.close();
}

TEST(TokensTest, add_tokens)
{
    {
        Database::TokenDB db;
        EXPECT_TRUE(db.add_tokens(0, 2, 900));
        token[2].balance += 900;
        EXPECT_TRUE(db.add_tokens(2, 4, 990));
        token[3].balance += 990;
    }

    std::ifstream file("token.db", std::ifstream::in | std::ifstream::binary);
    if(file.is_open())
    {
        uint32_t timestamp;
        file.read((char*)&timestamp, sizeof(uint32_t));
        for (unsigned int i=0; i<5; ++i)
        {
            TokenTst tok = {};
            file.read((char*)&tok, sizeof(TokenTst));
            EXPECT_EQ(token[i].user_id, tok.user_id);
            EXPECT_EQ(token[i].token_id,tok.token_id);
            EXPECT_EQ(token[i].balance, tok.balance);
            EXPECT_EQ(token[i].next,    tok.next);
        }
    }
    file.close();
}

TEST(TokensTest, remove_tokens)

{
    {
        Database::TokenDB db;
        EXPECT_TRUE(db.remove_tokens(0, 1, 900));
        token[0].balance -= 900;
        EXPECT_TRUE(db.remove_tokens(1, 3, 123456678));
        token[4].balance -= 123456678;
    }

    std::ifstream file("token.db", std::ifstream::in | std::ifstream::binary);
    if(file.is_open())
    {
        uint32_t timestamp;
        file.read((char*)&timestamp, sizeof(uint32_t));
        for (unsigned int i=0; i<5; ++i)
        {
            TokenTst tok = {};
            file.read((char*)&tok, sizeof(TokenTst));
            EXPECT_EQ(token[i].user_id, tok.user_id);
            EXPECT_EQ(token[i].token_id,tok.token_id);
            EXPECT_EQ(token[i].balance, tok.balance);
            EXPECT_EQ(token[i].next,    tok.next);
        }
    }
    file.close();
}

TEST(TokensTest, tokens_for_non_existing)
{
    {
        Database::TokenDB db;
        EXPECT_FALSE(db.add_tokens(10, 10, 1000));
        EXPECT_FALSE(db.add_tokens(0, 3, 100));
    }

    {
        Database::TokenDB db;
        EXPECT_FALSE(db.remove_tokens(10, 10, 10));
        EXPECT_FALSE(db.remove_tokens(3, 1, 10));
    }
}

TEST(TokensTest, move_tokens)
{
    {
        Database::TokenDB db;

        EXPECT_TRUE(db.move_tokens(token[0].user_id, token[1].user_id, token[0].token_id, 55));
        token[0].balance -= 55;
        token[1].balance += 55;
    }

    std::ifstream file("token.db", std::ifstream::in | std::ifstream::binary);
    if(file.is_open())
    {
        uint32_t timestamp;
        file.read((char*)&timestamp, sizeof(uint32_t));
        for (unsigned int i=0; i<2; ++i)
        {
            TokenTst tok = {};
            file.read((char*)&tok, sizeof(TokenTst));
            EXPECT_EQ(token[i].user_id, tok.user_id);
            EXPECT_EQ(token[i].token_id,tok.token_id);
            EXPECT_EQ(token[i].balance, tok.balance);
            EXPECT_EQ(token[i].next,    tok.next);
        }
    }
    file.close();
}

TEST(TokensTest, move_tokens_non_existing)
{
    Database::TokenDB db;
    EXPECT_FALSE(db.move_tokens(1, 10, 1, 100));
    EXPECT_FALSE(db.move_tokens(10, 1, 1, 10));
}

TEST(TokensTest, move_tokens_oversize)
{
    Database::TokenDB db;
    EXPECT_FALSE(db.move_tokens(token[0].user_id, token[1].user_id, token[0].token_id, token[0].balance + 10));
}
