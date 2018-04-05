#ifndef SERVERS_H
#define SERVERS_H

#include <stdio.h>
#include <stdint.h>
#include <openssl/sha.h>
#include <string>
#include <vector>

namespace Helper {

struct ServersHeader {
    uint32_t version;
    uint32_t ttime;
    uint32_t messageCount;
    uint32_t nodesCount;
    uint32_t dividendBalance;
    uint8_t oldHash[SHA256_DIGEST_LENGTH];
    uint8_t minHash[SHA256_DIGEST_LENGTH];
    uint8_t msgHash[SHA256_DIGEST_LENGTH];
    uint8_t nodHash[SHA256_DIGEST_LENGTH];
    uint8_t vipHash[SHA256_DIGEST_LENGTH];
    uint8_t nowHash[SHA256_DIGEST_LENGTH];
    uint16_t voteYes;
    uint16_t voteNo;
    uint16_t voteTotal;
}__attribute__((packed));

struct ServersNode {
    unsigned char publicKey[32];
    uint64_t hash[4];
    uint8_t messageHash[SHA256_DIGEST_LENGTH];
    uint32_t messageId;
    uint32_t messageTime;
    int64_t weight;
    uint32_t status;
    uint32_t accountCount;
    uint32_t port;
    uint32_t ipv4;
}__attribute__((packed));

class Servers {

public:
    Servers();
    Servers(const char* filePath);

    void load(const char* filePath = nullptr);

    ServersHeader getHeader();
    std::vector<ServersNode> getNodes();
    ServersNode getNode(unsigned int nodeId);
    unsigned int getNodesCount();

private:
    ServersHeader m_header;
    std::vector<ServersNode> m_nodes;
    std::string m_filePath;
};

}

#endif // SERVERS_H
