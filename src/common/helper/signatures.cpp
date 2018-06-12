#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include "signatures.h"

#include "helper/blocks.h"

extern boost::mutex siglock;

namespace Helper {

Signatures::Signatures() {
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

void Signatures::readSignaturesOk(uint32_t path) {
    char fileName[64];
    Helper::FileName::getName(fileName, path, "signatures.ok");
    Helper::get_file_from_block(fileName);
    m_signaturesOk = readSignatures(fileName);
}

void Signatures::readSignaturesNo(uint32_t path) {
    char fileName[64];
    Helper::FileName::getName(fileName, path, "signatures.no");
    Helper::get_file_from_block(fileName);
    m_signaturesNo = readSignatures(fileName);
}

void Signatures::load(uint32_t path) {
    boost::lock_guard<boost::mutex> lock(siglock);
    readSignaturesOk(path);
    if (m_signaturesOk.size() == 0) {
        return;
    }
    readSignaturesNo(path);
}

void Signatures::loadSignaturesOk(uint32_t path) {
    boost::lock_guard<boost::mutex> lock(siglock);
    readSignaturesOk(path);
}

const std::vector<Signature>& Signatures::getSignaturesOk() {
    return m_signaturesOk;
}

const std::vector<Signature>& Signatures::getSignaturesNo() {
    return m_signaturesNo;
}

}
