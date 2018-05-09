#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <openssl/rand.h>
#include <sstream>
#include <stack>
#include <vector>
#include <iostream>
#include <fcntl.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/optional/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
//#include <boost/serialization/list.hpp>
//#include <boost/serialization/vector.hpp>
#include <boost/shared_ptr.hpp>

#include "default.hpp"
#include "hash.hpp"
#include "user.hpp"
#include "settings.hpp"
#include "message.hpp"
#include "servers.hpp"
#include "networkclient.h"
#include "helper/ascii.h"
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
#include "helper/hash.h"
#include "helper/json.h"
#include "helper/hlog.h"
#include "helper/txsname.h"

using namespace Helper;

boost::mutex flog; //to compile LOG()
FILE* stdlog=stderr; //to compile LOG()

// expected format:  :to_bank,to_user,to_mass:to_bank,to_user,to_mass:to_bank,to_user,to_mass ... ;
// the list must terminate with ';'
bool parse_mpt(std::string& text,uint32_t& to_bank,const char* line,int end) {
    int pos=3;
    int add=0;
    std::set<uint64_t> out;
    union {
        uint64_t big;
        uint32_t small[2];
    } to;
    //to.small[1]=0;
    for(; pos<end; pos+=add) {
        char to_acct[2+4+8];
        uint32_t& tuser=to.small[0];
        uint32_t& tbank=to.small[1];
        int64_t tmass;
        if(line[pos]==';') {
            return(true);
        }
        if(3!=sscanf(line+pos,":%X,%X,%lX%n",&tbank,&tuser,&tmass,&add) || !add) {
            return(false);
        }
        if(out.find(to.big)!=out.end()) {
            fprintf(stderr,"ERROR: duplicate target: %04X:%08X\n",tbank,tuser);
            return(false);
        }
        out.insert(to.big);
        memcpy(to_acct+0,&tbank,2);
        memcpy(to_acct+2,&tuser,4);
        memcpy(to_acct+6,&tmass,8);
        text.append(to_acct,2+4+8);
        to_bank++;
    }
    return(false);
}

//bool parse_bank(uint16_t& to_bank,boost::optional<std::string>& json_bank)
//{ std::string str_bank=json_bank.get();
/*bool parse_bank(uint16_t& to_bank,std::string str_bank)
{ char *endptr;
  if(str_bank.length()!=4){
    fprintf(stderr,"ERROR: parse_bank(%s) bad length (required 4)\n",str_bank.c_str());
    return(false);}
  errno=0;
  to_bank=(uint16_t)strtol(str_bank.c_str(),&endptr,16);
  if(errno || endptr==str_bank.c_str() || *endptr!='\0'){
    fprintf(stderr,"ERROR: parse_bank(%s)\n",str_bank.c_str());
    perror("ERROR: strtol");
    return(false);}
  return(true);
}*/

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

