#ifndef TXSNAME_H
#define TXSNAME_H


namespace Helper {

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
//overwriting user only TX in log
#define TXSTYPE_DIV 15	/* user dividend payment tx code */
#define TXSTYPE_FEE 16	/* bank fee payment tx code */
//user only
#define TXSTYPE_INF 15	/* strating from this message only messages between wallet and client.hpp */
#define TXSTYPE_LOG 16	/* return user status and log */
#define TXSTYPE_BLG 17	/* return broadcast log */
#define TXSTYPE_BLK 18	/* return block list */
#define TXSTYPE_TXS 19	/* return transaction + hash path */
#define TXSTYPE_VIP 20	/* return vip public keys */
#define TXSTYPE_SIG 21	/* return block signatures */
#define TXSTYPE_NDS 22	/* return nodes (servers.srv) */
#define TXSTYPE_NOD 23	/* return users of a node */
#define TXSTYPE_MGS 24	/* return list of messages */
#define TXSTYPE_MSG 25	/* return message */
#define TXSTYPE_GFI 26  /* return fileds of transactio type */
//end
#define TXSTYPE_MAX 27  /* should be 0xFE, with txslen[0xFE]=max_fixed_transaction_size */

static const char* txsname[TXSTYPE_MAX]= {
    "empty",		//0
    "dividend",		//1
    "account_created",	//2
    "broadcast",		//3
    "send_one",		//4
    "send_many",		//5
    "create_account",	//6
    "create_node",		//7
    "retrieve_funds",	//8
    "change_account_key",	//9
    "change_node_key",	//10
    "set_account_status",	//11 NEW
    "set_node_status",	//12 NEW
    "unset_account_status",	//13 NEW
    "unset_node_status",	//14 NEW
    "get_account",		//15 also 'get_me'
    "get_log",		//16
    "get_broadcast",	//17
    "get_blocks",		//18
    "get_transaction",	//19
    "get_vipkeys",		//20
    "get_signatures",	//21
    "get_block",		//22
    "get_accounts",		//23
    "get_message_list",	//24
    "get_message", //25
    "get_fields" //26
};

static const char* logname[TXSTYPE_MAX]= {
    "node_started",		//0
    "unknown",		//1
    "account_created",	//2
    "broadcast",		//3
    "send_one",		//4
    "send_many",		//5
    "create_account",	//6
    "create_node",		//7
    "retrieve_funds",	//8
    "change_account_key",	//9
    "change_node_key",	//10
    "set_account_status",	//11 NEW
    "set_node_status",	//12 NEW
    "unset_account_status",	//13 NEW
    "unset_node_status",	//14 NEW
    "dividend",		//15
    "bank_profit",		//16
    "unknown",		//17
    "unknown",		//18
    "unknown",		//19
    "unknown",		//20
    "unknown",		//21
    "unknown",		//22
    "unknown",		//23
    "unknown",		//24
    "unknown",      //25
    "unknown"       //26
};

inline int getTxnTypeId(const char* txnName) {
    for (int i=0; i<TXSTYPE_MAX; ++i)  {
        if (strcmp(txsname[i], txnName) == 0) {
            return i;
        }
    }
    return -1;
}

inline int getTxnLogTypeId(const char* txnName) {
    for (int i=0; i<TXSTYPE_MAX; ++i) {
        if (strcmp(logname[i], txnName) == 0) {
            return i;
        }
    }
    return -1;
}

inline const char* getTxnName(int id) {
    if (id < TXSTYPE_MAX) {
        return txsname[id];
    }
    return "";
}

inline const char* getTxnLogName(int id) {
    if (id < TXSTYPE_MAX) {
        return logname[id];
    }
    return "";
}

}

#endif // TXSNAME_H
