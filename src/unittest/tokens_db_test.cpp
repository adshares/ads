#include "gtest/gtest.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <fcntl.h>

#include "../common/database/tokendb.h"

const int kSizeOfToken = 20;

const char* kDatabaseName = "token.db";

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
    boost::filesystem::remove(kDatabaseName);
    boost::filesystem::remove("token_header.db");

    Database::TokenDB db;

    for (unsigned int i=0; i<5; ++i)
    {
        EXPECT_TRUE(db.create_token(token[i].user_id, token[i].token_id, token[i].balance));
    }

    EXPECT_TRUE(boost::filesystem::exists(kDatabaseName));

    for (unsigned int i=0; i<5; ++i)
    {
        EXPECT_TRUE(db.is_exists_token(token[i].user_id, token[i].token_id));
        EXPECT_EQ(db.get_balance(token[i].user_id, token[i].token_id), token[i].balance);
    }
}

TEST(TokensTest, add_tokens)
{
    Database::TokenDB db;
    EXPECT_TRUE(db.add_tokens(0, 2, 900));
    token[2].balance += 900;
    EXPECT_TRUE(db.add_tokens(2, 4, 990));
    token[3].balance += 990;

    EXPECT_EQ(db.get_balance(token[2].user_id, token[2].token_id), token[2].balance);
    EXPECT_EQ(db.get_balance(token[3].user_id, token[3].token_id), token[3].balance);
}

TEST(TokensTest, remove_tokens)

{
    Database::TokenDB db;
    EXPECT_TRUE(db.remove_tokens(0, 1, 900));
    token[0].balance -= 900;
    EXPECT_TRUE(db.remove_tokens(1, 3, 123456678));
    token[4].balance -= 123456678;

    EXPECT_EQ(db.get_balance(token[0].user_id, token[0].token_id), token[0].balance);
    EXPECT_EQ(db.get_balance(token[4].user_id, token[4].token_id), token[4].balance);
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
    Database::TokenDB db;

    EXPECT_TRUE(db.move_tokens(token[0].user_id, token[1].user_id, token[0].token_id, 55));
    token[0].balance -= 55;
    token[1].balance += 55;

    for (unsigned int i=0; i<2; ++i)
    {
        EXPECT_EQ(db.get_balance(token[i].user_id, token[i].token_id), token[i].balance);
    }
}

TEST(TokensTest, move_tokens_non_existing)
{
    Database::TokenDB db;
    EXPECT_FALSE(db.move_tokens(1, 10, 1, 100));
    EXPECT_FALSE(db.move_tokens(10, 1, 1, 10));

    EXPECT_FALSE(db.is_exists_token(10, 1));
}

TEST(TokensTest, move_tokens_oversize)
{
    Database::TokenDB db;
    EXPECT_FALSE(db.move_tokens(token[0].user_id, token[1].user_id, token[0].token_id, token[0].balance + 10));
}