bool parse_key(uint8_t* to_key,boost::optional<std::string>& json_key,int len) {
    std::string str_key=json_key.get();
    if((int)str_key.length()!=2*len) {
        fprintf(stderr,"ERROR: parse_key(%s) bad string length (required %d)\n",str_key.c_str(),2*len);
        return(false);
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

usertxs_ptr run_json(settings& sts, const std::string& line ,int64_t& deduct,int64_t& fee, std::unique_ptr<IBlockCommand> &command) {
    uint16_t    to_bank=0;
    uint32_t    to_user=0;
    int64_t     to_mass=0;
    //uint64_t  to_info=0;
    uint32_t    to_from=0;
    uint8_t     to_info[32]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint8_t     to_pkey[32];
    uint8_t     to_sign[64];
    uint32_t    to_status=0;
    std::string txn_type{};
    uint32_t    now=time(NULL);
    uint32_t    to_block=now-(now%BLOCKSEC)-BLOCKSEC;
    usertxs_ptr txs=NULL;
    boost::property_tree::ptree pt;

    std::stringstream ss(line);

    try {
        //DLOG("%s\n", line.c_str());
        boost::property_tree::read_json(ss,pt);
    } catch (std::exception& e) {
        std::cerr << "RUN_JSON Exception: " << e.what() << "\n";
        return nullptr;
    }

    boost::optional<std::string> json_sign=pt.get_optional<std::string>("signature");
    if(json_sign && !parse_key(to_sign,json_sign,64)) {
        return(NULL);
    }
    boost::optional<std::string> json_pkey=pt.get_optional<std::string>("pkey");
    if(json_pkey && !parse_key(to_pkey,json_pkey,32)) {
        return(NULL);
    }
    boost::optional<std::string> json_hash=pt.get_optional<std::string>("hash");
    if(json_hash && !parse_key(sts.ha.data(),json_hash,32)) {
        return(NULL);
    }
    boost::optional<std::string> json_mass=pt.get_optional<std::string>("amount");
    if(json_mass && !parse_amount(to_mass,json_mass.get())) {
        return(NULL);
    }
    boost::optional<std::string> json_user=pt.get_optional<std::string>("id");
    if(json_user && !parse_user(to_user,json_user.get())) {
        return(NULL);
    }
    boost::optional<std::string> json_acnt=pt.get_optional<std::string>("address");
    if(json_acnt && !sts.parse_acnt(to_bank,to_user,json_acnt.get())) {
        return(NULL);
    }
    boost::optional<uint16_t> json_bank=pt.get_optional<uint16_t>("node");
    if(json_bank) {
        to_bank=json_bank.get();
    }
    boost::optional<uint32_t> json_msid=pt.get_optional<uint32_t>("msid");
    if(json_msid) {
        sts.msid=json_msid.get();
    }
    boost::optional<uint32_t> json_status=pt.get_optional<uint32_t>("status");
    if(json_status) {
        to_status=json_status.get();
    }
    boost::optional<std::string> json_block=pt.get_optional<std::string>("block");
    if(json_block) {
        to_block=hexdec(json_block.get());
    }
    boost::optional<std::string> json_type=pt.get_optional<std::string>("type");
    if (json_type) {
        txn_type = json_type.get();
    }

    deduct=0;
    fee=0;
    std::string run=pt.get<std::string>("run");

    if(!run.compare("get_me")) {
        command.reset( new GetAccount(sts.bank,sts.user,sts.bank,sts.user,now));
    } else if(!run.compare(txsname[TXSTYPE_INF])) {
        if(!to_bank && !to_user) { // no target account specified
            command.reset( new GetAccount(sts.bank,sts.user,sts.bank,sts.user,now));
        } else {
            command.reset( new GetAccount(sts.bank,sts.user,to_bank,to_user,now));
        }
    } else if(!run.compare(txsname[TXSTYPE_LOG])) {
        boost::optional<uint32_t> json_from=pt.get_optional<uint32_t>("from"); //FIXME, decide HEX or DEC
        if(json_from){
            to_from=json_from.get();
        }
        command.reset(new GetLog(sts.bank, sts.user, to_from, txn_type.c_str()));
    } else if(!run.compare(txsname[TXSTYPE_BLG])) {
        boost::optional<std::string> json_from=pt.get_optional<std::string>("from");
        if(json_from){
            to_from=hexdec(json_from.get());
        }
        command.reset(new GetBroadcastMsg(sts.bank, sts.user, to_from, now));
    } else if(!run.compare(txsname[TXSTYPE_BLK])) {
        boost::optional<std::string> json_from=pt.get_optional<std::string>("from");
        if(json_from){
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
        } //                                    !!!!!!!             !!!!!
        txs=boost::make_shared<usertxs>(TXSTYPE_BLK,sts.bank,sts.user,to_from,now,to_bank,to_to,to_mass,to_info,(const char*)NULL);
    } else if(!run.compare(txsname[TXSTYPE_TXS])) {
        uint32_t node_msid=0;
        uint32_t node_mpos=0;
        boost::optional<std::string> json_txid=pt.get_optional<std::string>("txid");
        if(json_txid && !sts.parse_txid(to_bank,node_msid,node_mpos,json_txid.get())) {
            return(NULL);
        }
        command.reset(new GetTransaction(sts.bank, sts.user, to_bank, node_msid, node_mpos, now));
    } else if(!run.compare(txsname[TXSTYPE_VIP])) {
        boost::optional<std::string> json_viphash=pt.get_optional<std::string>("viphash");
        if(json_viphash && !parse_key(to_info,json_viphash,32)) {
            return(NULL);
        } //                                           !!!!!!!!                             !!!!!!!
        txs=boost::make_shared<usertxs>(TXSTYPE_VIP,sts.bank,sts.user,to_block,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);
    } else if(!run.compare(txsname[TXSTYPE_SIG])) { //                 !!!!!!!!
        txs=boost::make_shared<usertxs>(TXSTYPE_SIG,sts.bank,sts.user,to_block,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);
    } else if(!run.compare(txsname[TXSTYPE_NDS])) { //                 !!!!!!!!
        command.reset(new GetBlock(sts.bank, sts.user, to_block, now));
    } else if(!run.compare(txsname[TXSTYPE_NOD])) { //                 !!!!!!!!
        command.reset(new GetAccounts(sts.bank, sts.user, to_block, to_bank, now));
    } else if(!run.compare(txsname[TXSTYPE_MGS])) { //                 !!!!!!!!
        command.reset(new GetMessageList(sts.bank, sts.user, to_block, now));
//        txs=boost::make_shared<usertxs>(TXSTYPE_MGS,sts.bank,sts.user,to_block,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);
    } else if(!run.compare(txsname[TXSTYPE_MSG])) {
        uint32_t to_node_msid=0;
        boost::optional<uint32_t> json_node_msid=pt.get_optional<uint32_t>("node_msid");
        if(json_node_msid) {
            to_node_msid=json_node_msid.get();
        } //                      !!!!!!!!             !!!!!!!!!!!!
        command.reset(new GetMessage(sts.bank, sts.user, to_block, to_bank, to_node_msid, now));
//        txs=boost::make_shared<usertxs>(TXSTYPE_MSG,sts.bank,sts.user,to_block,now,to_bank,to_node_msid,to_mass,to_info,(const char*)NULL);
    } else if(!run.compare("send_again")) {
        boost::optional<std::string> json_data=pt.get_optional<std::string>("data");
        if(json_data) {
            std::string data_str=json_data.get();
            int len=data_str.length()/2;
            uint8_t *data=(uint8_t*)malloc(len+1);
            data[len]='\0';
            if(!parse_key(data,json_data,len)) {
                free(data);
                return(NULL);
            }
            command = command::factory::makeCommand(*data);
            command->setData((char*)data);
            free(data);
        }
        return(NULL);
    } else if(!run.compare(txsname[TXSTYPE_BRO])) {
        boost::optional<std::string> json_text_hex=pt.get_optional<std::string>("message");
        boost::optional<std::string> json_text_asci=pt.get_optional<std::string>("message_ascii");
        std::string text;
        if(json_text_hex) {
            std::string text_hex=json_text_hex.get();
            text2key(text_hex, text);
        } else if (json_text_asci) {
            text=json_text_asci.get();
        }
        if (!text.empty()) {
            command.reset( new BroadcastMsg(sts.bank, sts.user, sts.msid, text.length(), text.c_str(), now));
            fee = command->getFee();
        }
    } else if(!run.compare(txsname[TXSTYPE_MPT])) {
        std::vector<SendAmountTxnRecord> transactions;
        for(boost::property_tree::ptree::value_type &wire : pt.get_child("wires")) {
            uint16_t toNode;
            uint32_t toUser;
            int64_t amount;
            if(!sts.parse_acnt(toNode,toUser,wire.first)) {
                return(NULL);
            }
            if(!parse_amount(amount,wire.second.get_value<std::string>())) {
                return(NULL);
            }
            transactions.push_back(SendAmountTxnRecord(toNode, toUser, amount));
        }

        if (transactions.empty()) {
            return(NULL);
        }

        command.reset(new SendMany(sts.bank, sts.user, sts.msid, transactions, now));
        fee = command->getFee();
        deduct = command->getDeduct();
    } else if(!run.compare(txsname[TXSTYPE_PUT])) {
        boost::optional<std::string> json_info=pt.get_optional<std::string>("message"); // TXSTYPE_PUT only
        if(json_info && !parse_key(to_info,json_info,32)) {
            return(NULL);
        }
        command.reset(new SendOne(sts.bank,sts.user,sts.msid, to_bank, to_user, to_mass, to_info, now));
        fee = command->getFee();
        deduct = command->getDeduct();
    } else if(!run.compare(txsname[TXSTYPE_USR])) {
        if(!to_bank) {
            to_bank=sts.bank;
        }
        command.reset(new CreateAccount(sts.bank, sts.user, sts.msid, to_bank, now));
        deduct = command->getDeduct();
        fee = command->getFee();
    } else if(!run.compare(txsname[TXSTYPE_BNK])) {
        command.reset( new CreateNode(sts.bank, sts.user, sts.msid, now));
        deduct = command->getDeduct();
        fee = command->getFee();
    } else if(!run.compare(txsname[TXSTYPE_SAV])) {
        txs=boost::make_shared<usertxs>(TXSTYPE_SAV,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);
        fee=TXS_SAV_FEE;
    } else if(!run.compare(txsname[TXSTYPE_GET])) {
        txs=boost::make_shared<usertxs>(TXSTYPE_GET,sts.bank,sts.user,sts.msid,now,to_bank,to_user,to_mass,to_info,(const char*)NULL);
        fee=TXS_GET_FEE;
    } else if(!run.compare(txsname[TXSTYPE_KEY])) {
        command.reset( new SetAccountKey(sts.bank, sts.user, sts.msid, now, to_pkey, to_sign));
        fee=TXS_KEY_FEE;
    } else if(!run.compare(txsname[TXSTYPE_BKY])) {
        command.reset(new ChangeNodeKey(sts.bank, sts.user, sts.msid, to_bank, now, to_pkey));
        fee = command->getFee();
    }

    else if(!run.compare(txsname[TXSTYPE_SUS])) {
        txs=boost::make_shared<usertxs>(TXSTYPE_SUS,sts.bank,sts.user,sts.msid,now,to_bank,to_user,(uint64_t)to_status,to_info,(const char*)to_pkey);
        fee=TXS_SUS_FEE;
    } else if(!run.compare(txsname[TXSTYPE_SBS])) {
        txs=boost::make_shared<usertxs>(TXSTYPE_SBS,sts.bank,sts.user,sts.msid,now,to_bank,to_user,(uint64_t)to_status,to_info,(const char*)to_pkey);
        fee=TXS_SBS_FEE;
    } else if(!run.compare(txsname[TXSTYPE_UUS])) {
        txs=boost::make_shared<usertxs>(TXSTYPE_UUS,sts.bank,sts.user,sts.msid,now,to_bank,to_user,(uint64_t)to_status,to_info,(const char*)to_pkey);
        fee=TXS_UUS_FEE;
    } else if(!run.compare(txsname[TXSTYPE_UBS])) {
        txs=boost::make_shared<usertxs>(TXSTYPE_UBS,sts.bank,sts.user,sts.msid,now,to_bank,to_user,(uint64_t)to_status,to_info,(const char*)to_pkey);
        fee=TXS_UBS_FEE;
    }

    else {
        fprintf(stderr,"ERROR: run not defined or unknown\n");
        return(NULL);
    }

    //if(txs==NULL){
    //  return(NULL);
    //}

    if(txs) {
        txs->sign(sts.ha.data(),sts.sk,sts.pk);
    }

    if(command) {
        command->sign(sts.ha.data(), sts.sk, sts.pk);
    }

    if(!run.compare(txsname[TXSTYPE_KEY]) && command) {
        SetAccountKey* keycommand = dynamic_cast<SetAccountKey*>(command.get());

        if(keycommand) {
            if( !keycommand->checkPubKeySignaure() ) {
                fprintf(stderr,"ERROR, bad new KEY empty string signature1\n");
                command.reset();
                //eturn nullptr;
            }
        } else if(txs && txs->sign2(to_sign)) {
            fprintf(stderr,"ERROR, bad new KEY empty string signature2\n");
            return(NULL);
        }
    }
    return txs;
}

int check_csum(user_t& u,uint16_t peer,uint32_t uid) {
    hash_t csum;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,&u,sizeof(user_t)-4*sizeof(uint64_t));
    SHA256_Update(&sha256,&peer,sizeof(uint16_t));
    SHA256_Update(&sha256,&uid,sizeof(uint32_t));
    SHA256_Final(csum,&sha256);
    return(memcmp(csum,u.csum,sizeof(hash_t)));
}


void print_log(boost::property_tree::ptree& pt,settings& sts) {
    char filename[64];
    sprintf(filename,"log/%04X_%08X.bin",sts.bank,sts.user);
    int fd=open(filename,O_RDONLY);
    if(fd<0) {
        fprintf(stderr,"ERROR, failed to open log file %s\n",filename);
        return;
    }
    if(sts.lastlog>1) { //guess a good starting position
        off_t end=lseek(fd,0,SEEK_END);
        if(end>0) {
            off_t start=end;
            off_t tseek=0;
            while((start=lseek(fd,(start>(off_t)(sizeof(log_t)*32)?-sizeof(log_t)*32-tseek:-start-tseek),SEEK_CUR))>0) {
                uint32_t ltime=0;
                tseek=read(fd,&ltime,sizeof(uint32_t));
                if(ltime<sts.lastlog-1) { // tollerate 1s difference
                    lseek(fd,-tseek,SEEK_CUR);
                    break;
                }
            }
        }
    }
    log_t ulog;
    boost::property_tree::ptree logtree;
    while(read(fd,&ulog,sizeof(log_t))==sizeof(log_t)) {
        if(ulog.time<sts.lastlog) {
            //fprintf(stderr,"SKIPP %d [%08X]\n",ulog.time,ulog.time);
            continue;
        }
        char info[65];
        info[64]='\0';
        ed25519_key2text(info,ulog.info,32);
        boost::property_tree::ptree logentry;
        uint16_t suffix=sts.crc_acnt(ulog.node,ulog.user);
        char acnt[19];
        uint16_t txst=ulog.type&0xFF;
        sprintf(acnt,"%04X-%08X-%04X",ulog.node,ulog.user,suffix);
        logentry.put("time",ulog.time);
        logentry.put("date",mydate(ulog.time));
        logentry.put("type_no",ulog.type);
        // FIXME: properly flag confirmed transactions, which will not be rolled back
        logentry.put("confirmed", 1);
        if(txst<TXSTYPE_MAX) {
            logentry.put("type",logname[txst]);
        }
        if(ulog.type & 0x4000) { //errors
            if(txst==TXSTYPE_NON) { //node start
                logentry.put("account.error","logerror");
                logentry.put("account.newtime",ulog.time);
                logentry.put("account.newdate",mydate(ulog.time));
                logentry.put("account.badtime",ulog.nmid);
                logentry.put("account.baddate",mydate(ulog.nmid));
                logentry.put("account.badblock",ulog.mpos);
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
        }
        if(ulog.type & 0x8000) { //incomming network transactions (responses) except _GET
            if(txst==TXSTYPE_NON) { //node start
                logentry.put("node_start_msid",ulog.nmid);
                logentry.put("node_start_block",ulog.mpos);
                int64_t weight;
                uint8_t hash[8];
                uint32_t lpath;
                uint32_t rpath;
                uint8_t pkey[6];
                uint16_t stat;
                memcpy(&weight,ulog.info+ 0,8);
                memcpy(   hash,ulog.info+ 8,8);
                memcpy( &lpath,ulog.info+16,4);
                memcpy( &rpath,ulog.info+20,4);
                memcpy(   pkey,ulog.info+24,6);
                memcpy(  &stat,ulog.info+30,2);
                char hash_hex[17];
                hash_hex[16]='\0';
                ed25519_key2text(hash_hex,hash,8);
                char pkey_hex[13];
                pkey_hex[12]='\0';
                ed25519_key2text(pkey_hex,pkey,6);
                logentry.put("account.balance",print_amount(weight));
                logentry.put("account.local_change",lpath);
                logentry.put("account.remote_change",rpath);
                logentry.put("account.hash_prefix_8",hash_hex);
                logentry.put("account.public_key_prefix_6",pkey_hex);
                logentry.put("account.status",stat);
                logentry.put("account.msid",ulog.umid);
                logentry.put("account.node",ulog.node);
                logentry.put("account.id",ulog.user);
                logentry.put("dividend",print_amount(ulog.weight));
                uint16_t suffix=sts.crc_acnt(ulog.node,ulog.user);
                char acnt[19]="";
                sprintf(acnt,"%04X-%08X-%04X",ulog.node,ulog.user,suffix);
                logentry.put("account.address",acnt);
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_DIV) { //dividend
                logentry.put("node_msid",ulog.nmid);
                logentry.put("node_block",ulog.mpos);
                logentry.put("dividend",print_amount(ulog.weight));
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_FEE) { //bank profit
                logentry.put("profit",print_amount(ulog.weight));
                if(ulog.nmid) { // bank profit on transactions
                    logentry.put("node",ulog.node);
                    logentry.put("node_msid",ulog.nmid);
                    if(ulog.nmid==sts.bank) {
                        int64_t fee;
                        int64_t div;
                        //int64_t put; //FIXME, useless !!!
                        memcpy(&fee,ulog.info+ 0,8);
                        memcpy(&div,ulog.info+ 8,8);
                        //memcpy(&put,ulog.info+16,8); //FIXME, useless !!!
                        logentry.put("profit_fee",print_amount(fee));
                        logentry.put("profit_div",print_amount(div));
                        //logentry.put("profit_put",print_amount(put)); //FIXME, useless !!!
                    }
                } else { // bank profit at block end
                    logentry.put("node_block",ulog.mpos);
                    int64_t div;
                    int64_t usr;
                    int64_t get;
                    int64_t fee;
                    memcpy(&div,ulog.info+ 0,8);
                    memcpy(&usr,ulog.info+ 8,8);
                    memcpy(&get,ulog.info+16,8);
                    memcpy(&fee,ulog.info+24,8);
                    logentry.put("profit_div",print_amount(div));
                    logentry.put("profit_usr",print_amount(usr));
                    logentry.put("profit_get",print_amount(get));
                    logentry.put("fee",print_amount(fee));
                }
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_UOK) { //creare remote account
                logentry.put("node",ulog.node);
                logentry.put("node_block",ulog.mpos);
                if(ulog.user) {
                    logentry.put("account",ulog.user);
                    logentry.put("address",acnt);
                    if(ulog.umid) {
                        logentry.put("request","accepted");
                    } else {
                        logentry.put("request","late");
                    }
                } else {
                    logentry.put("request","failed");
                    logentry.put("amount",print_amount(ulog.weight));
                }
                logentry.put("public_key",info);
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_BNK) {
                logentry.put("node_block",ulog.mpos);
                if(ulog.node) {
                    logentry.put("node",ulog.node);
                    logentry.put("request","accepted");
                } else {
                    logentry.put("request","failed");
                    logentry.put("amount",print_amount(ulog.weight));
                }
                logentry.put("public_key",info);
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
        }
        logentry.put("node",ulog.node);
        logentry.put("account",ulog.user);
        logentry.put("address",acnt);
        if(!ulog.nmid) {
            logentry.put("node_block",ulog.mpos);
        } else {
            logentry.put("node_msid",ulog.nmid);
            logentry.put("node_mpos",ulog.mpos);
            logentry.put("account_msid",ulog.umid);
        }
        logentry.put("amount",print_amount(ulog.weight));
        //FIXME calculate fee
        if(txst==TXSTYPE_PUT) {
            int64_t amass=std::abs(ulog.weight);
            if(ulog.node==sts.bank) {
                logentry.put("sender_fee",print_amount(TXS_PUT_FEE(amass)));
            } else {
                logentry.put("sender_fee",print_amount(TXS_PUT_FEE(amass)+TXS_LNG_FEE(amass)));
            }
            logentry.put("message",info);
        } else {
            int64_t weight;
            int64_t deduct;
            int64_t fee;
            uint16_t stat;
            uint8_t key[6];
            memcpy(&weight,ulog.info+ 0,8);
            memcpy(&deduct,ulog.info+ 8,8);
            memcpy(   &fee,ulog.info+16,8);
            memcpy(  &stat,ulog.info+24,2);
            memcpy(    key,ulog.info+26,6);
            char key_hex[13];
            key_hex[12]='\0';
            ed25519_key2text(key_hex,key,6);
            logentry.put("sender_balance",print_amount(weight));
            logentry.put("sender_amount",print_amount(deduct));
            if(txst==TXSTYPE_MPT) {
                if(ulog.node==sts.bank) {
                    logentry.put("sender_fee",print_amount(TXS_MPT_FEE(ulog.weight)+(key[5]?TXS_MIN_FEE:0)));
                } else {
                    logentry.put("sender_fee",
                                 print_amount(TXS_MPT_FEE(ulog.weight)+TXS_LNG_FEE(ulog.weight)+(key[5]?TXS_MIN_FEE:0)));
                }
                logentry.put("sender_fee_total",print_amount(fee));
                key_hex[2*5]='\0';
                logentry.put("sender_public_key_prefix_5",key_hex);
            } else {
                logentry.put("sender_fee",print_amount(fee));
                logentry.put("sender_public_key_prefix_6",key_hex);
            }
            logentry.put("sender_status",stat);
        }
        char tx_id[64];
        if(ulog.type & 0x8000) {
            logentry.put("inout","in");
            sprintf(tx_id,"%04X:%08X:%04X",ulog.node,ulog.nmid,ulog.mpos);
        } else {
            logentry.put("inout","out");
            sprintf(tx_id,"%04X:%08X:%04X",sts.bank,ulog.nmid,ulog.mpos);
        }
        logentry.put("id",tx_id);
        logtree.push_back(std::make_pair("",logentry));
    }
    close(fd);
    pt.add_child("log",logtree);
}

void print_blg(char *blg,uint32_t len,uint32_t path,boost::property_tree::ptree& blogtree,settings& sts) {
    //boost::property_tree::ptree blogtree;
    usertxs utxs;
    for(uint8_t *p=(uint8_t*)blg; p<(uint8_t*)blg+len; p+=utxs.size+32+32+4+4) {
        boost::property_tree::ptree blogentry;
        blogentry.put("block_time",path);
        blogentry.put("block_date",mydate(path));
        if(!utxs.parse((char*)p) || *p!=TXSTYPE_BRO) {
            std::cerr<<"ERROR: failed to parse broadcast transaction\n";
            blogentry.put("ERROR","failed to parse transaction");
            blogtree.push_back(std::make_pair("",blogentry));
            break;
        }
        blogentry.put("node",utxs.abank);
        blogentry.put("account",utxs.auser);
        uint16_t suffix=sts.crc_acnt(utxs.abank,utxs.auser);
        char acnt[19];
        sprintf(acnt,"%04X-%08X-%04X",utxs.abank,utxs.auser,suffix);
        blogentry.put("address",acnt);
        blogentry.put("account_msid",utxs.amsid);
        blogentry.put("time",utxs.ttime);
        blogentry.put("date",mydate(utxs.ttime));
        //blogentry.put("length",utxs.bbank);
        //transaction
        char* data=(char*)malloc(2*txslen[TXSTYPE_BRO]+1);
        data[2*txslen[TXSTYPE_BRO]]='\0';
        ed25519_key2text(data,p,txslen[TXSTYPE_BRO]);
        blogentry.put("data",data);
        free(data);
        //message
        char* message=(char*)malloc(2*utxs.bbank+1);
        message[2*utxs.bbank]='\0';
        ed25519_key2text(message,(uint8_t*)utxs.broadcast((char*)p),utxs.bbank);
        blogentry.put("message",message);
        free(message);
        //signature
        char sigh[129];
        sigh[128]='\0';
        ed25519_key2text(sigh,(uint8_t*)utxs.broadcast((char*)p)+utxs.bbank,64);
        blogentry.put("signature",sigh);
        //hash
        char hash[65];
        hash[64]='\0';
        ed25519_key2text(hash,p+utxs.size,32);
        blogentry.put("input_hash",hash);
        //hash
        char pkey[65];
        pkey[64]='\0';
        ed25519_key2text(pkey,p+utxs.size+32,32);
        blogentry.put("public_key",pkey);
        //check signature
        if(utxs.wrong_sig((uint8_t*)p,(uint8_t*)p+utxs.size,(uint8_t*)p+utxs.size+32)) {
            blogentry.put("verify","failed");
        } else {
            blogentry.put("verify","passed");
        }
        //tx_id
        uint32_t nmid;
        uint32_t mpos;
        memcpy(&nmid,p+utxs.size+32+32,sizeof(uint32_t));
        memcpy(&mpos,p+utxs.size+32+32+sizeof(uint32_t),sizeof(uint32_t)); //FIXME, this should be uint16_t
        char tx_id[64];
        sprintf(tx_id,"%04X:%08X:%04X",utxs.abank,nmid,mpos);
        blogentry.put("node_msid",nmid);
        blogentry.put("node_mpos",mpos);
        blogentry.put("id",tx_id);
        //FIXME calculate fee
        blogentry.put("fee",print_amount(TXS_BRO_FEE(utxs.bbank)));
        blogtree.push_back(std::make_pair("",blogentry));
    }
}

//void print_blg(int fd,uint32_t path,boost::property_tree::ptree& pt)
void print_blg_file(int fd,uint32_t path,boost::property_tree::ptree& blogtree,settings& sts) {
    struct stat sb;
    fstat(fd,&sb);
    uint32_t len=sb.st_size;
    //pt.put("length",len);
    if(!len) {
        return;
    }
    char *blg=(char*)malloc(len);
    if(blg==NULL) {
        //pt.put("ERROR","broadcast file too large");
        fprintf(stderr,"ERROR, broadcast file malloc error (size:%d)\n",len);
        return;
    }
    if(read(fd,blg,len)!=len) {
        //pt.put("ERROR","broadcast file read error");
        fprintf(stderr,"ERROR, broadcast file read error\n");
        free(blg);
        return;
    }
    print_blg(blg,len,path,blogtree,sts);
    //pt.add_child("broadcast",blogtree);
    free(blg);
}

void out_log(boost::property_tree::ptree& logpt,uint16_t bank,uint32_t user) {
    char filename[64];
    std::ofstream outfile;
    mkdir("out",0755);
    sprintf(filename,"out/%04X_%08X.json",bank,user);
    outfile.open(filename,std::ios::out|std::ios::app);
    boost::property_tree::write_json(outfile,logpt,false);
    outfile.close();
}

void save_log(log_t* log,int len,uint32_t from,settings& sts) {
    char filename[64];
    mkdir("log",0755);
    sprintf(filename,"log/%04X_%08X.bin",sts.bank,sts.user);
    int fd=open(filename,O_RDWR|O_CREAT|O_APPEND,0644);
    if(fd<0) {
        fprintf(stderr,"ERROR, failed to open log file %s\n",filename);
        return;
    }
    std::vector<log_t> logv;
    off_t end=lseek(fd,0,SEEK_END);
    uint32_t maxlasttime=from;
    if(end>0) {
        off_t start=end;
        off_t tseek=0;
        while((start=lseek(fd,(start>(off_t)(sizeof(log_t)*32)?-sizeof(log_t)*32-tseek:-start-tseek),SEEK_CUR))>0) {
            uint32_t ltime=0;
            tseek=read(fd,&ltime,sizeof(uint32_t));
            if(ltime<from) { // tollerate 1s difference
                lseek(fd,sizeof(log_t)-sizeof(uint32_t),SEEK_CUR);
                break;
            }
        }
        log_t ulog;
        while(read(fd,&ulog,sizeof(log_t))==sizeof(log_t)) {
            if(ulog.time>=from) {
                if(ulog.time>maxlasttime) {
                    maxlasttime=ulog.time;
                }
                logv.push_back(ulog);
            }
        }
    }
    for(int l=0; l<len; l+=sizeof(log_t),log++) {
        if(log->time>=from && log->time<=maxlasttime) {
            for(auto ulog : logv) {
                if(!memcmp(&ulog,log,sizeof(log_t))) {
                    goto NEXT;
                }
            }
        }
        write(fd,log,sizeof(log_t));
NEXT:
        ;
    }
    close(fd);
}

bool node_connect(boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,boost::asio::ip::tcp::socket& socket) {
    static bool connected=false;
    boost::system::error_code error = boost::asio::error::host_not_found;
    try {
        if(!connected) {
            boost::asio::ip::tcp::resolver::iterator end;
            while (endpoint_iterator != end) {
                // std::cerr<<"CONNECTING to " << endpoint_iterator->endpoint().address().to_string() << "\n";
                socket.connect(*endpoint_iterator, error);
                if(!error) {
                    break;
                }
                socket.close();
                endpoint_iterator++;
            }
            if(error) {
                throw boost::system::system_error(error);
            }
            // std::cerr<<"CONNECTED\n";
            connected=true;
        } else {
            socket.connect(*endpoint_iterator, error);
            if(error) {
                throw boost::system::system_error(error);
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Connect Exception: " << e.what() << "\n";
        socket.close();
        return(false);
    }
    return(true);
}

void print_txs(boost::property_tree::ptree& pt,txspath_t& res,uint8_t* data) {
    char tx_id[64];
    sprintf(tx_id,"%04X:%08X:%04X",res.node,res.msid,res.tnum);
    pt.put("network_tx.id",&tx_id[0]);
    pt.put("network_tx.block_id",res.path);
    pt.put("network_tx.node_id",res.node);
    pt.put("network_tx.node_msid",res.msid);
    pt.put("network_tx.position",res.tnum);
    pt.put("network_tx.len",res.len);
    pt.put("network_tx.hash_path_len",res.hnum);
    char txstext[res.len*2+1];
    txstext[res.len*2]='\0';
    ed25519_key2text(txstext,data,res.len);
    pt.put("network_tx.hexstring",&txstext[0]);
    boost::property_tree::ptree hashpath;
    for(int i=0; i<res.hnum; i++) {
        char hashtext[65];
        hashtext[64]='\0';
        ed25519_key2text(hashtext,data+res.len+32*i,32);
        boost::property_tree::ptree hashstep;
        hashstep.put("",&hashtext[0]);
        hashpath.push_back(std::make_pair("",hashstep));
    }
    pt.add_child("network_tx.hashpath",hashpath);
    //parse transaction
    usertxs utxs;
    utxs.parse((char*)data);
//FIXME, add more data and improve formating
    pt.put("network_tx.ttype",utxs.ttype);
    pt.put("network_tx.abank",utxs.abank);
    pt.put("network_tx.auser",utxs.auser);
    pt.put("network_tx.amsid",utxs.amsid);
    pt.put("network_tx.ttime",utxs.ttime);
    pt.put("network_tx.bbank",utxs.bbank);
    pt.put("network_tx.buser",utxs.buser);
    pt.put("network_tx.amount",print_amount(utxs.tmass));
    char infotext[65];
    infotext[64]='\0';
    ed25519_key2text(infotext,utxs.tinfo,32);
    pt.put("network_tx.message",&infotext[0]);
    char signtext[129];
    signtext[128]='\0';
    ed25519_key2text(signtext,utxs.get_sig((char*)data),64);
    pt.put("network_tx.signature",&signtext[0]);
}

void print_blocktime(boost::property_tree::ptree& pt,uint32_t blocktime) {
    char blockhex[9];
    blockhex[8]='\0';
    sprintf(blockhex,"%08X",blocktime);
    pt.put("block_time_hex",blockhex);
    pt.put("block_time",blocktime);
}


void talk(boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,boost::asio::ip::tcp::socket& socket,settings& sts,usertxs_ptr txs,int64_t deduct,int64_t fee) { //len can be deduced from txstype
    char buf[0xff];
    boost::property_tree::ptree pt;
    boost::property_tree::ptree logpt;
    try {
        uint8_t hashout[32]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        char* tx_data=(char*)malloc(2*txs->size+1);
        tx_data[2*txs->size]='\0';
        ed25519_key2text(tx_data,txs->data,txs->size);

        std::cerr << tx_data;
        //create asa tx.data
        pt.put("tx.data",tx_data);
        logpt.put("tx.data",tx_data);
        free(tx_data);

        uint32_t now=time(NULL);
        now-=now%BLOCKSEC;
        pt.put("current_block_time",now);
        pt.put("previous_block_time",now-BLOCKSEC);

        if(txs->ttype<TXSTYPE_INF) {
            pt.put("tx.account_msid",sts.msid);
            logpt.put("tx.account_msid",sts.msid);
            char tx_user_hashin[65];
            tx_user_hashin[64]='\0';
            ed25519_key2text(tx_user_hashin,sts.ha.data(),32);
            pt.put("tx.account_hashin",tx_user_hashin);
            logpt.put("tx.account_hashin",tx_user_hashin);

            char tx_user_hashout[65];
            tx_user_hashout[64]='\0';
            SHA256_CTX sha256;
            SHA256_Init(&sha256);
            SHA256_Update(&sha256,txs->get_sig((char*)txs->data),64);
            SHA256_Final(hashout,&sha256);
            SHA256_Init(&sha256);
            SHA256_Update(&sha256,sts.ha.data(),32);
            SHA256_Update(&sha256,hashout,32);
            SHA256_Final(hashout,&sha256);
            ed25519_key2text(tx_user_hashout,hashout,32);
            pt.put("tx.account_hashout",tx_user_hashout);
            //FIXME calculate deduction and fee
            pt.put("tx.deduct",print_amount(deduct));
            pt.put("tx.fee",print_amount(fee));
        }

        if(sts.msid==1) {
            char tx_user_public_key[65];
            tx_user_public_key[64]='\0';
            ed25519_key2text(tx_user_public_key,sts.pk,32);
            logpt.put("tx.account_public_key",tx_user_public_key);
        }
        if(sts.drun) {
            boost::property_tree::write_json(std::cout,pt,sts.nice);
            return;
        }

        if(txs->ttype==TXSTYPE_BLG) { //FIXME, not tested !!!
            boost::property_tree::ptree blogtree;
            uint32_t path=txs->ttime;
            if(!path) {
                path=time(NULL)-BLOCKSEC;
            }
            path=path-(path%BLOCKSEC);
            char filename[64];
            sprintf(filename,"bro/%08X.bin",path);
            int fd=open(filename,O_RDONLY);
            if(fd>=0) {
                pt.put("log_file","archive");
                print_blg_file(fd,path,blogtree,sts);
                close(fd);
            } else {
                try {
                    if(txs->ttime && txs->ttime!=path) {
                        txs->change_time(path);
                        txs->sign(sts.ha.data(), sts.sk, sts.pk);
                    }
                    fprintf(stderr,"DEBUG request broadcast from %08X\n",path);
                    if(!node_connect(endpoint_iterator,socket)) {
                        std::cerr<<"ERROR connecting\n";
                        return;
                    }
                    boost::asio::write(socket,boost::asio::buffer(txs->data,txs->size));
                    uint32_t head[3];
                    size_t recvSize = boost::asio::read(socket,boost::asio::buffer(head,3*sizeof(uint32_t)));
                    if(3*sizeof(uint32_t) != recvSize) {
                        std::cerr<<"ERROR reading broadcast log length\n";
                        socket.close();
                        return;
                    }
                    uint32_t node_path=head[0];
                    uint32_t node_lpath=head[1];
                    int len=(int)head[2];
                    if(!len) {
                        fprintf(stderr,"WARNING broadcast for block %08X is empty\n",path);
                    } else {
                        char* blg=(char*)malloc(len);//last 4 bytes: the block time of the broadcast log file
                        if(blg==NULL) {
                            fprintf(stderr,"ERROR allocating %08X bytes\n",len);
                            close(fd);
                            socket.close();
                            return;
                        }
                        if(len!=(int)boost::asio::read(socket,boost::asio::buffer(blg,len))) { // exception will ...
                            std::cerr<<"ERROR reading broadcast log\n";
                            free(blg);
                            close(fd);
                            socket.close();
                            return;
                        }
                        print_blg(blg,len,node_path,blogtree,sts);
                        if(path==node_path && path<=node_lpath) {
                            pt.put("log_file","new");
                            mkdir("bro",0755);
                            fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0644);
                            if(fd>=0) {
                                write(fd,(char*)blg,len);
                                close(fd);
                            }
                        } else {
                            pt.put("log_file","pending");
                            path=node_path;
                        }
                        free(blg);
                    }
                    socket.close();
                } catch (std::exception& e) {
                    if(fd>=0) {
                        close(fd);
                    }
                    std::cerr << "Read Broadcast Exception: " << e.what() << "\n";
                    return;
                }
            }
            print_blocktime(pt,path);
            pt.add_child("broadcast",blogtree);
            boost::property_tree::write_json(std::cout,pt,sts.nice);
            return;
        }

        if(txs->ttype==TXSTYPE_TXS) {
            uint16_t svid=txs->bbank;
            uint32_t msid=txs->buser;
            uint16_t tnum=txs->amsid;
            uint8_t dir[6];
            memcpy(dir,&svid,2);
            memcpy(dir+2,&msid,4);
            char filename[128];
            sprintf(filename,"txs/%02X/%02X/%02X/%02X/%02X/%02X/%04X.txs",dir[1],dir[0],dir[5],dir[4],dir[3],dir[2],tnum);
            int fd=open(filename,O_RDONLY);
            if(fd<0) {
                //fprintf(stderr,"%s not found asking server\n",filename);
                if(!node_connect(endpoint_iterator,socket)) {
                    return;
                }
                //fprintf(stderr,"sending request %s\n",filename);
                boost::asio::write(socket,boost::asio::buffer(txs->data,txs->size));
                txspath_t res;
                //fprintf(stderr,"read txspath for %s\n",filename);
                boost::asio::read(socket,boost::asio::buffer(&res,sizeof(txspath_t)));
                if(!res.len) {
                    fprintf(stderr,"ERROR, failed to read transaction path for txid %04X:%08X:%04X\n",svid,msid,tnum);
                    socket.close();
                    return;
                }
                uint8_t data[res.len+res.hnum*32];
                //fprintf(stderr,"read data (%d) for %s\n",res.len,filename);
                boost::asio::read(socket,boost::asio::buffer(data,res.len));
                //fprintf(stderr,"read hashes (%d) for %s\n",res.hnum,filename);
                boost::asio::read(socket,boost::asio::buffer(data+res.len,res.hnum*32));
                //fprintf(stderr,"close socket for %s\n",filename);
                socket.close();
                if(!res.path) {
                    fprintf(stderr,"ERROR, got empty block for txid %04X:%08X:%04X\n",svid,msid,tnum);
                    return;
                }
                if(res.node!=svid || res.msid!=msid || res.tnum!=tnum) {
                    fprintf(stderr,"ERROR, got wrong transaction %04X%08X%04X for txid %04X:%08X:%04X\n",
                            res.node,res.msid,res.tnum,svid,msid,tnum);
                    return;
                }
                servers block;
                block.now=res.path;
                if(!block.load_nowhash()) {
                    fprintf(stderr,"ERROR, failed to load hash for block %08X\n",res.path);
                    return;
                }
                hash_t nhash;
                uint16_t* hashsvid=(uint16_t*)nhash;
                uint32_t* hashmsid=(uint32_t*)(&nhash[4]);
                uint16_t* hashtnum=(uint16_t*)(&nhash[8]);
                SHA256_CTX sha256;
                SHA256_Init(&sha256);
                SHA256_Update(&sha256,data,res.len);
                SHA256_Final(nhash,&sha256);
                *hashsvid^=svid;
                *hashmsid^=msid;
                *hashtnum^=tnum;
                if(memcmp(nhash,data+res.len,32)) {
                    fprintf(stderr,"ERROR, failed to confirm first hash for txid %04X:%08X:%04X\n",svid,msid,tnum);
                    return;
                }
                std::vector<hash_s> hashes(res.hnum);
                for(int i=0; i<res.hnum; i++) {
                    hashes[i]=*(hash_s*)(data+res.len+32*i);
                }
                hashtree tree;
                tree.hashpathrun(nhash,hashes);
                if(memcmp(nhash,block.nowhash,32)) {
                    fprintf(stderr,"ERROR, failed to confirm nowhash for txid %04X:%08X:%04X\n",svid,msid,tnum);
                    return;
                }
                //store confirmation and transaction in repository
                sprintf(filename,"txs/%02X",dir[1]);
                mkdir(filename,0755);
                sprintf(filename,"txs/%02X/%02X",dir[1],dir[0]);
                mkdir(filename,0755);
                sprintf(filename,"txs/%02X/%02X/%02X",dir[1],dir[0],dir[5]);
                mkdir(filename,0755);
                sprintf(filename,"txs/%02X/%02X/%02X/%02X",dir[1],dir[0],dir[5],dir[4]);
                mkdir(filename,0755);
                sprintf(filename,"txs/%02X/%02X/%02X/%02X/%02X",dir[1],dir[0],dir[5],dir[4],dir[3]);
                mkdir(filename,0755);
                sprintf(filename,"txs/%02X/%02X/%02X/%02X/%02X/%02X",dir[1],dir[0],dir[5],dir[4],dir[3],dir[2]);
                mkdir(filename,0755);
                sprintf(filename,"txs/%02X/%02X/%02X/%02X/%02X/%02X/%04X.txs",
                        dir[1],dir[0],dir[5],dir[4],dir[3],dir[2],tnum);
                int ld=open(filename,O_WRONLY|O_CREAT,0644);
                if(ld<0) {
                    fprintf(stderr,"ERROR opening %s\n",filename);
                } else {
                    write(ld,&res,sizeof(txspath_t));
                    write(ld,data,res.len+res.hnum*32);
                    close(ld);
                }
                print_txs(pt,res,data);
            } else {
                txspath_t res;
                read(fd,&res,sizeof(txspath_t));
                uint8_t data[res.len+res.hnum*32];
                read(fd,data,res.len+res.hnum*32);
                close(fd);
                print_txs(pt,res,data);
            }
            boost::property_tree::write_json(std::cout,pt,sts.nice);
            return;
        }

        if(!node_connect(endpoint_iterator,socket)) {
            return;
        }

        boost::asio::write(socket,boost::asio::buffer(txs->data,txs->size));

        if(txs->ttype==TXSTYPE_BLK) {
            boost::property_tree::ptree blockElement;
            boost::property_tree::ptree blockChild;

            fprintf(stderr,"PROCESSING BLOCKS\n");
            hash_t viphash;
            hash_t oldhash;
            servers block;
            char* firstkeys=NULL; //FIXME, free !!!
            int firstkeyslen=0;
            //read first vipkeys if needed
            if(!txs->amsid || txs->amsid == txs->buser) { // amsid = to_from
                mkdir("blk",0755);
                mkdir("vip",0755);
                mkdir("txs",0755);
                boost::asio::read(socket,boost::asio::buffer(&firstkeyslen,4));
                if(firstkeyslen<=0) {
                    fprintf(stderr,"ERROR, failed to read VIP keys for first hash\n");
                    socket.close();
                    return;
                } else {
                    fprintf(stderr,"READ VIP keys for first hash (len:%d)\n",firstkeyslen);
                }
                firstkeys=(char*)std::malloc(4+firstkeyslen);
                memcpy(firstkeys,&firstkeyslen,4); //probably not needed
                boost::asio::read(socket,boost::asio::buffer(firstkeys+4,firstkeyslen));
            }
            //read headers
            int num;
            boost::asio::read(socket,boost::asio::buffer(&num,4));
            if(num<=0) {
                fprintf(stderr,"ERROR, failed to read blocks since %08X\n",txs->amsid);
                socket.close();
                return;
            } else {
                fprintf(stderr,"READ %d blocks since %08X\n",num,txs->amsid);
            }
            pt.put("updated_blocks", num);
            header_t* headers=(header_t*)std::malloc(num*sizeof(header_t)); //FIXME, free !!!
            assert(headers!=NULL);
            boost::asio::read(socket,boost::asio::buffer((char*)headers,num*sizeof(header_t)));
            //load last header
            if(firstkeys!=NULL) { //store first vip keys
                block.now = 0;
                block.header_get();
                uint32_t laststart = block.now;
                memcpy(viphash,headers->viphash,32);
                char hash[65];
                hash[64]='\0';
                ed25519_key2text(hash,viphash,32);
                if(!block.vip_check(viphash,(uint8_t*)firstkeys+4,firstkeyslen/(2+32))) {
                    fprintf(stderr,"ERROR, failed to check %d VIP keys for hash %s\n",firstkeyslen/(2+32),hash);
                    socket.close();
                    return;
                }
                char filename[128];
                sprintf(filename,"vip/%64s.vip",hash);
                int fd=open(filename,O_WRONLY|O_CREAT,0644);
                if(fd<0) {
                    fprintf(stderr,"ERROR opening %s, fatal\n",filename);
                    socket.close();
                    return;
                }
                write(fd,firstkeys+4,firstkeyslen);
                close(fd);
                memcpy(oldhash,headers->nowhash,32);
                block.loadhead(*headers);
                if(memcmp(oldhash,block.nowhash,32)) {
                    fprintf(stderr,"ERROR, failed to confirm first header hash, fatal\n");
                    socket.close();
                    return;
                }
                memcpy(oldhash,headers->oldhash,32);
                if (!laststart) {
                    block.write_start();
                }
            } else {
                block.now=0;
                block.header_get();
                if(block.now+BLOCKSEC!=headers->now) { // do not accept download random blocks
                    fprintf(stderr,"ERROR, failed to get correct block (%08X), fatal\n",block.now+BLOCKSEC);
                    socket.close();
                    return;
                }
                memcpy(viphash,block.viphash,32);
                memcpy(oldhash,block.nowhash,32);
                block.load_vip(firstkeyslen,firstkeys,viphash);
            }
            //read signatures
            char* sigdata=NULL;
            union {
                uint64_t big;
                uint32_t small[2];
            } inf;
            uint32_t& signum=inf.small[1];
            boost::asio::read(socket,boost::asio::buffer(&inf.big,8));
            if(signum) {
                fprintf(stderr,"INFO get %d signatures for header %08X\n",signum,headers[num-1].now);
                sigdata=(char*)std::malloc(8+signum*sizeof(svsi_t)); //FIXME, free
                assert(sigdata!=NULL);
                memcpy(sigdata,&inf.big,8);
                boost::asio::read(socket,boost::asio::buffer(sigdata+8,signum*sizeof(svsi_t)));
            } else {
                fprintf(stderr,"ERROR, failed to get signatures for header %08X\n",headers[num-1].now);
                socket.close();
                return;
            }
            //read new viphash if different from last
            int lastkeyslen=0;
            boost::asio::read(socket,boost::asio::buffer(&lastkeyslen,4));
            if(lastkeyslen>0) {
                char* lastkeys=(char*)std::malloc(4+lastkeyslen);
                boost::asio::read(socket,boost::asio::buffer(lastkeys+4,lastkeyslen));
                char hash[65];
                hash[64]='\0';
                ed25519_key2text(hash,headers[num-1].viphash,32);
                if(!block.vip_check(headers[num-1].viphash,(uint8_t*)lastkeys+4,lastkeyslen/(2+32))) {
                    fprintf(stderr,"ERROR, failed to check VIP keys for hash %s\n",hash);
                    socket.close();
                    free(lastkeys);
                    return;
                }
                char filename[128];
                sprintf(filename,"vip/%64s.vip",hash);
                int fd=open(filename,O_WRONLY|O_CREAT,0644);
                if(fd<0) {
                    fprintf(stderr,"ERROR opening %s, fatal\n",filename);
                    socket.close();
                    free(lastkeys);
                    return;
                }
                write(fd,lastkeys+4,lastkeyslen);
                close(fd);
                free(lastkeys);
            }
            socket.close();
            //validate chain
            for(int n=0; n<num; n++) {
                if(n<num-1 && memcmp(viphash,headers[n].viphash,32)) {
                    fprintf(stderr,"ERROR failed to match viphash for header %08X, fatal\n",headers[n].now);
                    return;
                }
                if(memcmp(oldhash,headers[n].oldhash,32)) {
                    fprintf(stderr,"ERROR failed to match oldhash for header %08X, fatal\n",headers[n].now);
                    return;
                }
                memcpy(oldhash,headers[n].nowhash,32);
                block.loadhead(headers[n]);

                // send list of updated blocks
                // there is a dedicated function for hex?
                char blockId[9];
                sprintf(blockId, "%08X", headers[n].now);
                blockElement.put_value(blockId);
                blockChild.push_back(std::make_pair("",blockElement));

                if(memcmp(oldhash,block.nowhash,32)) {
                    fprintf(stderr,"ERROR failed to confirm nowhash for header %08X, fatal\n",headers[n].now);
                    return;
                }
            }

            pt.put_child("blocks", blockChild);
            //validate last block using firstkeys
            std::map<uint16_t,hash_s> vipkeys;
            for(int i=0; i<firstkeyslen; i+=2+32) {
                uint16_t svid=*((uint16_t*)(&firstkeys[i+4]));
                vipkeys[svid]=*((hash_s*)(&firstkeys[i+2+4]));
            }
            int vok=0;
            for(int i=0; i<(int)signum; i++) {
                uint16_t svid=*((uint16_t*)(&sigdata[i*sizeof(svsi_t)+8]));
                uint8_t* sig=(uint8_t*)sigdata+i*sizeof(svsi_t)+2+8;
                auto it=vipkeys.find(svid);
                if(it==vipkeys.end()) {
                    char hash[65];
                    hash[64]='\0';
                    ed25519_key2text(hash,sig,32);
                    fprintf(stderr,"ERROR vipkey (%d) not found %s\n",svid,hash);
                    continue;
                }
                if(!ed25519_sign_open((const unsigned char*)(&headers[num-1]),sizeof(header_t)-4,it->second.hash,sig)) {
                    vok++;
                } else {
                    char hash[65];
                    hash[64]='\0';
                    ed25519_key2text(hash,sig,32);
                    fprintf(stderr,"ERROR vipkey (%d) failed %s\n",svid,hash);
                }
            }
            if(2*vok<(int)vipkeys.size()) {
                fprintf(stderr,"ERROR failed to confirm enough signature for header %08X (2*%d<%d)\n",
                        headers[num-1].now,vok,(int)vipkeys.size());
                return;
            } else {
                fprintf(stderr,"CONFIRMED enough signature for header %08X (2*%d>=%d)\n",
                        headers[num-1].now,vok,(int)vipkeys.size());
            }
            // save hashes
            for(int n=0; n<num; n++) { //would be faster if avoiding open/close but who cares
                block.save_nowhash(headers[n]);
            }
            // update last confirmed header position
            fprintf(stderr,"SAVED %d headers from %08X to %08X\n",num,headers[0].now,headers[num-1].now);
            block.header_last();
        } //FIXME, check what needs to be freed

        else if(txs->ttype==TXSTYPE_VIP) {
            char hash[65];
            hash[64]='\0';
            ed25519_key2text(hash,txs->tinfo,32);
            int len;
            boost::asio::read(socket,boost::asio::buffer(&len,4));
            if(len<=0) {
                fprintf(stderr,"ERROR, failed to read VIP keys for hash %s\n",hash);
                socket.close();
                return;
            }
            char* buf=(char*)std::malloc(len);
            boost::asio::read(socket,boost::asio::buffer(buf,len));
            servers block;
            if(!block.vip_check(txs->tinfo,(uint8_t*)buf,len/(2+32))) {
                fprintf(stderr,"ERROR, failed to check VIP keys for hash %s\n",hash);
                socket.close();
                return;
            }
            char filename[128];
            sprintf(filename,"vip/%64s.vip",hash);
            mkdir("vip",0755);
            int fd=open(filename,O_WRONLY|O_CREAT,0644);
            if(fd<0) {
                fprintf(stderr,"ERROR opening %s, fatal\n",filename);
                socket.close();
                return;
            }
            write(fd,buf,len);
            close(fd);
            fprintf(stderr,"INFO, stored VIP keys in %s\n",filename);
            pt.put("viphash",hash);
            boost::property_tree::ptree viptree;
            for(char* p=buf; p<buf+len; p+=32) {
                ed25519_key2text(hash,(uint8_t*)p,32);
                boost::property_tree::ptree vipkey;
                vipkey.put("",hash);
                viptree.push_back(std::make_pair("",vipkey));
            }
            pt.add_child("vipkeys",viptree);
            free(buf);
        }

        else if(txs->ttype==TXSTYPE_SIG) { // print signatures
            print_blocktime(pt,txs->amsid);
            char sigh[129];
            sigh[128]='\0';
            uint32_t path=0;
            uint32_t nok=0;
            uint32_t nno=0;
            boost::asio::read(socket,boost::asio::buffer(&path,sizeof(uint32_t)));
            if(path!=txs->amsid) {
                fprintf(stderr,"ERROR, bad block number %d/%08X (<>%d/%08X)\n",path,path,txs->amsid,txs->amsid);
                goto END;
            } //nice :-)
            boost::asio::read(socket,boost::asio::buffer(&nok,sizeof(uint32_t)));
            if(nok) {
                svsi_t sigs[nok]; //assume nok will not be too large
                boost::asio::read(socket,boost::asio::buffer(sigs,nok*sizeof(svsi_t)));
                boost::property_tree::ptree psigs;
                for(uint32_t n=0; n<nok; n++) {
                    uint16_t *p=(uint16_t*)&sigs[n][0];
                    ed25519_key2text(sigh,&sigs[n][2],64);
                    boost::property_tree::ptree sig;
                    sig.put("node",*p);
                    sig.put("signature",sigh);
                    psigs.push_back(std::make_pair("",sig));
                }
                pt.add_child("signatures",psigs);
            }
            boost::asio::read(socket,boost::asio::buffer(&nno,sizeof(uint32_t)));
            if(nno) {
                svsi_t sigs[nno]; //assume nno will not be too large
                boost::asio::read(socket,boost::asio::buffer(sigs,nno*sizeof(svsi_t)));
                boost::property_tree::ptree psigs;
                for(uint32_t n=0; n<nno; n++) {
                    uint16_t *p=(uint16_t*)&sigs[n][0];
                    ed25519_key2text(sigh,&sigs[n][2],64);
                    boost::property_tree::ptree sig;
                    sig.put("node",*p);
                    sig.put("signature",sigh);
                    psigs.push_back(std::make_pair("",sig));
                }
                pt.add_child("fork_signatures",psigs);
            }
        }

        else if(txs->ttype==TXSTYPE_NDS) { // print nodes, consider archiving
            print_blocktime(pt,txs->amsid);
            uint32_t len;
            boost::asio::read(socket,boost::asio::buffer(&len,sizeof(uint32_t)));
            if(!len) {
                goto END;
            }
            char buf[len];
            boost::asio::read(socket,boost::asio::buffer(buf,len));
            mkdir("srv",0755);
            char filename[64];
            sprintf(filename,"srv/%08X.tmp",txs->amsid); //FIXME, do not save temp files !!!
            int fd=open(filename,O_WRONLY|O_CREAT,0644);
            if(fd<0) {
                goto END;
            }
            write(fd,buf,len);
            close(fd);
            servers srv;
            if(!srv.data_read(filename,true)) {
                goto END;
            }
            if(txs->amsid && srv.now!=txs->amsid) {
                goto END;
            }

            char blockhex[9];
            blockhex[8]='\0';
            sprintf(blockhex,"%08X",srv.now-BLOCKSEC);
            pt.put("block_prev",blockhex);
            sprintf(blockhex,"%08X",srv.now+BLOCKSEC);
            pt.put("block_next",blockhex);

            char hash[65];
            hash[64]='\0';
            boost::property_tree::ptree psrv;

            sprintf(blockhex,"%08X",srv.now);
            psrv.put("id", blockhex);
            psrv.put("time",srv.now);
            psrv.put("message_count",srv.msg);


            ed25519_key2text(hash,srv.oldhash,32);
            psrv.put("oldhash",hash);
            ed25519_key2text(hash,srv.minhash,32);
            psrv.put("minhash",hash);
            ed25519_key2text(hash,srv.msghash,32);
            psrv.put("msghash",hash);
            ed25519_key2text(hash,srv.nodhash,32);
            psrv.put("nodhash",hash);
            ed25519_key2text(hash,srv.viphash,32);
            psrv.put("viphash",hash);
            ed25519_key2text(hash,srv.nowhash,32);
            psrv.put("nowhash",hash);
            psrv.put("vote_yes",srv.vok);
            psrv.put("vote_no",srv.vno);
            psrv.put("vote_total",srv.vtot);

            psrv.put("dividend_balance",print_amount(srv.div));
            if(!((srv.now/BLOCKSEC)%BLOCKDIV)) {
                psrv.put("dividend_pay","true");
            } else {
                psrv.put("dividend_pay","false");
            }

            psrv.put("node_count",srv.nod);
            boost::property_tree::ptree nodes;
            for(uint32_t n=0; n<srv.nodes.size(); n++) {
                boost::property_tree::ptree node;

                char nodehex[5];
                nodehex[4]='\0';
                sprintf(nodehex,"%04X",n);
                node.put("id", nodehex);

                ed25519_key2text(hash,srv.nodes[n].pk,32);
                node.put("public_key",hash);
                ed25519_key2text(hash,(uint8_t*)&srv.nodes[n].hash[0],32);
                node.put("hash",hash);
                ed25519_key2text(hash,srv.nodes[n].msha,32);
                node.put("message_hash",hash);
                node.put("msid",srv.nodes[n].msid);
                node.put("mtim",srv.nodes[n].mtim);
                node.put("balance", print_amount(srv.nodes[n].weight));
                node.put("status",srv.nodes[n].status);
                node.put("account_count",srv.nodes[n].users);
                node.put("port",srv.nodes[n].port);

                struct in_addr ip_addr;
                ip_addr.s_addr = srv.nodes[n].ipv4;
                node.put("ipv4", inet_ntoa(ip_addr));

                nodes.push_back(std::make_pair("",node));
            }
            psrv.add_child("nodes",nodes);
            const uint8_t zero[32]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
            if(memcmp(srv.nodes[0].hash,zero,32)) { //read hlog
                boost::property_tree::ptree log;
                boost::asio::read(socket,boost::asio::buffer(&len,sizeof(uint32_t)));
                if(!len) {
                    goto END;
                }
                char buf[len];
                boost::asio::read(socket,boost::asio::buffer(buf,len));
                sprintf(filename,"srv/%08X.hlg",txs->amsid); //FIXME, do not save temp files !!!
                int fd=open(filename,O_WRONLY|O_CREAT,0644);
                if(fd<0) {
                    goto END;
                }
                write(fd,buf,len);
                close(fd);
                Helper::Hlog hlg(log,filename);
                psrv.add_child("log",log);
            }
            pt.add_child("block",psrv);
        }

        else if(txs->ttype==TXSTYPE_NOD) { // print accounts of a node
            print_blocktime(pt,txs->amsid);
            uint32_t len;
            boost::asio::read(socket,boost::asio::buffer(&len,sizeof(uint32_t)));
            if(!len) {
                goto END;
            }
            char buf[len];
            boost::asio::read(socket,boost::asio::buffer(buf,len));
            user_t* userp=(user_t*)buf;
            uint32_t usern=len/sizeof(user_t);
            boost::property_tree::ptree users;
            for(uint32_t n=0; n<usern; n++,userp++) {
                boost::property_tree::ptree user;
                print_user(*userp,user,true,txs->bbank,n);
                users.push_back(std::make_pair("",user.get_child("account")));
            }
            pt.add_child("accounts",users);
        }

        else if(txs->ttype==TXSTYPE_MGS) { // print message list, consider archiving
            print_blocktime(pt,txs->amsid);
            uint32_t len;
            boost::asio::read(socket,boost::asio::buffer(&len,sizeof(uint32_t)));
            if(!len || (len-32)%(2+4+32)) {
                pt.put("error_bad_length",len);
                goto END;
            }
            uint8_t msghash[32];
            char buf[len];
            char *bufp=buf;
            char *end=buf+len;
            boost::asio::read(socket,boost::asio::buffer(buf,len));
            char hash[65];
            hash[64]='\0';
            ed25519_key2text(hash,(uint8_t*)buf,32);
            pt.put("msghash",hash);
            bufp+=32;
            hashtree tree;
            boost::property_tree::ptree msghashes;
            for(; bufp<end; bufp+=2+4+32) {
                boost::property_tree::ptree msghash;
                uint16_t svid=*(uint16_t*)(bufp);


                char nodehex[5];
                nodehex[4]='\0';
                sprintf(nodehex,"%04X",svid);
                msghash.put("node_id", nodehex);

                uint32_t msid=*(uint32_t*)(bufp+2);
                msghash.put("node_msid",msid);
                ed25519_key2text(hash,(uint8_t*)bufp+6,32);
                msghash.put("hash",hash);
                tree.update((uint8_t*)bufp+6);
                msghashes.push_back(std::make_pair("",msghash));
            }
            tree.finish(msghash);
            if(memcmp(buf,msghash,32)) {
                ed25519_key2text(hash,msghash,32);
                pt.put("msghash_calculated",hash);
                pt.put("confirmed","no");
            } else {
                pt.put("confirmed","yes");
            }
            pt.add_child("messages",msghashes);
        }

        else if(txs->ttype==TXSTYPE_MSG) { // print message
            print_blocktime(pt,txs->amsid);
            uint32_t len;
            boost::asio::read(socket,boost::asio::buffer(&len,4));
            if(!len || (len & 0xFF) != MSGTYPE_MSG || len>>8 < message::data_offset) {
                pt.put("error_bad_length",len);
                goto END;
            }
            char buf[(len>>8)];
            memcpy(buf,&len,4);
            len>>=8;
            boost::asio::read(socket,boost::asio::buffer(buf+4,len-4));
            message_ptr msg(new message(len,(uint8_t*)buf));
            msg->read_head();
            pt.put("node",msg->svid);
            pt.put("node_msid",msg->msid);
            pt.put("time",msg->now);
            pt.put("length",msg->len);
            if(!msg->hash_tree_fast(msg->sigh,msg->data,msg->len,msg->svid,msg->msid)) {
                goto END;
            }
            char hash[65];
            hash[64]='\0';
            char signtext[129];
            signtext[128]='\0';
            ed25519_key2text(hash,msg->sigh,32);
            pt.put("hash",hash);
            //parse and print message
            uint8_t* p=msg->data+message::data_offset;
            uint8_t* end=msg->data+msg->len;
            uint32_t mpos=1;
            uint32_t l;
            assert(p<end);
            boost::property_tree::ptree transactions;
            for(; p<end; p+=l,mpos++) {
                usertxs utxs;
                if(!utxs.parse((char*)p)) {
                    pt.put("error_parse","true");
                    break;
                }
                l=utxs.size;
                boost::property_tree::ptree transaction;
                sprintf(hash,"%04X%08X%04X",msg->svid,msg->msid,mpos);
                transaction.put("id",hash);
                transaction.put("type",utxs.ttype);

                transaction.put("abank",utxs.abank);
                transaction.put("auser",utxs.auser);
                transaction.put("amsid",utxs.amsid);
                transaction.put("ttime",utxs.ttime);
                transaction.put("bbank",utxs.bbank);
                transaction.put("buser",utxs.buser);

                transaction.put("amount",print_amount(utxs.tmass));
                transaction.put("sender_fee",print_amount(1));
                uint16_t suffix=sts.crc_acnt(utxs.abank,utxs.auser);
                char acnt[19];
                sprintf(acnt,"%04X-%08X-%04X",utxs.abank,utxs.auser,suffix);
                if(utxs.abank != 0 || utxs.auser != 0) {
                    transaction.put("sender_address",acnt);
                }
                suffix=sts.crc_acnt(utxs.bbank,utxs.buser);
                sprintf(acnt,"%04X-%08X-%04X",utxs.bbank,utxs.buser,suffix);
                if(utxs.bbank != 0 || utxs.buser != 0) {
                    transaction.put("target_address",acnt);
                }


                ed25519_key2text(hash,utxs.tinfo,32);
                transaction.put("message",hash);
                ed25519_key2text(signtext,utxs.get_sig((char*)p),64);
                transaction.put("signature",signtext);
                transaction.put("size" ,utxs.size);
                transactions.push_back(std::make_pair("",transaction));
            }
            pt.add_child("transactions",transactions);
            //TODO,check if message in message list
        }

        else {
            int len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
            if(len!=sizeof(user_t)) {
                std::cerr<<"ERROR reading confirmation\n";
            } else {
                user_t myuser;
                memcpy(&myuser,buf,sizeof(user_t));
                if(txs->ttype==TXSTYPE_INF) {
                    print_user(myuser,pt,true,txs->bbank,txs->buser);
                } else {
                    print_user(myuser,pt,true,sts.bank,sts.user);
                }
                if(txs->ttype!=TXSTYPE_INF || (txs->buser==sts.user && txs->bbank==sts.bank)) {
                    if(txs->ttype!=TXSTYPE_INF && txs->ttype!=TXSTYPE_LOG && (uint32_t)sts.msid+1!=myuser.msid) {
                        std::cerr<<"ERROR transaction failed (bad msid)\n";
                    }
                    if(memcmp(sts.pk,myuser.pkey,32)) {
                        if(txs->ttype==TXSTYPE_KEY) {
                            std::cerr<<"PKEY changed\n";
                        } else {
                            char tx_user_public_key[65];
                            tx_user_public_key[64]='\0';
                            ed25519_key2text(tx_user_public_key,myuser.pkey,32);
                            logpt.put("tx.account_public_key_new",tx_user_public_key);
                            std::cerr<<"PKEY differs\n";
                        }
                    }
                    if(txs->ttype!=TXSTYPE_INF && txs->ttype!=TXSTYPE_LOG) {
                        //TODO, validate hashout, check if correct
                        char tx_user_hashout[65];
                        tx_user_hashout[64]='\0';
                        ed25519_key2text(tx_user_hashout,myuser.hash,32);
                        logpt.put("tx.account_hashout",tx_user_hashout);
                        if(memcmp(myuser.hash,hashout,32)) {
                            pt.put("error.text","hash mismatch");
                        }
                    }
                    sts.msid=myuser.msid;
                    memcpy(sts.ha.data(), myuser.hash, SHA256_DIGEST_LENGTH);
                }
            }
            /*if(txs->ttype==TXSTYPE_INF){
              try{
                len=boost::asio::read(socket,boost::asio::buffer(buf,sizeof(user_t)));
                if(len!=sizeof(user_t)){
                  std::cerr<<"ERROR reading global info\n";}
                else{
                  user_t myuser;
                  memcpy(&myuser,buf,sizeof(user_t));
                  print_user(myuser,pt,false,txs->bbank,txs->buser);}}
              catch (std::exception& e){
                fprintf(stderr,"ERROR reading global info: %s\n",e.what());}}
            else
              */if(txs->ttype==TXSTYPE_LOG) {
                int len;
                if(sizeof(int)!=boost::asio::read(socket,boost::asio::buffer(&len,sizeof(int)))) {
                    std::cerr<<"ERROR reading log length\n";
                    socket.close();
                    return;
                }
                if(!len) {
                    fprintf(stderr,"No new log entries\n");
                } else {
                    log_t* log=(log_t*)std::malloc(len);
                    if(len!=(int)boost::asio::read(socket,boost::asio::buffer((char*)log,len))) { // exception will cause leak
                        std::cerr<<"ERROR reading log\n";
                        free(log);
                        socket.close();
                        return;
                    }
                    save_log(log,len,txs->ttime,sts);
                    free(log);
                }
                print_log(pt,sts);
            } else {
                struct {
                    uint32_t msid;
                    uint32_t mpos;
                } m;
                if(sizeof(m)!=boost::asio::read(socket,boost::asio::buffer(&m,sizeof(m)))) {
                    std::cerr<<"ERROR reading transaction confirmation\n";
                    socket.close();
                    return;
                } else {
                    logpt.put("tx.node_msid",m.msid);
                    logpt.put("tx.node_mpos",m.mpos);
                    if(sts.olog) {
                        out_log(logpt,sts.bank,sts.user);
                    }
                    char tx_id[64];
                    sprintf(tx_id,"%04X:%08X:%04X",sts.bank,m.msid,m.mpos);
                    pt.put("tx.node_msid",m.msid);
                    pt.put("tx.node_mpos",m.mpos);
                    pt.put("tx.id",tx_id);
                }
            }
        }

END:
        socket.close();
        boost::property_tree::write_json(std::cout,pt,sts.nice);
    } catch (std::exception& e) {
        DLOG("Talk exception %s\n", e.what());
        pt.clear();
        pt.put(ERROR_TAG, e.what());
        boost::property_tree::write_json(std::cout,pt,sts.nice);
        // exit with error code to enable sane bash scripting
        if(!isatty(fileno(stdin))) {
            // fprintf(stderr,"EXIT\n");
            exit(-1);
        }
    }
}

//TODO add timeout
//http://www.boost.org/doc/libs/1_52_0/doc/html/boost_asio/example/timeouts/blocking_tcp_client.cpp
/*int main(int argc, char* argv[])
{ settings sts;
  sts.get(argc,argv);
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query(sts.host,std::to_string(sts.port).c_str());
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
  boost::asio::ip::tcp::socket socket(io_service);
#if INTPTR_MAX == INT64_MAX
  assert(sizeof(long double)==16);
#endif
  try{
    char line[0x10000];line[0xffff]='\0';
    usertxs_ptr txs;
    while(NULL!=fgets(line,0xffff,stdin)){
      int64_t deduct=0;
      int64_t fee=0;
      if(line[0]=='\n' || line[0]=='#'){
        continue;}
      txs=run_json(sts,line,deduct,fee);
      if(txs==NULL){
        break;}
      talk(endpoint_iterator,socket,sts,txs,deduct,fee);}}
  catch (std::exception& e){
    std::cerr << "Main Exception: " << e.what() << "\n";}
  return 0;
}*/
