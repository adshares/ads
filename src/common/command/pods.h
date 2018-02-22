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

struct accountresponse
{
    user_t      usera;
    user_t      globalusera;
}__attribute__((packed));

struct commandresponse
{
    user_t      usera;
    uint32_t    msid;
    uint32_t    mpos;
}__attribute__((packed));

struct userinfo
{
    userinfo() = default;
    userinfo(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser, uint32_t time)
        : _abank(abank),
          _auser(auser),
          _bbank(bbank),
          _buser(buser),
          _ttime(time)
    {
    }

    uint8_t  _ttype{TXSTYPE_INF};

    uint16_t _abank{0};
    uint32_t _auser{0};
    uint16_t _bbank{0};
    uint32_t _buser{0};
    uint32_t _ttime{0};
    //1+2+4+2+4+4,

} __attribute__((packed));

struct usertxs2
{
    usertxs2() = default;
    usertxs2(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser, uint32_t time)
        : _info(abank, auser, bbank, buser, time)
    {
    }

    userinfo        _info;
    unsigned char   _sign[64];
} __attribute__((packed));

struct usertxsresponse
{
    user_t      m_response;
    user_t      m_globalUser;
} __attribute__((packed));


struct accountkey
{
    accountkey() = default;
    accountkey(uint16_t abank, uint32_t auser, uint32_t namsid, uint32_t time, uint8_t pubkey[32], uint8_t pubkeysign[64])
        : _abank(abank),
          _auser(auser),
          _amsid(namsid),
          _ttime(time)
    {
        std::copy(pubkey, pubkey+32, _pubkey);
        std::copy(pubkeysign, pubkeysign+64, _pubkeysign);
    }

    uint8_t         _ttype{TXSTYPE_KEY};
    uint16_t        _abank{0};
    uint32_t        _auser{0};
    uint32_t        _amsid{0};
    uint32_t        _ttime{0};
    uint8_t         _pubkey[32];
    unsigned char   _sign[64];
    uint8_t         _pubkeysign[64];    
} __attribute__((packed));



#endif // PODS_H
