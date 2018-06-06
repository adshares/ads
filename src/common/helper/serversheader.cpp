#include "serversheader.h"
#include "helper/ascii.h"
#include "helper/json.h"
#include "default.h"

namespace Helper {

void ServersHeader::toJson(boost::property_tree::ptree& blocktree)
{
    char value[65];
    value[64]='\0';

    //blocktree.put("version", version);
    blocktree.put("time", ttime);
    blocktree.put("message_count", messageCount);

    Helper::ed25519_key2text(value, oldHash, 32);
    blocktree.put("oldhash", value);
    Helper::ed25519_key2text(value, minHash, 32);
    blocktree.put("minhash", value);
    Helper::ed25519_key2text(value, msgHash, 32);
    blocktree.put("msghash", value);
    Helper::ed25519_key2text(value, nodHash, 32);
    blocktree.put("nodhash", value);
    Helper::ed25519_key2text(value, vipHash, 32);
    blocktree.put("viphash", value);
    Helper::ed25519_key2text(value, nowHash, 32);
    blocktree.put("nowhash", value);
    blocktree.put("vote_yes", voteYes);
    blocktree.put("vote_no", voteNo);
    blocktree.put("vote_total", voteTotal);
    blocktree.put("node_count", nodesCount);

    blocktree.put("dividend_balance", Helper::print_amount(dividendBalance));
    if(!((ttime/BLOCKSEC)%BLOCKDIV)) {
        blocktree.put("dividend_pay","true");
    } else {
        blocktree.put("dividend_pay","false");
    }
}

}
