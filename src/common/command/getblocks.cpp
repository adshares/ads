#include "getblocks.h"
#include "helper/hash.h"
#include "helper/json.h"
#include "default.hpp"
#include "hash.hpp"
#include "helper/vipkeys.h"
#include "helper/signatures.h"
#include "helper/block.h"
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

GetBlocks::GetBlocks()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetBlocks::GetBlocks(uint16_t abank, uint32_t auser, uint32_t ttime, uint32_t from, uint32_t to)
    : m_data{abank, auser, ttime, from, to} {
    m_responseError = ErrorCodes::Code::eNone;
}

int GetBlocks::getType() {
    return TXSTYPE_BLK;
}

CommandType GetBlocks::getCommandType() {
    return CommandType::eReadingOnly;
}

unsigned char* GetBlocks::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetBlocks::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetBlocks::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetBlocks::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetBlocks::getDataSize() {
    return sizeof(m_data.info);
}

int GetBlocks::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetBlocks::getSignature() {
    return m_data.sign;
}

int GetBlocks::getSignatureSize() {
    return sizeof(m_data.sign);
}

void GetBlocks::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetBlocks::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

uint32_t GetBlocks::getTime() {
    return m_data.info.ttime;
}

uint32_t GetBlocks::getUserId() {
    return m_data.info.auser;
}

uint32_t GetBlocks::getBankId() {
    return m_data.info.abank;
}

int64_t GetBlocks::getFee() {
    return 0;
}

int64_t GetBlocks::getDeduct() {
    return 0;
}

uint32_t GetBlocks::getUserMessageId() {
    return 0;
}

void GetBlocks::writeStart(uint32_t time) {
    std::ofstream file("blk/start.txt", std::ofstream::out);
    if(file.is_open()) {
        file.width(8);
        file.fill('0');
        file << std::hex << std::uppercase << time;
        file.close();
    }
    else {
        throw std::runtime_error("FATAL ERROR: failed to write to blk/start.txt");
    }
}

bool GetBlocks::saveNowhash(const header_t& head) {
    char filename[64];
    sprintf(filename, "blk/%03X.now", head.now>>20);
    int fd=open(filename, O_WRONLY|O_CREAT, 0644);
    if(fd<0) {
        DLOG("ERROR writing to %s\n", filename);
        return false;
    }
    lseek(fd, ((head.now&0xFFFFF)/BLOCKSEC)*32, SEEK_SET);
    if(write(fd, head.nowhash, 32)!=32) {
        close(fd);
        return false;
    }
    close(fd);
    return true;
}

bool GetBlocks::receiveHeaders(INetworkClient& netClient) {
    if(!netClient.readData((int32_t*)&m_numOfBlocks, sizeof(m_numOfBlocks))) {
        ELOG("GetBlocks ERROR reading number of blocks\n");
        return false;
    }

    if(m_numOfBlocks==0) {
        return true;
    }

    m_receivedHeaders.reserve(m_numOfBlocks);
    for(uint32_t n=0; n<m_numOfBlocks; ++n) {
        header_t sh;
        if(!netClient.readData((char*)&sh, sizeof(sh))) {
            return false;
        }
        m_receivedHeaders.push_back(sh);
    }

    if(!netClient.readData((char*)&m_newviphash, sizeof(m_newviphash))) {
        ELOG("GetBlocks ERROR reading newviphash flag\n");
        return false;
    }

    return true;
}

bool GetBlocks::receiveSignatures(INetworkClient& netClient) {
    uint32_t numOfSignatures{};
    if(!netClient.readData((int32_t*)&numOfSignatures, sizeof(numOfSignatures))) {
        ELOG("GetBlocks ERROR reading number of signatures\n");
        return false;
    }
    m_signatures.reserve(numOfSignatures);
    for(uint32_t i=0; i<numOfSignatures; ++i) {
        Signature sign;
        if (!netClient.readData((char*)&sign, sizeof(sign))) {
            return false;
        }
        m_signatures.push_back(sign);
    }
    return true;
}

bool GetBlocks::receiveFirstVipKeys(INetworkClient& netClient) {
    mkdir("blk",0755);
    mkdir("vip",0755);
    mkdir("txs",0755);

    if(!netClient.readData((int32_t*)&m_firstKeysLen, sizeof(m_firstKeysLen))) {
        ELOG("GetBlocks ERROR reading length of first vip keys error\n");
        return false;
    }

    if(m_firstKeysLen<=0) {
        ELOG("GetBlocks ERROR failed to read VIP keys for first hash\n");
        return false;
    } else {
        fprintf(stderr,"GetBlocks READ VIP keys for first hash (len:%d)\n", m_firstKeysLen);
    }

    auto buffer = std::make_unique<char[]>(m_firstKeysLen);
    if(!netClient.readData(buffer.get(), m_firstKeysLen)) {
        ELOG("GetBlocks ERROR reading length of first vip keys error\n");
        return false;
    }
    m_firstVipKeys.loadFromBuffer(std::move(buffer), m_firstKeysLen);
    return true;
}

