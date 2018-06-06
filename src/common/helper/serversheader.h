#ifndef SERVERSHEADERS_H
#define SERVERSHEADERS_H

#include <stdint.h>
#include <openssl/sha.h>
#include <boost/property_tree/ptree.hpp>

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

    void toJson(boost::property_tree::ptree& node);
}__attribute__((packed));

}

#endif
