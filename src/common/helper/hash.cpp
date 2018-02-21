#include "hash.h"
#include <stdio.h>
#include <stdint.h>
#include <sstream>


namespace Helper{

void create256signhash(const unsigned char* signature, int signSize, std::array<uint8_t, SHA256_DIGEST_LENGTH> ha, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout)
{
    create256signhash(signature, signSize, ha.data(), hashout);
}

void create256signhash(const unsigned char* signature, int signatureSize, const unsigned char* previosHash, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout)
{
/*    std::array<uint8_t, SHA256_DIGEST_LENGTH> temphash{0};

    SHA256_CTX sha256;

    SHA256(signature, signatureSize, temphash.data());
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, previosHash, SHA256_DIGEST_LENGTH);
    SHA256_Update(&sha256, temphash.data(), temphash.size());
    SHA256_Final(hashout.data(), &sha256);*/

    create256signhash(signature, signatureSize, previosHash, hashout.data());
}

void create256signhash(const unsigned char* signature, int signSize, const unsigned char* previosHash, unsigned char hashout[SHA256_DIGEST_LENGTH])
{
    std::array<uint8_t, SHA256_DIGEST_LENGTH> temphash{0};

    SHA256_CTX sha256;

    /*SHA256_Init(&sha256);
    SHA256_Update(&sha256, signature, signSize);
    SHA256_Final(hashout.data(), &sha256);*/
    SHA256(signature, signSize, temphash.data());
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, previosHash, SHA256_DIGEST_LENGTH);
    SHA256_Update(&sha256, temphash.data(), temphash.size());
    SHA256_Final(hashout, &sha256);
}

void create256signhash(const std::array<uint8_t, SHA256_DIGEST_LENGTH> signature, const std::array<uint8_t, SHA256_DIGEST_LENGTH> ha, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout)
{
    return create256signhash(signature.data(), signature.size(), ha.data(), hashout);
}

}

