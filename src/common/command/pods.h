#ifndef PODS_H
#define PODS_H

#include "default.hpp"
#include "helper/txsname.h"

/** \brief Struct data for get_me and get_accout response */
struct accountresponse {
    user_t      usera;
    user_t      globalusera;
} __attribute__((packed));

/** \brief Struct data for all command which are changed blockchain data */
struct commandresponse {
    user_t      usera;
    uint32_t    msid;
    uint32_t    mpos;
} __attribute__((packed));

struct tInfo {
    int64_t weight;
    int64_t deduct;
    int64_t fee;
    uint16_t stat;
    uint8_t pkey[6];
}__attribute__((packed));

/** \brief Struct data for get_me and get_accout command */
struct UserInfoData {
    UserInfoData() = default;
    UserInfoData(uint16_t abank_, uint32_t auser_, uint16_t bbank_, uint16_t buser_, uint32_t time_)
        : abank{abank_},
          auser{auser_},
          bbank{bbank_},
          buser{buser_},
          ttime{time_} {
    }

    uint8_t  ttype{TXSTYPE_INF};

    uint16_t abank{0};
    uint32_t auser{0};
    uint16_t bbank{0};
    uint32_t buser{0};
    uint32_t ttime{0};
    //1+2+4+2+4+4,

} __attribute__((packed));


/** \brief Full struct data for get_me and get_accout command with signature */
struct UserInfo {
    UserInfo() = default;
    UserInfo(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser, uint32_t time)
        : info(abank, auser, bbank, buser, time) {
    }

    UserInfoData        info;
    unsigned char   sign[64];
} __attribute__((packed));

/** \brief User info used in send one command (TXSTYPE_PUT) */
struct UserSendOneInfo {
    UserSendOneInfo() = default;
    UserSendOneInfo(uint16_t abank_, uint32_t auser_, uint32_t namsid_, uint16_t bbank_, uint32_t buser_, int64_t ntmass_, uint32_t time_, uint8_t ntinfo_[32])
        : abank(abank_), auser(auser_), namsid(namsid_), ttime(time_), bbank(bbank_), buser(buser_), ntmass(ntmass_) {
        std::copy(ntinfo_, ntinfo_+32, ntinfo);
    }

    uint8_t  ttype{TXSTYPE_PUT};    ///< command type
    uint16_t abank{0};              ///< source node
    uint32_t auser{0};              ///< source user
    uint32_t namsid{0};             ///< user message id
    uint32_t ttime{0};              ///< time
    uint16_t bbank{0};              ///< destination node
    uint32_t buser{0};              ///< destination user
    int64_t ntmass{0};              ///< deduct
    uint8_t ntinfo[32];             ///< additional info
} __attribute__((packed));

/** \brief Struct used in send one command (TXSTYPE_PUT) */
struct UserSendOne {
    UserSendOne() = default;
    UserSendOne(uint16_t abank_, uint32_t auser_, uint32_t namsid_, uint16_t bbank_, uint32_t buser_, int64_t ntmass_, uint32_t time_, uint8_t ntinfo_[32])
        : info(abank_, auser_, namsid_, bbank_, buser_, ntmass_, time_, ntinfo_) {
    }

    UserSendOneInfo info;   ///< user info
    unsigned char sign[64]; ///< signature
} __attribute__((packed));

/** \brief Full struct data for get_me and change account key command with signature */
struct accountkey {
    accountkey() = default;
    accountkey(uint16_t abank_, uint32_t auser_, uint32_t namsid_, uint32_t time_, uint8_t pubkey_[32], uint8_t pubkeysign_[64])
        : abank(abank_),
          auser(auser_),
          amsid(namsid_),
          ttime(time_) {
        std::copy(pubkey_, pubkey_+32, pubkey);
        std::copy(pubkeysign_, pubkeysign_+64, pubkeysign);
    }

    uint8_t         ttype{TXSTYPE_KEY};
    uint16_t        abank{0};
    uint32_t        auser{0};
    uint32_t        amsid{0};
    uint32_t        ttime{0};
    uint8_t         pubkey[32];
    unsigned char   sign[64];
    uint8_t         pubkeysign[64];
} __attribute__((packed));


/** \brief This struct contains data specific for create_node command */
struct CreateNodeInfo {
	CreateNodeInfo() = default;
	CreateNodeInfo(uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_)
	: abank(abank_),
	  auser(auser_),
	  amsid(amsid_),
	  ttime(ttime_) {
	}

