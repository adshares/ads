#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include "signatures.h"

extern boost::mutex siglock;

namespace Helper {

Signatures::Signatures(uint32_t path) : m_path(path) {
}

int Signatures::getNumOfSignaturesInFile(const char* fileName) const {
    struct stat sstat;
    stat(fileName, &sstat);
    const auto fileSize = sstat.st_size;
    return fileSize/sizeof(Signature);
}

std::vector<Signature> Signatures::readSignatures(const char* fileName) {
    std::ifstream file(fileName, std::ifstream::in | std::ifstream::binary);

    std::vector<Signature> signatures;

    if (file.is_open()) {
        const int numSignatures = getNumOfSignaturesInFile(fileName);
        for(int i=0; i<numSignatures; ++i) {
            Signature sign;
            file.read((char*)&sign, sizeof(sign));
            signatures.push_back(sign);
        }
        file.close();
    }

    return signatures;
}

void Signatures::load() {
    boost::lock_guard<boost::mutex> lock(siglock);

    char fileName[64];
    sprintf(fileName,"blk/%03X/%05X/signatures.ok", m_path>>20, m_path&0xFFFFF);
    m_signaturesOk = readSignatures(fileName);

    if (m_signaturesOk.size() == 0) {
        return;
    }

    sprintf(fileName,"blk/%03X/%05X/signatures.no", m_path>>20, m_path&0xFFFFF);
    m_signaturesNo = readSignatures(fileName);
}

std::vector<Signature> Signatures::getSignaturesOk() {
    return m_signaturesOk;
}

std::vector<Signature> Signatures::getSignaturesNo() {
    return m_signaturesNo;
}

}