bool GetBlocks::loadLastHeader() {
    if(m_firstVipKeys.getVipKeys() != nullptr) {
        m_block.getData().ttime = 0;
        m_block.readDataFromHeaderFile();

        uint32_t laststart = m_block.getData().ttime;
        memcpy(m_viphash, m_receivedHeaders[0].viphash, 32);
        char hash[65];
        hash[64]='\0';
        ed25519_key2text(hash, m_viphash, 32);

        if(!m_firstVipKeys.checkVipKeys(m_viphash, m_firstKeysLen/(2+32))) {
            ELOG("ERROR, failed to check %d VIP keys for hash %s\n", m_firstKeysLen/(2+32), hash);
            return false;
        }

        char fileName[128];
        sprintf(fileName, "vip/%64s.vip", hash);
        if(!m_firstVipKeys.storeVipKeys(fileName)) {
            ELOG("ERROR, failed to save VIP keys for hash %s\n", hash);
            return false;
        }

        memcpy(m_oldhash, m_receivedHeaders[0].nowhash, 32);
        m_block.loadFromHeader(m_receivedHeaders[0]);
        if(memcmp(m_oldhash, m_block.getData().nowHash, 32)) {
            ELOG("ERROR, failed to confirm first header hash, fatal\n");
            return false;
        }

        memcpy(m_oldhash, m_receivedHeaders[0].oldhash, 32);
        if (!laststart) {
            writeStart(m_block.getData().ttime);
        }
    }
    else {
        m_block.getData().ttime = 0;
        m_block.readDataFromHeaderFile();
        if(m_block.getData().ttime + BLOCKSEC != m_receivedHeaders[0].now) { // do not accept download random blocks
            ELOG("ERROR, failed to get correct block (%08X), fatal\n", m_block.getData().ttime + BLOCKSEC);
            return false;
        }
            memcpy(m_viphash, m_block.getData().vipHash, 32);
            memcpy(m_oldhash, m_block.getData().nowHash, 32);

            char hash[65];
            hash[64]='\0';
            ed25519_key2text(hash, m_viphash, 32);
            char filename[128];
            sprintf(filename,"vip/%64s.vip",hash);
            m_firstVipKeys.loadFromFile(filename);
            m_firstKeysLen = m_firstVipKeys.getLength();
    }
    return true;
}

bool GetBlocks::receiveNewVipKeys(INetworkClient& netClient) {
    uint32_t lastkeyslen=0;
    if(!netClient.readData((int32_t*)&lastkeyslen, sizeof(lastkeyslen))) {
        ELOG("GetBlocks ERROR reading last key length\n");
        return false;
    }

    if(lastkeyslen>0) {
        auto responseBuffer = std::make_unique<char[]>(lastkeyslen);
        if(!netClient.readData(responseBuffer.get(), lastkeyslen)) {
            ELOG("GetBlocks ERROR reading response buffer\n");
            return false;
        }

        VipKeys newVipKeys;
        newVipKeys.loadFromBuffer(std::move(responseBuffer), lastkeyslen);

        char hash[65];
        hash[64]='\0';
        ed25519_key2text(hash, m_receivedHeaders[m_numOfBlocks-1].viphash, 32);

        if(!newVipKeys.checkVipKeys(m_receivedHeaders[m_numOfBlocks-1].viphash, lastkeyslen/(2+32))) {
            ELOG("ERROR, failed to check VIP keys for hash %s\n", hash);
            return false;
        }

        char fileName[128];
        sprintf(fileName, "vip/%64s.vip", hash);
        if(!m_firstVipKeys.storeVipKeys(fileName)) {
            ELOG("ERROR, failed to save VIP keys for hash %s\n", hash);
            return false;
        }
    }
    return true;
}

bool GetBlocks::validateChain() {
    for(uint32_t n=0; n<m_numOfBlocks; n++) {
        if(n<m_numOfBlocks-1 && memcmp(m_viphash, m_receivedHeaders[n].viphash, 32)) {
            ELOG("ERROR failed to match viphash for header %08X, fatal\n", m_receivedHeaders[n].now);
            return false;
        }

        if(memcmp(m_oldhash, m_receivedHeaders[n].oldhash, 32)) {
            ELOG("ERROR failed to match oldhash for header %08X, fatal\n", m_receivedHeaders[n].now);
            return false;
        }

        memcpy(m_oldhash, m_receivedHeaders[n].nowhash, 32);
        m_block.loadFromHeader(m_receivedHeaders[n]);
        if(memcmp(m_oldhash, m_block.getData().nowHash, 32)) {
            ELOG("ERROR failed to confirm nowhash for header %08X, fatal\n", m_receivedHeaders[n].now);
            return false;
        }
    }
    return true;
}

