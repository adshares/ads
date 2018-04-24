#ifndef SERVERS_H
#define SERVERS_H

#include <stdio.h>
#include <stdint.h>
#include <openssl/sha.h>
#include <string>
#include <vector>
#include "default.hpp"

namespace Helper {

struct ServersHeader {
    uint32_t version{0};
    uint32_t ttime{0};
    uint32_t messageCount{0};
    uint32_t nodesCount{0};
    uint32_t dividendBalance{0};
    uint8_t oldHash[SHA256_DIGEST_LENGTH];
    uint8_t minHash[SHA256_DIGEST_LENGTH];
    uint8_t msgHash[SHA256_DIGEST_LENGTH];
    uint8_t nodHash[SHA256_DIGEST_LENGTH];
    uint8_t vipHash[SHA256_DIGEST_LENGTH];
    uint8_t nowHash[SHA256_DIGEST_LENGTH];
    uint16_t voteYes{0};
    uint16_t voteNo{0};
    uint16_t voteTotal{0};
}__attribute__((packed));

struct ServersNode {
    unsigned char publicKey[32];
    uint64_t hash[4];
    uint8_t messageHash[SHA256_DIGEST_LENGTH];
    uint32_t messageId{0};
    uint32_t messageTime{0};
    int64_t weight{0};
    uint32_t status{0};
    uint32_t accountCount{0};
    uint32_t port{0};
    uint32_t ipv4{0};
}__attribute__((packed));

struct HashSingleVariant {
    hash_s ha{};
    uint16_t svid{0};
    uint32_t msid{0};
}__attribute__((packed));

struct HashDoubleVariant {
    uint16_t svid1{0};
    uint32_t msid1{0};
    hash_s ha1{};
    uint16_t svid2{0};
    uint32_t msid2{0};
    hash_s ha2{};
}__attribute__((packed));

class Servers {

public:
    Servers();
    Servers(const char* filePath);

    void load(const char* filePath = nullptr);
    bool loadHeader();
    bool loadNowhash();

    ServersHeader getHeader();
    std::vector<ServersNode> getNodes();
    ServersNode getNode(unsigned int nodeId);
    unsigned int getNodesCount();
    uint8_t* getNowHash();

    void setNow(uint32_t time);

    bool getMsglHashTree(uint16_t svid,uint32_t msid,uint32_t mnum,std::vector<hash_s>& hashes);

private:
    ServersHeader m_header;
    std::vector<ServersNode> m_nodes;
    std::string m_filePath;
};

}

#endif // SERVERS_H
