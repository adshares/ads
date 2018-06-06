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
    Signatures();
    void load(uint32_t path);
    void loadSignaturesOk(uint32_t path);
    const std::vector<Signature>& getSignaturesOk();
    const std::vector<Signature>& getSignaturesNo();
private:
    int getNumOfSignaturesInFile(const char* fileName) const;
    std::vector<Signature> readSignatures(const char* fileName);
    void readSignaturesOk(uint32_t path);
    void readSignaturesNo(uint32_t path);
    std::vector<Signature> m_signaturesOk;
    std::vector<Signature> m_signaturesNo;
};

}

#endif