bool GetBlocks::validateLastBlockUsingFirstKeys() {
    std::map<uint16_t, hash_s> vipkeys;
    for(uint32_t i=0; i<m_firstKeysLen; i+=2+32) {
        char* firstkeys = m_firstVipKeys.getVipKeys();
        uint16_t svid=*((uint16_t*)(&firstkeys[i]));
        vipkeys[svid]=*((hash_s*)(&firstkeys[i+2]));
    }

    int vok=0;

    for(const auto& s : m_signatures) {
        auto it=vipkeys.find(s.svid);
        if(it==vipkeys.end()) {
            char hash[65];
            hash[64]='\0';
            ed25519_key2text(hash, s.signature, 32);
            ELOG("ERROR vipkey (%d) not found %s\n", s.svid, hash);
            continue;
        }

        if(!ed25519_sign_open((const unsigned char*)(&m_receivedHeaders[m_numOfBlocks-1]), sizeof(header_t)-4, it->second.hash, s.signature)) {
            vok++;
        } else {
            char hash[65];
            hash[64]='\0';
            ed25519_key2text(hash, s.signature, 32);
            ELOG("ERROR vipkey (%d) failed %s\n", s.svid, hash);            
        }
    }

    if(2*vok < (int)vipkeys.size()) {
        ELOG("ERROR failed to confirm enough signature for header %08X (2*%d<%d)\n",
        m_receivedHeaders[m_numOfBlocks-1].now, vok, (int)vipkeys.size());
        return false;
    } else {
        ELOG("CONFIRMED enough signature for header %08X (2*%d>=%d)\n",
        m_receivedHeaders[m_numOfBlocks-1].now, vok, (int)vipkeys.size());
    }
    return true;
}

bool GetBlocks::send(INetworkClient& netClient)
{
    sendDataSize(netClient);

    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetBlocks sending error\n");
        return false;
    }

    readDataSize(netClient);

    if(!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("GetBlocks reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }    

    //read first vipkeys if needed
     if(!getBlockNumberFrom() || getBlockNumberFrom() == getBlockNumberTo()) {
         if(!receiveFirstVipKeys(netClient)) {
             return false;
         }
     }

    //read headers
    if(!receiveHeaders(netClient)) {
        ELOG("GetBlocks ERROR reading server headers\n");
        return false;
    }


    if(m_numOfBlocks == 0) {
        return true;
    }

    //load last header
    if(!loadLastHeader()) {
        ELOG("GetBlocks ERROR loading last header\n");
        return false;
    }

    //read signatures
    if(!receiveSignatures(netClient)) {
        ELOG("GetBlocks ERROR reading signatures\n");
        return false;
    }

    //read new viphash if different from last
    if(!receiveNewVipKeys(netClient)) {
        ELOG("GetBlocks ERROR reading new vip keys\n");
        return false;
    }

    //validate chain
    if(!validateChain()) {
        ELOG("GetBlocks ERROR validating chain\n");
        return false;
    }

    //validate last block using firstkeys
    if(!validateLastBlockUsingFirstKeys()) {
        ELOG("GetBlocks ERROR validating last block using first key\n");
        m_responseError = ErrorCodes::eGetSignatureUnavailable;
        return false; //???
    }

    // save hashes
    for(uint32_t n=0; n<m_numOfBlocks; n++) { //would be faster if avoiding open/close but who cares
        saveNowhash(m_receivedHeaders[n]);
    }

    // update last confirmed header position
    fprintf(stderr,"SAVED %d headers from %08X to %08X\n", m_numOfBlocks, m_receivedHeaders[0].now, m_receivedHeaders[m_numOfBlocks-1].now);
    if(!m_block.updateLastHeader()) {
        ELOG("ERROR, failed to write last header\n");
    }
    return true;
}

void GetBlocks::saveResponse(settings& /*sts*/) {
}

std::string GetBlocks::toString(bool /*pretty*/) {
    return "";
}

void GetBlocks::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        boost::property_tree::ptree blockElement;
        boost::property_tree::ptree blockChild;

        ptree.put("updated_blocks", m_numOfBlocks);

        if(m_receivedHeaders.size() > 0) {
            for(const auto& sh : m_receivedHeaders) {
                char blockId[9];
                sprintf(blockId, "%08X", sh.now);
                blockElement.put_value(blockId);
                blockChild.push_back(std::make_pair("", blockElement));
            }
            ptree.put_child("blocks", blockChild);
        }

        if(m_newviphash) {
            ptree.put("warning", "some blocks may not have been updated due to new nodes - rerun command");
        }
    }
    else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void GetBlocks::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::FROM, m_data.info.from);
    ptree.put(TAG::TO, m_data.info.to);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

uint32_t GetBlocks::getBlockNumberFrom() {
    return m_data.info.from;
}

uint32_t GetBlocks::getBlockNumberTo() {
    return m_data.info.to;
}

std::string GetBlocks::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"get_blocks\",[\"from\":<from_timestamp>],[\"to\":<to_timestamp>]}" << "\n";
    ss << "Example: " << "{\"run\":\"get_blocks\",\"from\":\"1491210824\",\"to\":\"1491211048\"}" << "\n";
    return ss.str();
}