	uint8_t ttype{TXSTYPE_BNK};
	uint16_t abank{0};
	uint32_t auser{0};
	uint32_t amsid{0};
	uint32_t ttime{0};	
} __attribute__((packed));

/** \brief Full struct data for create_node command with signature */
struct CreateNodeData {
	CreateNodeData() = default;
	CreateNodeData(uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_)
	: data(abank_, auser_, amsid_, ttime_) {
	}
	
	CreateNodeInfo data;
	unsigned char sign[64];
} __attribute__((packed));

/** \brief Info for send_many transaction type */
struct SendManyInfo {
    SendManyInfo() = default;
    SendManyInfo(uint16_t node, uint32_t user, uint32_t msgid, uint32_t time, uint16_t txncounter)
        : src_node(node), src_user(user), msg_id(msgid), txn_time(time), txn_counter(txncounter) {
    }

    uint8_t  txn_type{TXSTYPE_MPT}; ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t msg_id{0};             ///< user message id
    uint32_t txn_time{0};           ///< time
    uint16_t txn_counter{0};        ///< counter of transactions to perform
} __attribute__((packed));

/** \brief Struct used in send many command (TXSTYPE_MPT) */
struct SendManyData {
    SendManyData() = default;
    SendManyData(uint16_t node, uint32_t user, uint32_t msgid, uint32_t time, uint16_t txncounter)
        : info (node, user, msgid, time, txncounter) {
    }

    SendManyInfo info;
    unsigned char sign[64];
} __attribute__((packed));

/** \brief Vector item, record to store one transaction in send_many message. */
struct SendAmountTxnRecord {
    SendAmountTxnRecord() = default;
    SendAmountTxnRecord(uint16_t bank, uint32_t user, int64_t mass)
        : dest_node(bank), dest_user(user), amount(mass) {
    }

    uint16_t dest_node; ///< destintaion node
    uint32_t dest_user; ///< destination user
    int64_t amount;     ///< sending amount
} __attribute__((packed));

struct CreateAccountInfo {
    CreateAccountInfo() = default;
    CreateAccountInfo(uint16_t srcNode, uint32_t srcUser, uint32_t msgId, uint32_t txnTime,
                      uint16_t destNode)
        : src_node(srcNode), src_user(srcUser), msg_id(msgId), ttime(txnTime), dst_node(destNode) {
    }

    uint8_t  ttype{TXSTYPE_USR};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t msg_id{0};             ///< user message id
    uint32_t ttime{0};              ///< time
    uint16_t dst_node{0};           ///< destination node

}__attribute__((packed));

struct CreateAccountData {
    CreateAccountData() = default;
    CreateAccountData(uint16_t srcNode_, uint32_t srcUser_, uint32_t msgId_, uint32_t txnTime_, uint16_t destNode_)
        : info(srcNode_, srcUser_, msgId_, txnTime_, destNode_) {
    }

    CreateAccountInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct NewAccountData {
    NewAccountData() = default;
    NewAccountData(uint32_t userId, uint8_t userPkey[SHA256_DIGEST_LENGTH])
        : user_id(userId) {
        std::copy(userPkey, userPkey+SHA256_DIGEST_LENGTH, user_pkey);
    }

    uint32_t user_id;
    uint8_t user_pkey[SHA256_DIGEST_LENGTH];
}__attribute__((packed));

/** \brief Info data for get_accounts command. */
struct GetAccountsInfo {
    GetAccountsInfo() = default;
    GetAccountsInfo(uint16_t srcNode, uint32_t srcUser, uint32_t txnTime, uint32_t blockNo,
                      uint16_t destNode)
        : src_node(srcNode), src_user(srcUser), ttime(txnTime), block(blockNo), dst_node(destNode) {
    }

    uint8_t ttype{TXSTYPE_NOD};
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t ttime{0};              ///< time
    uint32_t block{0};              ///< block number
    uint16_t dst_node{0};           ///< destination node
}__attribute__((packed));

/** \brief Data struct for get_accounts command. */
struct GetAccountsData {
    GetAccountsData() = default;
    GetAccountsData(uint16_t srcNode, uint32_t srcUser, uint32_t txnTime, uint32_t blockNo,
                    uint16_t destNode)
        : info(srcNode, srcUser, txnTime, blockNo, destNode) {
    }

