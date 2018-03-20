#include "hash.h"
#include <stdio.h>
#include <stdint.h>
#include <sstream>


namespace Helper {

uint16_t crc16(const uint8_t* data_p, uint8_t length) {
    uint8_t x;
    uint16_t crc = 0x1D0F; //differet initial checksum !!!

    while(length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}

uint16_t crc_acnt(uint16_t node, uint32_t user) {
    uint8_t data[6];
    uint8_t* bankp=(uint8_t*)&node;
    uint8_t* userp=(uint8_t*)&user;
    //change endian
    data[0]=bankp[1];
    data[1]=bankp[0];
    data[2]=userp[3];
    data[3]=userp[2];
    data[4]=userp[1];
    data[5]=userp[0];
    return(crc16(data,6));
}

void create256signhash(const unsigned char* signature, int signSize, std::array<uint8_t, SHA256_DIGEST_LENGTH> ha, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout) {
    create256signhash(signature, signSize, ha.data(), hashout);
}

void create256signhash(const unsigned char* signature, int signatureSize, const unsigned char* previosHash, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout) {
    create256signhash(signature, signatureSize, previosHash, hashout.data());
}

void create256signhash(const unsigned char* signature, int signSize, const unsigned char* previosHash, unsigned char hashout[SHA256_DIGEST_LENGTH]) {
    std::array<uint8_t, SHA256_DIGEST_LENGTH> temphash;

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

void create256signhash(const std::array<uint8_t, SHA256_DIGEST_LENGTH> signature, const std::array<uint8_t, SHA256_DIGEST_LENGTH> ha, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout) {
    return create256signhash(signature.data(), signature.size(), ha.data(), hashout);
}

}

