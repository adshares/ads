#include "json.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include "hash.h"
#include "ascii.h"
#include "txsname.h"
#include "command/errorcodes.h"
#include "command/factory.h"

namespace Helper {

void print_user(user_t& u, boost::property_tree::ptree& pt, bool local, uint32_t bank, uint32_t user) {
    char pkey[65];
    char hash[65];
    ed25519_key2text(pkey,u.pkey,32);
    ed25519_key2text(hash,u.hash,32);
    pkey[64]='\0';
    hash[64]='\0';
    uint16_t suffix=crc_acnt(bank,user);
    char ucnt[19]="";
    char acnt[19];
    sprintf(acnt,"%04X-%08X-%04X",bank,user,suffix);
    if(u.node) {
        suffix=crc_acnt(u.node,u.user);
        sprintf(ucnt,"%04X-%08X-%04X",u.node,u.user,suffix);
    }
    if(local) {
        pt.put("account.address",acnt);
        pt.put("account.node",bank);
        pt.put("account.id",user);
        pt.put("account.msid",u.msid);
        pt.put("account.time",u.time);
        pt.put("account.date",mydate(u.time));
        pt.put("account.status",u.stat);
        pt.put("account.paired_node",u.node);
        pt.put("account.paired_id",u.user);
        if((u.node || u.user) && (u.node != bank || u.user != user)) {
            pt.put("account.paired_address",ucnt);
        }
        pt.put("account.local_change",u.lpath);
        pt.put("account.remote_change",u.rpath);
        pt.put("account.balance",print_amount(u.weight));
        pt.put("account.public_key",pkey);
        pt.put("account.hash",hash);
    } else {
        pt.put("network_account.address",acnt);
        pt.put("network_account.node",bank);
        pt.put("network_account.id",user);
        pt.put("network_account.msid",u.msid);
        pt.put("network_account.time",u.time);
        pt.put("network_account.date",mydate(u.time));
        pt.put("network_account.status",u.stat);
        pt.put("network_account.paired_node",u.node);
        pt.put("network_account.paired_id",u.user);
        if((u.node || u.user) && (u.node != bank || u.user != user)) {
            pt.put("network_account.paired_address",ucnt);
        }
        pt.put("network_account.local_change",u.lpath);
        pt.put("network_account.remote_change",u.rpath);
        pt.put("network_account.balance",print_amount(u.weight));
        pt.put("network_account.public_key",pkey);
        pt.put("network_account.hash",hash);
        if(!check_csum(u,bank,user)) {
            pt.put("network_account.checksum","true");
        } else {
            pt.put("network_account.checksum","false");
        }
    }
}

void print_msgid_info(boost::property_tree::ptree& pt, uint16_t node, uint32_t msid, uint32_t mpos){
    pt.put("tx.node_msid", msid);
    pt.put("tx.node_mpos", mpos);
    pt.put("tx.id", Helper::print_msg_id(node, msid, mpos));
}

bool parse_amount(int64_t& amount,std::string str_amount) {
    size_t dot_pos = str_amount.find('.');
    if(dot_pos == std::string::npos) {
        str_amount.insert(str_amount.length(), AMOUNT_DECIMALS, '0');
    } else {
        size_t after_dot = str_amount.length() - dot_pos - 1;
        if(after_dot == 0 || after_dot > AMOUNT_DECIMALS) {
            return(false);
        }
        str_amount.erase(dot_pos, 1).append(AMOUNT_DECIMALS - after_dot, '0');
    }
    char * endptr;
    errno = 0;
    amount=std::strtoll(str_amount.c_str(), &endptr, 10);
    if(*endptr != '\0' || errno) {
        return(false);
    }
    return(true);
}

char* print_amount(int64_t amount) {
    static char text[32];
    uint is_neg = amount<0?1:0;
    std::string str_amount = std::to_string(amount);
    if(str_amount.length() < AMOUNT_DECIMALS + 1 + is_neg) {
        str_amount.insert(is_neg, AMOUNT_DECIMALS + 1 + is_neg - str_amount.length(), '0');
    }
    str_amount.insert(str_amount.length() - AMOUNT_DECIMALS, 1, '.');
    strncpy(text, str_amount.c_str(), sizeof(text));
    return(text);
}

char* mydate(uint32_t now) {
    time_t      lnow=now;
    static char text[64];
    struct tm   *tmp;
    tmp=localtime(&lnow);

    if(!strftime(text,64,"%F %T",tmp)) {
        text[0]='\0';
    }
    return(text);
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

void printErrorJson(const char *errorMsg, bool pretty) {
    boost::property_tree::ptree ptree;
    ptree.put(ERROR_TAG, errorMsg);
    boost::property_tree::write_json(std::cout, ptree, pretty);
}

const std::string print_address(uint16_t node, uint32_t user, int32_t _suffix) {
    char acnt[19];
    uint16_t suffix;
    (_suffix == -1) ? suffix = Helper::crc_acnt(node, user) : suffix = _suffix;
    sprintf(acnt,"%04X-%08X-%04X", node, user, suffix);
    return std::string(acnt);
}

const std::string print_msg_id(uint16_t node, uint32_t user, int32_t _suffix) {
    char acnt[19];
    uint16_t suffix;
    (_suffix == -1) ? suffix = Helper::crc_acnt(node, user) : suffix = _suffix;
    sprintf(acnt,"%04X:%08X:%04X", node, user, suffix);
    return std::string(acnt);
}

const std::string print_msg_pack_id(uint16_t node, uint32_t msg_id) {
    char acnt[19];
    sprintf(acnt,"%04X:%08X", node, msg_id);
    return std::string(acnt);
}

void print_log(boost::property_tree::ptree& pt, uint16_t bank, uint32_t user, uint32_t lastlog, int txnType = -1) {
    char filename[64];
    sprintf(filename,"log/%04X_%08X.bin", bank, user);
    int fd=open(filename,O_RDONLY);
    if(fd<0) {
        fprintf(stderr,"ERROR, failed to open log file %s\n",filename);
        return;
    }
    if(lastlog>1) { //guess a good starting position
        off_t end=lseek(fd,0,SEEK_END);
        if(end>0) {
            off_t start=end;
            off_t tseek=0;
            while((start=lseek(fd,(start>(off_t)(sizeof(log_t)*32)?-sizeof(log_t)*32-tseek:-start-tseek),SEEK_CUR))>0) {
                uint32_t ltime=0;
                tseek=read(fd,&ltime,sizeof(uint32_t));
                if(ltime<lastlog-1) { // tollerate 1s difference
                    lseek(fd,-tseek,SEEK_CUR);
                    break;
                }
            }
        }
    }
    log_t ulog;
    boost::property_tree::ptree logtree;
    while(read(fd,&ulog,sizeof(log_t))==sizeof(log_t)) {
        if(ulog.time<lastlog) {
            //fprintf(stderr,"SKIPP %d [%08X]\n",ulog.time,ulog.time);
            continue;
        }
        uint16_t txst=ulog.type&0xFF;
        if (txnType != -1 && txst != txnType) {
            continue;
        }
        char info[65];
        info[64]='\0';
        ed25519_key2text(info,ulog.info,32);
        boost::property_tree::ptree logentry;
        uint16_t suffix=crc_acnt(ulog.node,ulog.user);
        char acnt[19];
        sprintf(acnt,"%04X-%08X-%04X",ulog.node,ulog.user,suffix);
        logentry.put("time",ulog.time);
        logentry.put("date",mydate(ulog.time));
        logentry.put("type_no",ulog.type);
        // FIXME: properly flag confirmed transactions, which will not be rolled back
        uint32_t prev_block=time(NULL);
        prev_block-=prev_block%BLOCKSEC;
        logentry.put("confirmed", ulog.time < prev_block - VIP_MAX*VOTE_DELAY ? "yes" : "no");
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
                uint16_t suffix=crc_acnt(ulog.node,ulog.user);
                char acnt[19]="";
                sprintf(acnt,"%04X-%08X-%04X",ulog.node,ulog.user,suffix);
                logentry.put("account.address",acnt);
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_DIV) { //dividend
                logentry.put("node_msid",ulog.nmid);
                char blockhex[9];
                blockhex[8]='\0';
                sprintf(blockhex,"%08X",ulog.mpos);
                logentry.put("block_id",blockhex);
                logentry.put("dividend",print_amount(ulog.weight));
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_FEE) { //bank profit
                if(ulog.nmid) { // bank profit on transactions
                    logentry.put("profit",print_amount(ulog.weight));
                    logentry.put("node",ulog.node);
                    logentry.put("node_msid",ulog.nmid);
                    if(ulog.nmid==bank) {
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
                    char blockhex[9];
                    blockhex[8]='\0';
                    sprintf(blockhex,"%08X",ulog.mpos);
                    logentry.put("block_id",blockhex);
                    int64_t div;
                    int64_t usr;
                    int64_t get;
                    int64_t put;
                    memcpy(&div,ulog.info+ 0,8);
                    memcpy(&usr,ulog.info+ 8,8);
                    memcpy(&get,ulog.info+16,8);
                    memcpy(&put,ulog.info+24,8);
                    logentry.put("profit_put",print_amount(put));
                    logentry.put("profit_div",print_amount(div));
                    logentry.put("profit_usr",print_amount(usr));
                    logentry.put("profit_get",print_amount(get));
                    logentry.put("profit",print_amount(div+usr+get+put));
                    logentry.put("fee",print_amount(ulog.weight));
                }
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_UOK) { //creare remote account
                logentry.put("node",ulog.node);
                char blockhex[9];
                blockhex[8]='\0';
                sprintf(blockhex,"%08X",ulog.mpos);
                logentry.put("block_id",blockhex);
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
            if(txst==TXSTYPE_USR && ulog.node != bank) { //create_account log on remote account
                logentry.put("amount",print_amount(ulog.weight));
                logentry.put("node",ulog.node);
                char blockhex[9];
                blockhex[8]='\0';
                sprintf(blockhex,"%08X",ulog.mpos);
                logentry.put("block_id",blockhex);
                if(ulog.user) {
                    logentry.put("account",ulog.user);
                    logentry.put("address",acnt);
                }
                logentry.put("public_key",info);
                logtree.push_back(std::make_pair("",logentry));
                continue;
            }
            if(txst==TXSTYPE_BNK) {
                char blockhex[9];
                blockhex[8]='\0';
                sprintf(blockhex,"%08X",ulog.mpos);
                logentry.put("block_id",blockhex);
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
        if(ulog.node > 0) {
            logentry.put("node",ulog.node);
            logentry.put("account",ulog.user);
            logentry.put("address",acnt);
        }
        if(!ulog.nmid) {
            char blockhex[9];
            blockhex[8]='\0';
            sprintf(blockhex,"%08X",ulog.mpos);
            logentry.put("block_id",blockhex);
        } else {
            logentry.put("node_msid",ulog.nmid);
            logentry.put("node_mpos",ulog.mpos);
            logentry.put("account_msid",ulog.umid);
        }
        logentry.put("amount",print_amount(ulog.weight));
        //FIXME calculate fee
        if(txst==TXSTYPE_PUT) {
            int64_t fee;
            if(ulog.node==bank) {
                fee = TXS_PUT_FEE(std::abs(ulog.weight));
            } else {
                fee = TXS_PUT_FEE(std::abs(ulog.weight))+TXS_LNG_FEE(std::abs(ulog.weight));
            }

            logentry.put("sender_fee",print_amount(fee<TXS_MIN_FEE?TXS_MIN_FEE:fee));
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
            if(txst==TXSTYPE_SBS || txst==TXSTYPE_UBS || txst==TXSTYPE_SUS || txst==TXSTYPE_UUS) {
                logentry.put("status",(weight));
            } else {
                logentry.put("sender_balance",print_amount(weight));
            }
            logentry.put("sender_amount",print_amount(deduct));
            if(txst==TXSTYPE_MPT) {
                if(ulog.node==bank){
                  logentry.put("sender_fee",print_amount((int64_t)((boost::multiprecision::int128_t)fee * std::abs(ulog.weight) / deduct)));}
                else{
                  logentry.put("sender_fee",
                    print_amount((int64_t)((boost::multiprecision::int128_t)fee * std::abs(ulog.weight) / deduct)));}
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
            sprintf(tx_id,"%04X:%08X:%04X",bank,ulog.nmid,ulog.mpos);
        }
        logentry.put("id",tx_id);
        logtree.push_back(std::make_pair("",logentry));
    }
    close(fd);
    pt.add_child("log",logtree);
}

void save_log(log_t* log, int len, uint32_t from, uint16_t bank, uint32_t user) {
    char filename[64];
    mkdir("log",0755);
    sprintf(filename,"log/%04X_%08X.bin", bank, user);
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

void print_all_commands_help()
{
    std::cerr<<"Possible commands listing"<<std::endl<<std::endl;;

    std::unique_ptr<IBlockCommand> command;
    for (uint8_t id = 0; id < TXSTYPE_MAX; ++id)
    {
        command = command::factory::makeCommand(id);
        if (command)
        {
            std::string command_help = command->usageHelperToString();
            if (!command_help.empty())
            {
                std::cout<<"Command: " << getTxnName(id) << std::endl;
                std::cout << command_help << std::endl;
            }
        }
    }
}

}

