#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <memory>
#include "user.hpp"
#include <boost/thread/recursive_mutex.hpp>
#include <boost/optional/optional.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "settings.hpp"
#include "servers.hpp"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "command/createnode.h"
#include "command/sendone.h"
#include "command/sendmany.h"
#include "command/createaccount.h"
#include "command/getaccounts.h"
#include "command/broadcastmsg.h"
#include "command/getbroadcastmsg.h"
#include "command/changenodekey.h"
#include "command/getblock.h"
#include "command/getmessage.h"
#include "command/getmessagelist.h"
#include "command/factory.h"
#include "command/getlog.h"
#include "command/gettransaction.h"
#include "command/setaccountstatus.h"
#include "command/setnodestatus.h"
#include "command/unsetaccountstatus.h"
#include "command/unsetnodestatus.h"
#include "command/getfields.h"
#include "command/getsignatures.h"
#include "command/retrievefunds.h"
#include "command/getvipkeys.h"
#include "command/getblocks.h"
#include "command/logaccount.h"
#include "helper/txsname.h"

using namespace Helper;

bool parse_user(uint32_t& to_user,std::string str_user) {
    char *endptr;
    if(str_user.length()!=8) {
        fprintf(stderr,"ERROR: parse_user(%s) bad length (required 8)\n",str_user.c_str());
        return(false);
    }
    errno=0;
    to_user=(uint32_t)strtoll(str_user.c_str(),&endptr,16);
    if(errno || endptr==str_user.c_str()) {
        fprintf(stderr,"ERROR: parse_user(%s)\n",str_user.c_str());
        perror("ERROR: strtol");
        return(false);
    }
    return(true);
}

bool parse_key(uint8_t* to_key,boost::optional<std::string>& json_key,int len, std::string field) {
    std::string str_key=json_key.get();
    if((int)str_key.length()!=2*len) {
        std::stringstream info;
        info << "parse_key(" << field << ") bad string length (required " << 2*len << ")";
        throw CommandException(ErrorCodes::Code::eCommandParseError, info.str());
    }
    if(str_key.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
        std::stringstream info;
        info << "parse_key(" << field << ") bad string format";
        throw CommandException(ErrorCodes::Code::eCommandParseError, info.str());
    }
    ed25519_text2key(to_key,str_key.c_str(),len);
    return(true);
}

uint32_t hexdec(std::string str, uint32_t fallback = 0) {
    try {
        return std::stoul(str, nullptr, 16);
    } catch(std::exception &ex) {
        return fallback;
    }
}

void checkUnusedFields(boost::property_tree::ptree& pt, std::string used_fields) {
    std::vector<std::string> parts;
    boost::split(parts, used_fields, boost::is_any_of(","));
    parts.push_back("run");
    parts.push_back("signature");
    parts.push_back("time");
    parts.push_back("sender");
    parts.push_back("hash");
    parts.push_back("msid");
    parts.push_back("extra_data");

    for(auto& it : pt) {
        if(std::find(parts.begin(), parts.end(), it.first) == parts.end()) {
            throw CommandException(ErrorCodes::Code::eCommandParseError, "Unrecognised field: " + it.first);
        }
    }
}

