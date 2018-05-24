#ifndef SIGNATURES_H
#define SIGNATURES_H

#include <stdint.h>
#include <openssl/sha.h>
#include <vector>

namespace Helper {

struct Signature {
    uint16_t svid{0};
    uint8_t signature[2*SHA256_DIGEST_LENGTH];
}__attribute__((packed));

class Signatures {
public:
    Signatures(uint32_t path);
    void load();
    std::vector<Signature> getSignaturesOk();
    std::vector<Signature> getSignaturesNo();
private:
    int getNumOfSignaturesInFile(const char* fileName) const;
    std::vector<Signature> readSignatures(const char* fileName);
    uint32_t m_path;
    std::vector<Signature> m_signaturesOk;
    std::vector<Signature> m_signaturesNo;
};

}

#endif
