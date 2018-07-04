#ifndef MSGLISTPARSER_H
#define MSGLISTPARSER_H

#include <map>
#include <openssl/sha.h>
#include <vector>

#include "default.hpp"

/** Header of msglist.data file */
struct MessageListHeader {
    MessageListHeader() = default;
    MessageListHeader(uint32_t numOfMsg, uint8_t messageHash[SHA256_DIGEST_LENGTH])
        : num_of_msg(numOfMsg) {
        std::copy(messageHash, messageHash + SHA256_DIGEST_LENGTH, messageHash);
    }

    uint32_t num_of_msg;                    ///< number of messages (MessageRecord's)
    uint8_t message_hash[SHA256_DIGEST_LENGTH]; ///< message pack hash
}__attribute__((packed));

/** \brief Single message record */
struct MessageRecord {
    MessageRecord() = default;
    MessageRecord(uint16_t nodeId, uint32_t nodeMsid, uint8_t msgHash[SHA256_DIGEST_LENGTH])
        : node_id(nodeId), node_msid(nodeMsid) {
        std::copy(msgHash, msgHash + SHA256_DIGEST_LENGTH, hash);
    }

    uint16_t node_id;                   ///< node id
    uint32_t node_msid;                 ///< node message id
    uint8_t hash[SHA256_DIGEST_LENGTH]; ///< message hash
}__attribute__((packed));


namespace Parser {

//! Messages iterator
typedef std::vector<MessageRecord>::iterator MsgIterator;
//! Hashes iterator
typedef std::vector<hash_s>::iterator HashIterator;

/**
 * Class to parse and extract data from msglist.dat files
 */
class MsglistParser {
public:
    MsglistParser() = delete;
    MsglistParser(uint32_t path);
    MsglistParser(const char* filepath);

    ~MsglistParser();

    /**
     * @brief Load msglist.dat file.
     * @param filepath - [optional] file path
     * @return true - if successfuly opened and loaded, otherwise false
     */
    bool load(const char* filepath = nullptr);

    /**
     * @brief save msglist.dat file
     * @param filepath - [optional] file path
     * @return true - if successfuly saved, otherwise false
     */
    bool save(const char* filepath = nullptr);

    /**
     * @brief Get header of msglist.dat file.
     * @return MessageListHeader struct
     */
    MessageListHeader getHeader();

    /**
     * @brief Get vector of all messages
     * @return vector of MessageRecord
     */
    std::vector<MessageRecord> getMessageList();

    /**
     * @brief Get only hash from file header.
     * @return hash
     */
    uint8_t* getHeaderHash();

    /**
     * @brief Total no of additional hashes at the end of msglist file
     * @return no of hashes
     */
    uint32_t getHashesTotalSize();

    //! iterator to first messages element.
    MsgIterator begin();
    //! iterator to the end of messages vector.
    MsgIterator end();

    //! check is list of messages empty
    bool isEmpty();

    /** get element, id - numbers starting from 1 */
    MsgIterator get(unsigned int id);

    //! iterator to first element of additional hashes
    HashIterator h_begin();
    //! iterator to last element of additional hashes
    HashIterator h_end();

private:
    std::string m_filePath;
    MessageListHeader m_header;
    std::vector<MessageRecord> m_msgList;
    std::vector<hash_s> m_hashes;
};

}
#endif // MSGLISTPARSER_H