std::unique_ptr<IBlockCommand> run_json(settings& sts, boost::property_tree::ptree& pt) {
    uint16_t    to_bank=0;
    uint32_t    to_user=0;
    int64_t     to_mass=0;
    uint32_t    to_from=0;
    uint8_t     to_info[32]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint8_t     to_pkey[32];
    uint8_t     to_confirm[64];
    uint8_t     to_signature[64];
    std::string txn_type{};
    uint32_t    now=time(NULL);
    uint32_t    to_block=now-(now%BLOCKSEC)-BLOCKSEC;

    uint16_t sts_bank = sts.bank;
    uint32_t sts_user = sts.user;

    sts.signature_provided = false;

    std::string run=pt.get<std::string>("run");

    boost::optional<std::string> json_signature=pt.get_optional<std::string>("signature");
    if(json_signature) {
        if(parse_key(to_signature,json_signature,64,"signature")) {
            sts.signature_provided = true;
        } else {
            return nullptr;
        }
    }

    boost::optional<std::string> json_sender=pt.get_optional<std::string>("sender");
    if(json_sender) {
        if(!sts.drun && !sts.signature_provided) {
            std::cerr << "Sender given but no signature provided. Abort." << std::endl;
            throw CommandException(ErrorCodes::Code::eCommandParseError, "Sender given but no signature provided");
        }
        if(!sts.parse_acnt(sts_bank, sts_user, *json_sender, "sender")) {
            std::cerr << "Invalid sender. Abort." << std::endl;
            return nullptr;
        }
    }

    if(run != "decode_raw" && !sts.drun && sts.without_secret && !sts.signature_provided) {
      std::cerr << "No secret and no signature provided. Abort." << std::endl;
      throw CommandException(ErrorCodes::Code::eCommandParseError, "No secret and no signature provided.");
    }

    if(run != "decode_raw" && sts.drun && sts.without_secret) {
      std::cerr << "WARNING: dry-run will not produce signature (no secret provided)\n";
    }

    if(run != "decode_raw" && sts.drun && sts.without_secret) {
      std::cerr << "WARNING: dry-run will not produce signature (no secret provided)\n";
    }

    boost::optional<std::string> json_extra_hex=pt.get_optional<std::string>("extra_data");
    std::string extraData;
    if(json_extra_hex) {
       std::string text_hex=json_extra_hex.get();
       if(text_hex.length() % 2 != 0) {
           throw CommandException(ErrorCodes::Code::eCommandParseError, "extra_data invalid length.");
       }
       if(text_hex.length() > MAX_EXTRADATA_LENGTH * 2) {
          throw CommandException(ErrorCodes::Code::eCommandParseError, "extra_data too long.");
      }
       extraData.resize(text_hex.length() / 2);
       parse_key(reinterpret_cast<unsigned char*>(&extraData[0]), json_extra_hex, text_hex.length() / 2, "extra_data");
    }

    boost::optional<uint32_t> json_time=pt.get_optional<uint32_t>("time");
    if(json_time) {
        now=json_time.get();
    }
    boost::optional<std::string> json_confirm=pt.get_optional<std::string>("confirm");
    if(json_confirm && !parse_key(to_confirm,json_confirm,64,"confirm")) {
        return nullptr;
    }
    boost::optional<std::string> json_pkey=pt.get_optional<std::string>("public_key");
    if(json_pkey && !parse_key(to_pkey,json_pkey,32,"public_key")) {
        return nullptr;
    }
    boost::optional<std::string> json_hash=pt.get_optional<std::string>("hash");
    if(json_hash && !parse_key(sts.ha.data(),json_hash,32,"hash")) {
        return nullptr;
    }
    boost::optional<std::string> json_mass=pt.get_optional<std::string>("amount");
    if(json_mass && !parse_amount(to_mass,json_mass.get())) {
        throw CommandException(ErrorCodes::Code::eCommandParseError, "Amount parse error");
    }
    boost::optional<std::string> json_user=pt.get_optional<std::string>("id");
    if(json_user && !parse_user(to_user,json_user.get())) {
        return nullptr;
    }
    boost::optional<std::string> json_acnt=pt.get_optional<std::string>("address");
    if(json_acnt && !sts.parse_acnt(to_bank,to_user,json_acnt.get(), "address")) {
        return nullptr;
    }
    boost::optional<uint16_t> json_bank=pt.get_optional<uint16_t>("node");
    if(json_bank) {
        to_bank=json_bank.get();
    }
    boost::optional<uint32_t> json_msid=pt.get_optional<uint32_t>("msid");
    if(json_msid) {
        sts.msid=json_msid.get();
    }

    boost::optional<std::string> json_block=pt.get_optional<std::string>("block");
    if(json_block) {
        to_block=hexdec(json_block.get());
    }
    boost::optional<std::string> json_type=pt.get_optional<std::string>("type");
    if (json_type) {
        txn_type = json_type.get();
    }


    std::unique_ptr<IBlockCommand> command;

    if(!run.compare("get_me")) {
        checkUnusedFields(pt, "");
        command = std::make_unique<GetAccount>(sts_bank,sts_user,sts_bank,sts_user,now);
    }
    else if(!run.compare(txsname[TXSTYPE_INF])) {
        checkUnusedFields(pt, "address");
        if(!to_bank && !to_user) { // no target account specified
            command = std::make_unique<GetAccount>(sts_bank,sts_user,sts_bank,sts_user,now);
        }
        else {
            command = std::make_unique<GetAccount>(sts_bank,sts_user,to_bank,to_user,now);
        }
    }
    else if(!run.compare(txsname[TXSTYPE_LOG])) {
        checkUnusedFields(pt, "from,type,address,full");
        boost::optional<uint32_t> json_from=pt.get_optional<uint32_t>("from"); //FIXME, decide HEX or DEC
        if(json_from) {
            to_from=json_from.get();
        }
        bool full = false;
        boost::optional<bool> json_full=pt.get_optional<bool>("full"); //FIXME, decide HEX or DEC
        if(json_full) {
            full=json_full.get();
        }
        command = std::make_unique<GetLog>(sts_bank, sts_user, to_from, to_bank, to_user, full, txn_type.c_str());
    }
    else if(!run.compare(txsname[TXSTYPE_BLG])) {
        checkUnusedFields(pt, "from");
        boost::optional<std::string> json_from=pt.get_optional<std::string>("from");
        if(json_from) {
            to_from=hexdec(json_from.get());
        }
        command = std::make_unique<GetBroadcastMsg>(sts_bank, sts_user, to_from, now);
    }
    else if(!run.compare(txsname[TXSTYPE_BLK])) {
        boost::optional<std::string> json_from=pt.get_optional<std::string>("from");
        if(json_from) {
            to_from=hexdec(json_from.get());
        }
        if(!to_from) {
            servers block;
            block.header_get();
            if(block.now) {
                to_from=block.now+BLOCKSEC;
            }
        }
        uint32_t to_to=0;
        boost::optional<std::string> json_to=pt.get_optional<std::string>("to");
        if(json_to) {
            to_to=hexdec(json_to.get());
        }
        checkUnusedFields(pt, "from,to");
        command = std::make_unique<GetBlocks>(sts_bank, sts_user, now, to_from, to_to);
    }
    else if(!run.compare(txsname[TXSTYPE_TXS])) {
        uint32_t node_msid=0;
        uint32_t node_mpos=0;
        boost::optional<std::string> json_txid=pt.get<std::string>("txid");
        if(json_txid && !sts.parse_txid(to_bank,node_msid,node_mpos,json_txid.get(),"txid")) {
            return nullptr;
        }
        checkUnusedFields(pt, "txid");
        command = std::make_unique<GetTransaction>(sts_bank, sts_user, to_bank, node_msid, node_mpos, now);
    }
    else if(!run.compare(txsname[TXSTYPE_VIP])) {
        boost::optional<std::string> json_viphash=pt.get<std::string>("viphash");
        if(json_viphash && !parse_key(to_info,json_viphash,32,"viphash")) {
            return nullptr;
        }
        checkUnusedFields(pt, "viphash");
        command = std::make_unique<GetVipKeys>(sts_bank, sts_user, now, to_info);
    }
    else if(!run.compare(txsname[TXSTYPE_SIG])) {
        checkUnusedFields(pt, "block");
        command = std::make_unique<GetSignatures>(sts_bank, sts_user, now, to_block);
    }
    else if(!run.compare(txsname[TXSTYPE_NDS])) {
        checkUnusedFields(pt, "block");
        command = std::make_unique<GetBlock>(sts_bank, sts_user, to_block, now);
    }
    else if(!run.compare(txsname[TXSTYPE_NOD])) {
        checkUnusedFields(pt, "node,block");
        command = std::make_unique<GetAccounts>(sts_bank, sts_user, to_block, to_bank, now);
    }
    else if(!run.compare(txsname[TXSTYPE_MGS])) {
        checkUnusedFields(pt, "block");
        command = std::make_unique<GetMessageList>(sts_bank, sts_user, to_block, now);
    }
    else if(!run.compare(txsname[TXSTYPE_MSG])) {
        uint32_t to_node_msid=0;
        boost::optional<uint32_t> json_node_msid=pt.get_optional<uint32_t>("node_msid");
        if(json_node_msid) {
            to_node_msid=json_node_msid.get();
        }

        boost::optional<std::string> json_txid;

        if(json_node_msid) {
            json_txid=pt.get_optional<std::string>("message_id");
        } else {
            json_txid=pt.get<std::string>("message_id");
        }
        if(json_txid && !sts.parse_msgid(to_bank,to_node_msid,json_txid.get(),"message_id")) {
            return nullptr;
        }

        boost::optional<std::string> json_block=pt.get_optional<std::string>("block");
        if(!json_block) {
            to_block=0;
        }
        checkUnusedFields(pt, "message_id,node_msid,block,node");
        command = std::make_unique<GetMessage>(sts_bank, sts_user, to_block, to_bank, to_node_msid, now);
    }
    else if(!run.compare("send_again") || !run.compare("send_raw") || !run.compare("decode_raw")) {
        checkUnusedFields(pt, "data");
        boost::optional<std::string> json_data=pt.get<std::string>("data");
        if(json_data) {
            std::string data_str=json_data.get();
            int len=data_str.length()/2;
            if(len < 1) {
                return nullptr;
            }
            uint8_t *data=(uint8_t*)malloc(len+1);
            data[len]='\0';
            if(!parse_key(data,json_data,len,"data")) {
                free(data);
                return nullptr;
            }
            command = command::factory::makeCommand(*data);
            if(!command) {
                throw CommandException(ErrorCodes::Code::eCommandParseError, "Failed to decode data field");
            }
            if(len < command->getDataSize()) {
                free(data);
                throw CommandException(ErrorCodes::Code::eCommandParseError, "Failed to decode data field");
            }
            command->setData((char*)data);
            free(data);
        } else {
          return nullptr;
        }
    }
    else if(!run.compare(txsname[TXSTYPE_BRO])) {
        checkUnusedFields(pt, "message,message_ascii");

        boost::optional<std::string> json_text_hex=pt.get_optional<std::string>("message");
        boost::optional<std::string> json_text_asci=pt.get_optional<std::string>("message_ascii");
        std::string text;

        if(json_text_hex && json_text_asci) {
            throw CommandException(ErrorCodes::Code::eCommandParseError, "Cannot use both fields: message, message_ascii");
        } else if(json_text_hex) {
            std::string text_hex=json_text_hex.get();
            text2key(text_hex, text);
        }
        else if (json_text_asci) {
            text=json_text_asci.get();
        }
        if (text.empty()) {
            throw CommandException(ErrorCodes::Code::eCommandParseError, "Required field is missing: message");
        } else {
            command = std::make_unique<BroadcastMsg>(sts_bank, sts_user, sts.msid, text.length(), text.c_str(), now);
        }
    }
    else if(!run.compare(txsname[TXSTYPE_MPT])) {
        std::vector<SendAmountTxnRecord> transactions;
        int i=0;
        for(boost::property_tree::ptree::value_type &wire : pt.get_child("wires")) {

            uint16_t toNode;
            uint32_t toUser;
            int64_t amount;
            if(!sts.parse_acnt(toNode,toUser,wire.first,"wires[" + std::to_string(i) + "]")) {
                return nullptr;
            }
            if(!parse_amount(amount,wire.second.get_value<std::string>())) {
                throw CommandException(ErrorCodes::Code::eCommandParseError, "Amount parse error wires[" + wire.first + "]");
            }
            transactions.push_back(SendAmountTxnRecord(toNode, toUser, amount));
            i++;
        }

        if (transactions.empty()) {
            return nullptr;
        }
        checkUnusedFields(pt, "wires");
        command = std::make_unique<SendMany>(sts_bank, sts_user, sts.msid, transactions, now);
    }
    else if(!run.compare(txsname[TXSTYPE_PUT])) {
        boost::optional<std::string> json_info=pt.get_optional<std::string>("message"); // TXSTYPE_PUT only
        if(json_info && !parse_key(to_info,json_info,32,"message")) {
            return nullptr;
        }
        pt.get<std::string>("address");
        pt.get<std::string>("amount");
        checkUnusedFields(pt, "address,amount,message");
        command = std::make_unique<SendOne>(sts_bank,sts_user,sts.msid, to_bank, to_user, to_mass, to_info, now);
    }
    else if(!run.compare(txsname[TXSTYPE_USR])) {
        if(!to_bank) {
            to_bank=sts_bank;
        }

        if(json_pkey) {
            pt.get<std::string>("confirm");
            if(ed25519_sign_open((uint8_t*)nullptr, 0, to_pkey , to_confirm) == -1) {
                return nullptr;
            }
        }

        checkUnusedFields(pt, "node,confirm,public_key");
        command = std::make_unique<CreateAccount>(sts_bank, sts_user, sts.msid, to_bank, now);

        CreateAccount* keycommand = dynamic_cast<CreateAccount*>(command.get());
        keycommand->setPublicKey(json_pkey ? to_pkey : sts.pk);
    }
    else if(!run.compare(txsname[TXSTYPE_BNK])) {
        checkUnusedFields(pt, "");
        command = std::make_unique<CreateNode>(sts_bank, sts_user, sts.msid, now);
    }
    else if(!run.compare(txsname[TXSTYPE_SAV])) {
        checkUnusedFields(pt, "");
        command = std::make_unique<LogAccount>(sts_bank, sts_user, sts.msid, now);
    }
    else if(!run.compare(txsname[TXSTYPE_GET])) {
        checkUnusedFields(pt, "address");
        pt.get<std::string>("address");
        command = std::make_unique<RetrieveFunds>(sts_bank, sts_user, sts.msid, now, to_bank, to_user);
    }
    else if(!run.compare(txsname[TXSTYPE_KEY])) {
        pt.get<std::string>("confirm");
        pt.get<std::string>("public_key");
        checkUnusedFields(pt, "confirm,public_key");
        command = std::make_unique<SetAccountKey>(sts_bank, sts_user, sts.msid, now, to_pkey, to_confirm);
    }
    else if(!run.compare(txsname[TXSTYPE_BKY])) {
        checkUnusedFields(pt, "node,public_key");
        command = std::make_unique<ChangeNodeKey>(sts_bank, sts_user, sts.msid, to_bank, now, to_pkey);
    }
    else if(!run.compare(txsname[TXSTYPE_SUS])) {
        checkUnusedFields(pt, "node,status,address");
        uint16_t  to_status=pt.get<uint16_t>("status");
        command = std::make_unique<SetAccountStatus>(sts_bank, sts_user, sts.msid, now, to_bank, to_user, to_status);
    }
    else if(!run.compare(txsname[TXSTYPE_SBS])) {
        uint32_t  to_status=pt.get<uint32_t>("status");
        checkUnusedFields(pt, "node,status");
        command = std::make_unique<SetNodeStatus>(sts_bank, sts_user, sts.msid, now, to_bank, to_status);
    }
    else if(!run.compare(txsname[TXSTYPE_UUS])) {
        uint16_t  to_status=pt.get<uint16_t>("status");
        checkUnusedFields(pt, "node,status,address");
        command = std::make_unique<UnsetAccountStatus>(sts_bank, sts_user, sts.msid, now, to_bank, to_user, to_status);
    }
    else if(!run.compare(txsname[TXSTYPE_UBS])) {
        uint32_t  to_status=pt.get<uint32_t>("status");
        checkUnusedFields(pt, "node,status");
        command = std::make_unique<UnsetNodeStatus>(sts_bank, sts_user, sts.msid, now, to_bank, to_status);
    }
    else if (!run.compare(txsname[TXSTYPE_GFI])) {
        checkUnusedFields(pt, "type");
        command = std::make_unique<GetFields>(txn_type.c_str());
    }
    else {
        throw CommandException(ErrorCodes::Code::eCommandParseError, "run not defined or unknown");
    }

    if(command) {
        if(sts.signature_provided) {
          memcpy(command->getSignature(), to_signature, command->getSignatureSize());
        } else {
            if(run == "decode_raw") {
                bzero(command->getSignature(), command->getSignatureSize());
            } else {
                command->sign(sts.ha.data(), sts.sk, sts.pk);
            }
        }
    }

    if(!run.compare(txsname[TXSTYPE_KEY]) && command) {
        SetAccountKey* keycommand = dynamic_cast<SetAccountKey*>(command.get());

        if(keycommand && !keycommand->checkPubKeySignaure()) {
            throw CommandException(ErrorCodes::Code::eCommandParseError, "bad new KEY or empty string signature");
        }
    }

    if(extraData.length() > 0) {
        command->setExtraData(extraData);
    }

    return command;
}
