#ifndef PODS_H
#define PODS_H

#include "default.hpp"
//TODO, change the structure of this data and covert it to an array of structures or array of classes

//bank only
#define TXSTYPE_NON 0	/* placeholder for failed transactions */
#define TXSTYPE_CON 1	/* connected */			/* inform peers about location */
#define TXSTYPE_UOK 2	/* accept remote account request */
//bank & user
#define TXSTYPE_BRO 3	/* broadcast */			/* send ads to network */
#define TXSTYPE_PUT 4	/* send funds */
#define TXSTYPE_MPT 5	/* send funds to many accounts */
#define TXSTYPE_USR 6	/* create new user */
#define TXSTYPE_BNK 7	/* create new bank */
#define TXSTYPE_GET 8	/* get funds */			/* retreive funds from remote/dead bank */
#define TXSTYPE_KEY 9	/* change account key */
#define TXSTYPE_BKY 10	/* change bank key */
#define TXSTYPE_SUS 11	/* NEW set user status bits (OR) */
#define TXSTYPE_SBS 12	/* NEW set bank status bits (OR) */
#define TXSTYPE_UUS 13	/* NEW unset user status bits (AND) */
#define TXSTYPE_UBS 14	/* NEW unset bank status bits (AND) */
#define TXSTYPE_SAV 15  /* NEW request account status confirmation in chain */
//overwriting user only TX in log
#define TXSTYPE_DIV 16	/* user dividend payment tx code */
#define TXSTYPE_FEE 17	/* bank fee payment tx code */
//user only
#define TXSTYPE_INF 16	/* strating from this message only messages between wallet and client.hpp */
#define TXSTYPE_LOG 17	/* return user status and log */
#define TXSTYPE_BLG 18	/* return broadcast log */
#define TXSTYPE_BLK 19	/* return block list */
#define TXSTYPE_TXS 20	/* return transaction + hash path */
#define TXSTYPE_VIP 21	/* return vip public keys */
#define TXSTYPE_SIG 22	/* return block signatures */
#define TXSTYPE_NDS 23	/* return nodes (servers.srv) */
#define TXSTYPE_NOD 24	/* return users of a node */
#define TXSTYPE_MGS 25	/* return list of messages */
#define TXSTYPE_MSG 26	/* return message */
//end
#define TXSTYPE_MAX 27  /* should be 0xFE, with txslen[0xFE]=max_fixed_transaction_size */

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
struct userinfo {
    userinfo() = default;
    userinfo(uint16_t abank_, uint32_t auser_, uint16_t bbank_, uint16_t buser_, uint32_t time_)
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
struct usertxs2 {
    usertxs2() = default;
    usertxs2(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser, uint32_t time)
        : info(abank, auser, bbank, buser, time) {
    }

    userinfo        info;
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


/** \brief Change Node key command info */
struct ChangeNodeKeyInfo {
    ChangeNodeKeyInfo() = default;
    ChangeNodeKeyInfo(uint16_t node, uint32_t srcUser, uint32_t msgId, uint32_t txnTime, uint16_t dstNode, uint8_t pubKey[32])
        : src_node(node), src_user(srcUser), msg_id(msgId), ttime(txnTime), dst_node(dstNode) {
        std::copy(pubKey, pubKey+32, node_new_key);
    }

    uint8_t  ttype{TXSTYPE_BKY};    ///< command type
    uint16_t src_node{0};           ///< source node
    uint32_t src_user{0};           ///< source user
    uint32_t msg_id{0};             ///< user message id
    uint32_t ttime{0};              ///< time
    uint16_t dst_node{0};           ///< destination node
    uint8_t node_new_key[32];       ///< new node key
    uint8_t old_public_key[32];     ///< old public key
}__attribute__((packed));

/** \brief Change Node Key command data */
struct ChangeNodeKeyData {
    ChangeNodeKeyData() = default;
    ChangeNodeKeyData(uint16_t node_, uint32_t srcUser_, uint32_t msgId_, uint32_t txnTime_, uint16_t dstNode_, uint8_t pubKey_[32])
        : info(node_, srcUser_, msgId_, txnTime_, dstNode_, pubKey_) {
    }

    ChangeNodeKeyInfo info;
    unsigned char sign[64];
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

/** \brief Get message list response record */
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

#endif // PODS_H
