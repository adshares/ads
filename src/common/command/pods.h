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

/** \brief Struct used in send one command (TXSTYPE_PUT) */
struct UserSendOneInfo {
    UserSendOneInfo() = default;
    UserSendOneInfo(uint16_t abank_, uint32_t auser_, uint32_t namsid_, uint16_t bbank_, uint32_t buser_, int64_t ntmass_, uint32_t time_, uint8_t ntinfo_[32])
        : abank(abank_), auser(auser_), namsid(namsid_), ttime(time_), bbank(bbank_), buser(buser_), ntmass(ntmass_) {
        std::copy(ntinfo_, ntinfo_+32, ntinfo);
    }

    uint8_t  ttype{TXSTYPE_PUT};
    uint16_t abank{0};
    uint32_t auser{0};
    uint32_t namsid{0};
    uint32_t ttime{0};
    uint16_t bbank{0};
    uint32_t buser{0};
    int64_t ntmass{0};
    uint8_t ntinfo[32];
} __attribute__((packed));

struct UserSendOne {
    UserSendOne() = default;
    UserSendOne(uint16_t abank_, uint32_t auser_, uint32_t namsid_, uint16_t bbank_, uint32_t buser_, int64_t ntmass_, uint32_t time_, uint8_t ntinfo_[32])
        : info(abank_, auser_, namsid_, bbank_, buser_, ntmass_, time_, ntinfo_) {
    }

    UserSendOneInfo info;
    unsigned char sign[64];
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

#endif // PODS_H