    GetAccountsInfo info;           ///< info data struct
    unsigned char sign[64];         ///< signature
}__attribute__((packed));

/** \brief Send Broadcast info */
struct SendBroadcastInfo {
    SendBroadcastInfo() = default;
    SendBroadcastInfo(uint16_t srcNode, uint32_t srcUser, uint32_t msgId, uint32_t txnTime,
                      uint16_t msgLength)
        : src_node(srcNode), src_user(srcUser), msg_id(msgId), ttime(txnTime), msg_length(msgLength) {
    }


    uint8_t  ttype{TXSTYPE_BRO};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t msg_id{0};             ///< user message id
    uint32_t ttime{0};              ///< time
    uint16_t msg_length{0};         ///< msg_length
}__attribute__((packed));

/** \brief Data used in send broadcast command */
struct SendBroadcastData {
    SendBroadcastData() = default;
    SendBroadcastData(uint16_t srcNode_, uint32_t srcUser_, uint32_t msgId_, uint32_t txnTime_, uint16_t msgLength_)
        : info(srcNode_, srcUser_, msgId_, txnTime_, msgLength_) {
    }

    SendBroadcastInfo info;
    unsigned char sign[64];
}__attribute__((packed));

/** \brief Get broadcast info */
struct GetBroadcastInfo {
    GetBroadcastInfo() = default;
    GetBroadcastInfo(uint16_t srcNode, uint32_t srcUser, uint32_t txnTime, uint32_t block_)
        : src_node(srcNode), src_user(srcUser), block(block_), ttime(txnTime) {
    }

    uint8_t  ttype{TXSTYPE_BLG};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t block{0};              ///< block no.
    uint32_t ttime{0};              ///< time
}__attribute__((packed));

/** \brief Get broadcast data */
struct GetBroadcastData {
    GetBroadcastData() = default;
    GetBroadcastData(uint16_t srcNode_, uint32_t srcUser_, uint32_t block_, uint32_t txnTime_)
        : info(srcNode_, srcUser_, txnTime_, block_) {
    }

    GetBroadcastInfo info;
    unsigned char sign[64];
}__attribute__((packed));

/** \brief Header data user in get broadcast command */
struct BroadcastFileHeader {
    uint32_t path;
    uint32_t lpath;
    uint32_t fileSize;
}__attribute__((packed));

/** \brief Rest of broadcast response, after header and message. */
struct GetBroadcastAdditionalData {
    unsigned char sign[64];             ///< signature
    uint8_t hash[SHA256_DIGEST_LENGTH]; ///< users block hash
    uint8_t pkey[SHA256_DIGEST_LENGTH]; ///< public key
    uint32_t msid;                      ///< id of last transaction
    uint32_t mpos;
}__attribute__((packed));

/** \Get Broadcast command response */
struct GetBroadcastResponse {
    SendBroadcastInfo info;
    GetBroadcastAdditionalData data;
}__attribute__((packed));
/** Broadcast file format
 * for each block:
 *      SendBroadcastInfo
 *      Message (floating size = SendBroadcastInfo.msg_length)
 *      GetBroadcastAdditionalData
 */

/** \brief Change Node Key command data */
struct ChangeNodeKeyData {
    ChangeNodeKeyData() = default;
    ChangeNodeKeyData(uint16_t node_, uint32_t srcUser_, uint32_t msgId_, uint32_t txnTime_, uint16_t dstNode_, uint8_t pubKey_[32]):
        src_node{node_},
        src_user{srcUser_},
        msg_id{msgId_},
        ttime{txnTime_},
        dst_node{dstNode_}
    {
        std::copy(pubKey_, pubKey_+32, node_new_key);
    }

    uint8_t  ttype{TXSTYPE_BKY};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t msg_id{0};             ///< user message id
    uint32_t ttime{0};              ///< time
    uint16_t dst_node{0};           ///< destination node
    uint8_t node_new_key[32];       ///< new node key
    unsigned char sign[64];
    uint8_t old_public_key[32];     ///< old public key
}__attribute__((packed));

/** \brief Get block info */
struct GetBlockInfo {
    GetBlockInfo() = default;
    GetBlockInfo(uint16_t srcNode, uint32_t srcUser, uint32_t block_, uint32_t txnTime)
        : src_node(srcNode), src_user(srcUser), block(block_), ttime(txnTime) {
    }

