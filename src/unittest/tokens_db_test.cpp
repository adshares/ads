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


TEST(TokensTest, create_token_account)
{
    boost::filesystem::remove("token.db");
    boost::filesystem::remove("token_header.db");

    TokenTst token[5];
    token[0] = {0, 1, 1000, 40};
    token[1] = {1, 1, 200, 80};
    token[2] = {0, 2, 100, 0};
    token[3] = {2, 4, 10, 0};
    token[4] = {1, 3, 1234567890123456678, 0};

    uint32_t expected_header_jump[3] = {0, 20, 60};

    Database::TokenDB db;
    for (unsigned int i=0; i<5; ++i)
    {
        db.create_token(token[i].user_id, token[i].token_id, token[i].balance);
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
    EXPECT_EQ(st.st_size, sizeof(token));

    file.open("token.db", std::ifstream::in | std::ifstream::binary);
    if(file.is_open())
    {
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
