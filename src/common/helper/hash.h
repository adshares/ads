#ifndef HASH_H
#define HASH_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>
#include <array>
#include <openssl/sha.h>

namespace Helper {

uint16_t crc_acnt(uint16_t node, uint32_t user);

void create256signhash(const unsigned char* signature, int signSize, std::array<uint8_t, SHA256_DIGEST_LENGTH> ha, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout);

void create256signhash(const unsigned char* signature, int signSize, const unsigned char* previosHash, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout);

void create256signhash(const unsigned char* signature, int signSize, const unsigned char* previosHash, unsigned char outHash[SHA256_DIGEST_LENGTH]);

void create256signhash(std::array<uint8_t, SHA256_DIGEST_LENGTH> signature, std::array<uint8_t, SHA256_DIGEST_LENGTH> ha, std::array<uint8_t, SHA256_DIGEST_LENGTH>& hashout);

}

#endif // HASH_H