    uint8_t  ttype{TXSTYPE_NDS};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t block{0};              ///< block no.
    uint32_t ttime{0};              ///< time
}__attribute__((packed));

/** Get block data */
struct GetBlockData {
    GetBlockData() = default;
    GetBlockData(uint16_t srcNode_, uint32_t srcUser_, uint32_t block_, uint32_t txnTime_)
        : info(srcNode_, srcUser_, block_, txnTime_) {
    }

    GetBlockInfo info;
    unsigned char sign[64];
}__attribute__((packed));

/** \brief Get list of message info */
struct GetMessageListInfo {
    GetMessageListInfo() = default;
    GetMessageListInfo(uint16_t srcNode, uint32_t srcUser, uint32_t txnTime, uint32_t block_)
        : src_node(srcNode), src_user(srcUser), ttime(txnTime), block(block_) {
    }

    uint8_t  ttype{TXSTYPE_MGS};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t ttime{0};              ///< time
    uint32_t block{0};              ///< block no.
}__attribute__((packed));

/** \brief Get list of message data */
struct GetMessageListData {
    GetMessageListData() = default;
    GetMessageListData(uint16_t srcNode_, uint32_t srcUser_, uint32_t block_, uint32_t txnTime_)
        : info(srcNode_, srcUser_, txnTime_, block_) {
    }

    GetMessageListInfo info;
    unsigned char sign[64];
}__attribute__((packed));

/** \brief Get message info */
struct GetMessageInfo {
    GetMessageInfo() = default;
    GetMessageInfo(uint16_t srcNode, uint32_t srcUser, uint32_t txnTime, uint32_t block_, uint16_t dstNode, uint32_t msgId)
        : src_node(srcNode), src_user(srcUser), ttime(txnTime), block(block_), dst_node(dstNode), node_msgid(msgId) {
    }

    uint8_t  ttype{TXSTYPE_MSG};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t ttime{0};              ///< time
    uint32_t block{0};              ///< block no.
    uint16_t dst_node{0};           ///< destination node
    uint32_t node_msgid{0};         ///< node message id
}__attribute__((packed));

/** \brief Get message data */
struct GetMessageData {
    GetMessageData() = default;
    GetMessageData(uint16_t srcNode_, uint32_t srcUser_, uint32_t block_, uint16_t dst_node, uint32_t msg_id, uint32_t txnTime_)
        : info(srcNode_, srcUser_, txnTime_, block_, dst_node, msg_id) {
    }

    GetMessageInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct GetMessageResponse {
    uint16_t svid;
    uint32_t msgid;
    uint32_t time;
    uint32_t length;
    uint8_t signature[SHA256_DIGEST_LENGTH];
    bool hash_tree_fast;
};

struct GetLogInfo {
    GetLogInfo() = default;
    GetLogInfo(uint16_t node_, uint32_t user_, uint32_t from_)
        : node(node_), user(user_), from(from_) {
    }

    uint8_t ttype{TXSTYPE_LOG}; ///< command type
    uint16_t node{0};           ///< source node
    uint32_t user{0};           ///< source user
    uint32_t from{0};           ///< from time

}__attribute__((packed));

struct GetLogData {
    GetLogData() = default;
    GetLogData(uint16_t node, uint32_t user, uint32_t from)
        : info(node, user, from) {
    }

    GetLogInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct GetTransactionInfo {
    GetTransactionInfo() = default;
    GetTransactionInfo(uint16_t srcNode, uint32_t srcUser, uint32_t time, uint16_t dstNode, uint32_t nodeMsgid, uint32_t pos)
        : src_node(srcNode), src_user(srcUser), ttime(time), dst_node(dstNode), node_msgid(nodeMsgid), position(pos) {
    }

    uint8_t ttype{TXSTYPE_TXS}; ///< command type
    uint16_t src_node{0};       ///< source node
    uint32_t src_user{0};       ///< source user
    uint32_t ttime{0};          ///< time
    uint16_t dst_node{0};       ///< dest node
    uint32_t node_msgid{0};     ///< node msg id
    uint16_t position{0};       ///< position
}__attribute__((packed));

struct GetTransactionData {
    GetTransactionData() = default;
    GetTransactionData(uint16_t srcNode_, uint32_t srcUser_, uint32_t time_, uint16_t dstNode_, uint32_t nodeMsgid_, uint32_t pos_)
        : info(srcNode_, srcUser_, time_, dstNode_, nodeMsgid_, pos_) {
    }

    GetTransactionInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct GetTransactionResponse {
    uint32_t path;
    uint32_t msid;
    uint16_t node;
    uint16_t tnum;
    uint16_t len; //64k lentgh limit !
    uint16_t hnum;
}__attribute__((packed));

/* inform peers about location */
struct ConnectedInfo {
    ConnectedInfo() = default;
    ConnectedInfo(uint16_t port_, uint32_t ip_address_) : port(port_), ip_address(ip_address_) {
    }

    uint8_t ttype{TXSTYPE_CON}; ///< command type
    uint16_t port{0};           ///< port number
    uint32_t ip_address{0};     ///< ip address
}__attribute__((packed));

struct TransactionAcceptedInfo {
    TransactionAcceptedInfo() = default;
    TransactionAcceptedInfo(uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_, uint16_t bbank_, uint32_t buser_, uint8_t public_key[32])
        : abank(abank_), auser(auser_), amsid(amsid_), ttime(ttime_), bbank(bbank_), buser(buser_) {
        std::copy(public_key, public_key + 32, publicKey);
    }

    uint8_t ttype{TXSTYPE_UOK};  ///< command type
    uint16_t abank{0};          ///< source node
    uint32_t auser{0};          ///< source user
    uint32_t amsid{0};          ///< msg id
    uint32_t ttime{0};          ///< time
    uint16_t bbank{0};          ///< dest node
    uint32_t buser{0};          ///< dest user
    uint8_t  publicKey[32];     ///< new account public key
}__attribute__((packed));

struct TransactionFailedInfo {
    TransactionFailedInfo() = default;
    TransactionFailedInfo(uint32_t msgSize) : message_size(msgSize) {
    }
    uint8_t ttype{TXSTYPE_NON};  ///< command type
    uint32_t message_size{0};    ///< message size
}__attribute__((packed));

struct AccountStatusInfo {
    AccountStatusInfo(uint8_t ttype_) : ttype(ttype_) {}
    AccountStatusInfo(uint8_t ttype_, uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_, uint16_t bbank_, uint32_t buser_, uint16_t status_)
        : ttype(ttype_), abank(abank_), auser(auser_), amsid(amsid_), ttime(ttime_), bbank(bbank_), buser(buser_), status(status_) {
    }

    uint8_t ttype;              ///< command type
    uint16_t abank{0};          ///< source node
    uint32_t auser{0};          ///< source user
    uint32_t amsid{0};          ///< msg id
    uint32_t ttime{0};          ///< time
    uint16_t bbank{0};          ///< dest node
    uint32_t buser{0};          ///< dest user
    uint16_t status{0};         ///< account status
}__attribute__((packed));

struct AccountStatusData {
    AccountStatusData(uint8_t ttype_) : info(ttype_) {}
    AccountStatusData(uint8_t ttype_, uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_, uint16_t bbank_, uint32_t buser_, uint16_t status_)
        : info(ttype_, abank_, auser_, amsid_, ttime_, bbank_, buser_, status_) {
    }

    AccountStatusInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct NodeStatusInfo {
    NodeStatusInfo(uint8_t ttype_) : ttype(ttype_) {}
    NodeStatusInfo(uint8_t ttype_, uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_, uint16_t bbank_, uint32_t status_)
        : ttype(ttype_), abank(abank_), auser(auser_), amsid(amsid_), ttime(ttime_), bbank(bbank_), status(status_) {
    }

    uint8_t ttype;                 ///< command type
    uint16_t abank{0};             ///< source node
    uint32_t auser{0};             ///< source user
    uint32_t amsid{0};             ///< msg id
    uint32_t ttime{0};             ///< time
    uint16_t bbank{0};             ///< dest node
    uint32_t status{0};            ///< node status
}__attribute__((packed));

struct NodeStatusData {
    NodeStatusData(uint8_t ttype_) : info(ttype_) {}
    NodeStatusData(uint8_t ttype_, uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_, uint16_t bbank_, uint32_t status_)
        : info(ttype_, abank_, auser_, amsid_, ttime_, bbank_, status_) {
    }

    NodeStatusInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct GetSignaturesInfo {
    GetSignaturesInfo() = default;
    GetSignaturesInfo(uint16_t abank_, uint32_t auser_, uint32_t ttime_, uint32_t block_)
        : abank(abank_), auser(auser_), ttime(ttime_), block(block_) {
    }

    uint8_t ttype{TXSTYPE_SIG};    ///< command type
    uint16_t abank{0};             ///< source node
    uint32_t auser{0};             ///< source user
    uint32_t ttime{0};             ///< time
    uint32_t block{0};             ///< block number
}__attribute__((packed));

struct GetSignaturesData {
    GetSignaturesData() = default;
    GetSignaturesData(uint16_t abank_, uint32_t auser_, uint32_t ttime_, uint32_t block_)
        : info(abank_, auser_, ttime_, block_) {
    }

    GetSignaturesInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct RetrieveFundsInfo {
    RetrieveFundsInfo() = default;
    RetrieveFundsInfo(uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_, uint16_t bbank_, uint32_t buser_)
        : abank(abank_), auser(auser_), amsid(amsid_), ttime(ttime_), bbank(bbank_), buser(buser_) {
    }

    uint8_t ttype {TXSTYPE_GET};///< command type
    uint16_t abank{0};          ///< source node
    uint32_t auser{0};          ///< source user
    uint32_t amsid{0};          ///< msg id
    uint32_t ttime{0};          ///< time
    uint16_t bbank{0};          ///< dest node
    uint32_t buser{0};          ///< dest user
}__attribute__((packed));

struct RetrieveFundsData {
    RetrieveFundsData() = default;
    RetrieveFundsData(uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_, uint16_t bbank_, uint32_t buser_)
        : info(abank_, auser_, amsid_, ttime_, bbank_, buser_) {
    }

    RetrieveFundsInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct GetVipKeysInfo {
    GetVipKeysInfo() = default;
    GetVipKeysInfo(uint16_t abank_, uint32_t auser_, uint32_t ttime_, uint8_t vhash_[32])
        : abank(abank_), auser(auser_), ttime(ttime_) {
        std::copy(vhash_, vhash_+32, viphash);
    }

    uint8_t ttype {TXSTYPE_VIP};///< command type
    uint16_t abank{0};          ///< source node
    uint32_t auser{0};          ///< source user
    uint32_t ttime{0};          ///< time
    uint8_t viphash[32]{};       ///< vip hash

}__attribute__((packed));

struct GetVipKeysData {
    GetVipKeysData() = default;
    GetVipKeysData(uint16_t abank_, uint32_t auser_, uint32_t ttime_, uint8_t vhash_[32])
        : info(abank_, auser_, ttime_, vhash_) {
    }

    GetVipKeysInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct GetBlocksInfo {
    GetBlocksInfo() = default;
    GetBlocksInfo(uint16_t abank_, uint32_t auser_, uint32_t ttime_, uint32_t from_, uint32_t to_)
        : abank(abank_), auser(auser_), ttime(ttime_), from(from_), to(to_) {
    }

    uint8_t ttype {TXSTYPE_BLK};///< command type
    uint16_t abank{0};          ///< source node
    uint32_t auser{0};          ///< source user
    uint32_t ttime{0};          ///< time
    uint32_t from{0};           ///< block number from
    uint32_t to{0};             ///< block number to
}__attribute__((packed));

struct GetBlocksData {
    GetBlocksData() = default;
    GetBlocksData(uint16_t abank_, uint32_t auser_, uint32_t ttime_, uint32_t from_, uint32_t to_)
        : info(abank_, auser_, ttime_, from_, to_) {
    }

    GetBlocksInfo info;
    unsigned char sign[64];
}__attribute__((packed));

struct LogAccountInfo {
    LogAccountInfo() = default;
    LogAccountInfo(uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_)
        : abank(abank_), auser(auser_), amsid(amsid_), ttime(ttime_) {
    }

    uint8_t ttype {TXSTYPE_SAV};///< command type
    uint16_t abank{0};          ///< source node
    uint32_t auser{0};          ///< source user
    uint32_t amsid{0};          ///< msg id
    uint32_t ttime{0};          ///< time
}__attribute__((packed));

struct LogAccountData {
    LogAccountData() = default;
    LogAccountData(uint16_t abank_, uint32_t auser_, uint32_t amsid_, uint32_t ttime_)
        : info(abank_, auser_, amsid_, ttime_) {
    }

    LogAccountInfo info;
    unsigned char sign[64];
}__attribute__((packed));

#endif // PODS_H
