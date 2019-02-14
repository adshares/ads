#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include "signatures.h"
#include "helper/blockfilereader.h"

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
    Helper::BlockFileReader file(fileName);
    std::vector<Signature> signatures;

    if (file.isOpen()) {
        const int numSignatures = file.getSize() / sizeof(Signature);
        for(int i=0; i<numSignatures; ++i) {
            Signature sign;
            if (file.read((char*)&sign, sizeof(sign)) > 0) {
                signatures.push_back(sign);
            } else {
                break;
            }
        }
    }

    return signatures;
}

void Signatures::readSignaturesOk(uint32_t path) {
    char fileName[64];
    Helper::FileName::getName(fileName, sizeof(fileName), path, "signatures.ok");
    m_signaturesOk = readSignatures(fileName);
}

void Signatures::readSignaturesNo(uint32_t path) {
    char fileName[64];
    Helper::FileName::getName(fileName, sizeof(fileName), path, "signatures.no");
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
