#ifndef SERVERS_HPP
#define SERVERS_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <openssl/rand.h>
#include <dirent.h>
#include "ed25519/ed25519.h"
#include "message.hpp"
#include "helper/json.h"

class node {
  public:
    node() :
        pk{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        hash{0,0,0,0},
        msha{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        msid(0),
        mtim(0),
        weight(0),
        status(0),
        users(0),
        port(0),
        ipv4(0)
        //addr(""),
    {
        //bzero(host,64);
    }
    ed25519_public_key pk; // public key
    //uint8_t ohash[SHA256_DIGEST_LENGTH]; // hash of accounts
    uint64_t hash[4]; // hash of accounts, could be atomic to prevent locking
    uint8_t msha[SHA256_DIGEST_LENGTH]; // hash of last message
    uint32_t msid; // last message, server closed if msid==0xffffffff
    uint32_t mtim; // time of last message
    int64_t weight; // placeholder for server weight (total assets)
    uint32_t status; // placeholder for future status settings
    //uint32_t weight; // placeholder for server weight (total assets)
    uint32_t users;
    uint32_t port; // port
    uint32_t ipv4; // ipv4

    //char host[64]; // hostname or address ... remove later
    //std::string addr; // hostname or address ... remove later
    //std::list<int> in; // incomming connections
    //std::list<int> out; // outgoing connections
    std::vector<uint64_t> changed; //account changed bits
//private:
//	friend class boost::serialization::access;
//	template<class Archive> void serialize(Archive & ar, const unsigned int version)
//	{
//		ar & pk;
//	 	if(version<3){	ar & ohash; memcpy(hash,ohash,32);}
//		else{		ar & hash;}
//		//if(version>0) ar & msha;
//		ar & msha;
//	 	ar & msid;
//	 	ar & mtim;
//		ar & weight; //changed order !!!
//	 	ar & status; //changed order !!!
//	 	//if(version==0) ar & in;
//	 	//if(version==0) ar & out;
//	 	//if(version==0) ar & addr;
//	 	//if(version==1) ar & host;
//	 	//if(version==1) ar & port;
//	 	////ar & status;
//		//if(version>0) ar & weight;
//		////ar & weight;
//	 	if(version>1) ar & users;
//	 	if(version>1) ar & port;
//	 	if(version>1) ar & ipv4;
//	}
};
//BOOST_CLASS_VERSION(node, 3)
class servers { // also a block
  public:
    uint32_t now; // start time of the block
    //uint32_t old; // time of last block
    uint32_t msg; // number of messages in block, FIXME, should be uint16_t [why???]
    uint32_t nod; // number of nodes in block, FIXME, should be uint16_t
    uint32_t div; // dividend
    uint8_t oldhash[SHA256_DIGEST_LENGTH]; // previous hash
    uint8_t minhash[SHA256_DIGEST_LENGTH]; // hash input before msghash used only for hashtree reports
    uint8_t msghash[SHA256_DIGEST_LENGTH]; // hash of transactions
    //uint8_t txshash[SHA256_DIGEST_LENGTH]; // hash of transactions
    uint8_t nodhash[SHA256_DIGEST_LENGTH]; // hash of nodes
    uint8_t viphash[SHA256_DIGEST_LENGTH]; // hash of vip public keys
    uint8_t nowhash[SHA256_DIGEST_LENGTH]; // current hash
    uint16_t vok; //votes in favor
    uint16_t vno; //votes against
    uint16_t vtot; //total eligible voters
    uint64_t txs; //number of transactions in block
    std::vector<node> nodes; //FIXME, this vector can fail to allocate RAM, do tests with more than 32k nodes!
    //std::vector<void*> users; // place holder for user accounts
    servers():
        now(0),
        //old(0), //FIXME, remove, oldhash will be '000...000' if old!=now-BLOCKSEC
        msg(0),
        nod(0),
        div(0),
        oldhash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        minhash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        msghash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        //txshash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        nodhash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        viphash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        nowhash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        vok(0),
        vno(0),
        vtot(0xffff) {
    }


    void create_genesis_block(const std::string genesis_file) {
        assert(nodes.size() == 0);
        ELOG("INIT: using genesis file %s\n", genesis_file.c_str());

        boost::property_tree::ptree data;
        boost::property_tree::read_json(genesis_file, data);

        std::ifstream t(genesis_file);
        std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

        hash_t genesis_hash;
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, str.c_str(), str.length());
        SHA256_Final(genesis_hash, &sha256);

        char hash_text[2 * SHA256_DIGEST_LENGTH];
        ed25519_key2text(hash_text, genesis_hash, SHA256_DIGEST_LENGTH);
        ELOG("genesis hash: %.*s\n", 2 * SHA256_DIGEST_LENGTH, hash_text);

        boost::property_tree::ptree dataConfig = data.get_child("config");

        boost::optional < uint32_t > startTimeOpt = dataConfig.get_optional < uint32_t > ("start_time");

        if (startTimeOpt.is_initialized() && startTimeOpt.get() % BLOCKSEC != 0) {
            ELOG("Invalid genesis start time: %d, must be divisible by %d\n", startTimeOpt.get(), BLOCKSEC);
            exit(-1);
        } else {
            ELOG("Genesis start time: %d\n", startTimeOpt.get());

        }

        uint32_t startTime = startTimeOpt.get();

        uint64_t waitUntil = startTime + BLOCKSEC;

        uint64_t clockNow = time(NULL);
        while (clockNow < waitUntil) {
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            ELOG("Awaiting for genesis block time: %lu s\n", waitUntil - clockNow);
            clockNow = time(NULL);
            RETURN_ON_SHUTDOWN()
            ;
        }

        node nn;
        now = nn.mtim = startTime;

        memcpy(nn.msha, genesis_hash, SHA256_DIGEST_LENGTH);
        nodes.push_back(nn);

        boost::property_tree::ptree dataNodes = data.get_child("nodes");

        uint16_t node_num = 1;
        for (auto e : dataNodes) {
            uint64_t users_weight = 0;
            uint32_t users_count = 0;
            node nn;

            std::string node_pkey = e.second.get < std::string > ("public_key");
            ed25519_text2key(nn.pk, node_pkey.c_str(), 32);

            nn.mtim = nodes[0].mtim;
            nn.users = users_count;
            nodes.push_back(nn);

            for (auto f : e.second.get_child("accounts")) {
                user_t u;
                std::string user_pkey = f.second.get < std::string > ("public_key");
                ed25519_public_key user_pk;
                ed25519_text2key(user_pk, user_pkey.c_str(), 32);

                std::string user_balance = f.second.get < std::string > ("balance");
                parse_amount(u.weight, user_balance);
                init_user(u, node_num, users_count, u.weight, user_pk, nodes[0].mtim, node_num, users_count);
                put_user(u, node_num, users_count);
                xor4(nodes[node_num].hash, u.csum);
                users_count++;
                users_weight += u.weight;
            }

            nodes[node_num].weight = users_weight;
            init_node_hash(nodes[node_num]);
            node_num++;
        }
        update_vipstatus();
    }

    void init_fast(uint16_t node_count, hash_t pk) {
        std::cout << "init_fast" << node_count << std::endl;
        assert(nodes.size() == 0 || nodes[0].status == SERVER_FST);
        node nn;
        nn.status = SERVER_FST;
        memcpy(nn.pk,pk,32);
        for(int i=nodes.size();i<=node_count;i++) {
            nodes.push_back(nn);
        }
    }

    void init(uint32_t newnow) {
        uint16_t num = 0;
        uint64_t sum = 0;

        if (!nodes.size()) {
            now = newnow;
            blockdir();
            // create test network
            mkdir("key", 0700);
            int fd = open("key/key.txt", O_WRONLY | O_CREAT, 0600);
            close(fd);
            FILE *fp = fopen("key/key.txt", "a");
            // ed25519/key "author name"
            fprintf(fp,
                    "14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0\n#PK: 7D21F4EE7DE72EEDDC2EBFFEC5E7F33F140A975A629EE312075BB04610A9CFFF\n#SG: C42B0C170A78C9985319B7A2E17D44E4BD88845FCE21C3FCC00A496AAAB6E8F84AD54E06F0D5FDDE98D370462C4EFAA52A38C8BCB513B7DF597315835244D10A\n");
            fclose(fp);
            hash_t hash;
            ed25519_text2key(hash, "7D21F4EE7DE72EEDDC2EBFFEC5E7F33F140A975A629EE312075BB04610A9CFFF", 32);
            node nn;
            memcpy(nn.pk, hash, 32);
            nodes.push_back(nn);
            nodes.push_back(nn);

            int64_t stw = TOTALMASS / (nodes.size() - 1); // removed initial tax of 1%
            for (auto it = nodes.begin(); it < nodes.end(); it++, num++) {

                if (!num) {
                    RAND_bytes(it->msha, sizeof(it->msha));
                }
//          memset(it->msha,0xff,SHA256_DIGEST_LENGTH); //TODO, start servers this way too
//          memcpy(it->msha,&num,2); // always start with a unique hash
//          memcpy(it->msha+2,&now,4); // network id
                it->msid = 0;
                it->mtim = now; // blockchain start time in nodes[0] == network id
                it->status = 0;
                if (num) {
                    //if(num<=VIP_MAX){
                    if (num == 1) {
                        it->status |= SERVER_VIP;
                    }
                    it->users = 1;
                    // create the first user
                    user_t u;
                    init_user(u, num, 0, stw, it->pk, now, num, 0);
                    put_user(u, num, 0);
                    //update_nodehash(num);
                    memcpy(it->hash, u.csum, SHA256_DIGEST_LENGTH);
                    it->weight = u.weight;
                    init_node_hash(*it);
                } else {
                    bzero(it->hash, SHA256_DIGEST_LENGTH);
                    bzero(it->pk, SHA256_DIGEST_LENGTH);
                    it->users = 0;
                    it->weight = 0;
                }
            }
        }

        num = 0;
        for (auto it = nodes.begin(); it < nodes.end(); it++, num++) {
            if (num) {
                sum += it->weight;
            }
        }

        ELOG("INIT: weight diff: %016lX\n", TOTALMASS-sum);
        //nodes.begin()->weight=TOTALMASS-sum;
        assert(num > 0);
        //vtot=(uint16_t)(num<VIP_MAX?num:VIP_MAX); // probably not needed !!!
        vtot = 1;
        finish();
        write_start();
        now = 0;
        put();
        now = nodes[0].mtim;
        //return(num<VIP_MAX?num:VIP_MAX);
    }

    void write_start() {
        FILE* fp=fopen("blk/start.txt","w");
        if(fp==NULL) {
            throw("FATAL ERROR: failed to write to blk/start.txt\n");
        }
        fprintf(fp,"%08X\n",now);
        fclose(fp);
    }

    uint32_t read_start() {
        FILE* fp=fopen("blk/start.txt","r");
        if(fp==NULL) {
            ELOG("FATAL ERROR: failed to write to blk/start.txt\n");
            exit(-1);
        }
        uint32_t start;
        fscanf(fp,"%X",&start);
        fclose(fp);
        return(start);
    }

    void xor4(uint64_t* to,uint64_t* from) {
        to[0]^=from[0];
        to[1]^=from[1];
        to[2]^=from[2];
        to[3]^=from[3];
    }

    void user_csum(user_t& u,uint16_t peer,uint32_t uid) {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256,&u,sizeof(user_t)-4*sizeof(uint64_t));
        SHA256_Update(&sha256,&peer,sizeof(uint16_t));
        SHA256_Update(&sha256,&uid,sizeof(uint32_t));
        SHA256_Final((uint8_t*)u.csum,&sha256);
    }

    void init_node_hash(node& nn) {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256,&nodes[0].msha,sizeof(nodes[0].msha));
        SHA256_Update(&sha256,&nn.hash,sizeof(nn.hash));
        SHA256_Update(&sha256,&nn.pk,sizeof(nn.pk));
        SHA256_Final(nn.msha,&sha256);
    }

    uint16_t add_node(user_t& ou,uint16_t unode,uint32_t user) {
        uint16_t peer=nodes.size();
        node nn;
        nn.mtim=now;
        //nn.users=1; //too early !!! must resize changed[] in put_users
        memcpy(nn.pk,ou.pkey,32);
        init_node_hash(nn);
        nodes.push_back(nn);
        user_t nu;
        init_user(nu,peer,0,BANK_MIN_UMASS+BANK_MIN_TMASS,ou.pkey,now,unode,user);
        put_user(nu,peer,0);
        //update_nodehash(peer);
        nodes[peer].weight=nu.weight;
        memcpy(nodes[peer].hash,nu.csum,SHA256_DIGEST_LENGTH); //???
        return(peer);
    }

    void put_node(user_t& ou,uint16_t peer,uint16_t node,uint32_t user) { //executed in block mode, no lock needed
        std::map<uint32_t,user_t> undo;
        user_t nu;
        get_user(nu,peer,0);
        undo[0]=nu;
        xor4(nodes[peer].hash,nu.csum);
        save_undo(peer,undo,0);
        init_user(nu,peer,0,nu.weight+BANK_MIN_UMASS+BANK_MIN_TMASS,ou.pkey,now,node,user); // weight does not change
        put_user(nu,peer,0);
        xor4(nodes[peer].hash,nu.csum);
        nodes[peer].mtim=now;
    }

    void init_user_hash(user_t& u) {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256,&nodes[0].msha,sizeof(nodes[0].msha));
        SHA256_Update(&sha256,&u.node,sizeof(u.node));
        SHA256_Update(&sha256,&u.user,sizeof(u.user));
        SHA256_Update(&sha256,&u.user,sizeof(u.time));
        SHA256_Update(&sha256,&u.pkey,sizeof(u.pkey));
        SHA256_Final(u.hash,&sha256);
    }

    void init_user(user_t& u,uint16_t peer,uint32_t uid,int64_t weight,uint8_t* pk,uint32_t when,uint16_t node,uint16_t user) {
        memset(&u,0,sizeof(user_t));
        u.msid=1; // always >0 to help identify holes in delta files
        u.time=when;
        u.node=node;
        u.user=user;
        u.lpath=when;
        u.rpath=when; //-START_AGE;
        u.weight=weight;
        memcpy(u.pkey,pk,SHA256_DIGEST_LENGTH);
        init_user_hash(u);
        user_csum(u,peer,uid);
    }

    void put_user(user_t& u,uint16_t peer,uint32_t uid) {
        char filename[64];
        sprintf(filename,"usr/%04X.dat",peer);
        int fd=open(filename,O_WRONLY|O_CREAT,0644);
        if(fd<0) {
            ELOG("ERROR, failed to open account file %s, fatal\n",filename);
            exit(-1);
        }
        lseek(fd,uid*sizeof(user_t),SEEK_SET);
        write(fd,&u,sizeof(user_t));
        close(fd);
        if(nodes[peer].users<=uid) { //consider locking
            nodes[peer].users=uid+1;
            nodes[peer].changed.resize(1+uid/64);
        }
    }

    void get_user(user_t& u,uint16_t peer,uint32_t uid) {
        char filename[64];
        sprintf(filename,"usr/%04X.dat",peer);
        int fd=open(filename,O_RDONLY);
        if(fd<0) {
            ELOG("ERROR, failed to open account file %s, fatal\n",filename);
            return;
        }
        lseek(fd,uid*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        close(fd);
    }

    bool check_user(uint16_t peer,uint32_t uid) {
        if(peer>=nodes.size()) {
            DLOG("ERROR, bad user %04X:%08X (bad node)\n",peer,uid);
            return(false);
        }
        if(nodes[peer].users<=uid) {
            DLOG("ERROR, bad user %04X:%08X [%08X]\n",peer,uid,nodes[peer].users);
            return(false);
        }
        return(true);
    }

    bool find_key(uint8_t* pkey,uint8_t* skey) {
        FILE* fp=fopen("key/key.txt","r");
        char line[255];
        if(fp==NULL) {
            return(false);
        }
        while(fgets(line,255,fp) != nullptr) {
            if(line[0]=='#' || strlen(line)<64) {
                continue;
            }
            hash_t pk;
            ed25519_text2key(skey,line,32); // do not send last hash
            ed25519_publickey(skey,pk);
            if(!memcmp(pkey,pk,32)) {
                fclose(fp);
                return(true);
            }
        }
        fclose(fp);
        return(false);
    }

    bool find_pkey(uint8_t* pk) {
        FILE* fp=fopen("key/key.txt","r");
        char line[255];
        if(fp==NULL){
            return(false);
        }
        while(fgets(line,255,fp) != nullptr) {
            if(line[0]=='#' || strlen(line)<64) {
                continue;
            }
            hash_t skey;
            ed25519_text2key(skey,line,32); // do not send last hash
            ed25519_publickey(skey,pk);
            fclose(fp);
            return(true);
        }
        fclose(fp);
        return(false);
    }

    void find_more_keys(uint8_t* pkey,std::map<uint16_t,nodekey_t> &nkeys) {
        char key[65];
        key[64]='\0';
        ed25519_key2text(key,pkey,32);
        char filename[128];
        sprintf(filename,"key/%s.txt",key);
        FILE* fp=fopen(filename,"r");
        if(fp==NULL) {
            return;
        }
        char line[128];
        line[127]='\0';
        while(fgets(line,127,fp) != nullptr) {
            if(line[0]=='#' || strlen(line)<64) {
                continue;
            }
            uint16_t node;
            nodekey_t nkey;
            if(sscanf(line,"%hd %64s",&node,key)!=2) {
                ELOG("ERROR, parsing %s: %s\n",filename,line);
                continue;
            }
            ed25519_text2key(nkey.skey,key,32);
            if(!node || node>=nodes.size()) {
                ELOG("ERROR, parsing %s, bad node %d\n",filename,node);
                continue;
            }
            //TODO, in the future we should run nkey.skey=skey^nkey.skey;
            ed25519_publickey(nkey.skey,nkey.pkey);
            if(memcmp(nkey.pkey,nodes[node].pk,32)) {
                ed25519_key2text(key,nkey.pkey,32);
                ELOG("ERROR, parsing %s, bad pkey %s for node %d\n",filename,key,node);
                continue;
            }
            DLOG("%04X signing for this node\n",node);
            nkeys[node]=nkey;
        }
        fclose(fp);
    }

    bool check_nodehash(uint16_t peer) {
        assert(peer);
        char filename[64];
        sprintf(filename,"usr/%04X.dat",peer);
        int fd=open(filename,O_RDONLY);
        if(fd<0) {
            ELOG("ERROR, failed to open account file %s, fatal\n",filename);
            exit(-1);
        }
        uint64_t csum[4]= {0,0,0,0};
        uint32_t end=nodes[peer].users;
        int64_t weight=0;
        for(uint32_t i=0; i<end; i++) {
            user_t u;
            if(sizeof(user_t)!=read(fd,&u,sizeof(user_t))) {
                ELOG("ERROR, failed to read %04X:%08X from %s\n",peer,i,filename);
                close(fd);
                exit(-1);
            }
            xor4(csum,u.csum);
            weight+=u.weight;
#ifdef DEBUG
            DLOG("USER:%04X:%08X m:%08X t:%08X s:%04X b:%04X u:%08X l:%08X r:%08X v:%016lX\n",
                 peer,i,u.msid,u.time,u.stat,u.node,u.user,u.lpath,u.rpath,u.weight);
            uint64_t usum[4]= {0,0,0,0};
            memcpy(usum,u.csum,sizeof(uint64_t)*4);
            user_csum(u,peer,i);
            if(memcmp(usum,u.csum,sizeof(uint64_t)*4)) {
                ELOG("ERROR, checksum failed for %04X:%08X from %s\n",peer,i,filename);
                close(fd);
                exit(-1);
            }
#endif
        }
        close(fd);
        if(nodes[peer].weight!=weight) {
            ELOG("ERROR: check_node: bad weight sum %016lX<>%016lX\n",nodes[peer].weight,weight);
            return(false);
        }
        if(memcmp(nodes[peer].hash,csum,4*sizeof(uint64_t))) {
            ELOG("ERROR: check_node: bad hash\n");
            return(false);
        }
        return(true);
    }

    void save_undo(uint16_t svid,std::map<uint32_t,user_t>& undo,uint32_t users) {
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/und/%04X.dat",now>>20,now&0xFFFFF,svid);
        int fd=open(filename,O_WRONLY|O_CREAT,0644);
        if(fd<0) {
            ELOG("ERROR, failed to open bank undo %04X, fatal\n",svid);
            exit(-1);
        }
        if(nodes[svid].users<=users) { //consider locking
            nodes[svid].users=users;
            nodes[svid].changed.resize(1+users/64);
        }
        for(auto it=undo.begin(); it!=undo.end(); it++) {
            uint32_t i=(it->first)/64;
            uint64_t j=1L<<((it->first)%64);
            assert(i<nodes[svid].changed.size());
            if(nodes[svid].changed[i]&j) {
                continue;
            }
            nodes[svid].changed[i]|=j;
            lseek(fd,(it->first)*sizeof(user_t),SEEK_SET);
            write(fd,&it->second,sizeof(user_t));
        }
        close(fd);
    }

    void clear_undo() {
        for(auto n=nodes.begin(); n!=nodes.end(); n++) {
            n->changed.clear();
        }
    }

    void get() {
        get(0);
    }
    void get(uint32_t path) {
        char filename[64]="servers.srv";
        if(path>0) {
            sprintf(filename,"blk/%03X/%05X/servers.srv",path>>20,path&0xFFFFF);
        }
        data_read(filename,true);
    }
    void read_servers(uint32_t path,uint8_t* &data,uint32_t &len,uint8_t* &hash) { // read file for user
        char filename[64]="servers.srv";
        if(path>0) {
            sprintf(filename,"blk/%03X/%05X/servers.srv",path>>20,path&0xFFFFF);
        }
        int fd=open(filename,O_RDONLY);
        if(fd>=0) {
            struct stat sb;
            fstat(fd,&sb);
            len=sb.st_size;
            data=(uint8_t*)malloc(4+len);
            read(fd,data+4,len);
            close(fd);
            //not a nice hack !!!
            hash=data+4
                 +sizeof(uint32_t) //version
                 +sizeof(uint32_t) //now
                 +sizeof(uint32_t) //msg
                 +sizeof(uint32_t) //nod
                 +sizeof(uint32_t) //div
                 +SHA256_DIGEST_LENGTH //oldhash
                 +SHA256_DIGEST_LENGTH //minhash
                 +SHA256_DIGEST_LENGTH //msghash
                 +SHA256_DIGEST_LENGTH //nodhash
                 +SHA256_DIGEST_LENGTH //viphash
                 +SHA256_DIGEST_LENGTH //nowhash
                 +sizeof(uint16_t) //vok
                 +sizeof(uint16_t) //vno
                 +sizeof(uint16_t) //vtot
                 +SHA256_DIGEST_LENGTH; //node[0].pk
        } else {
            len=0;
            data=(uint8_t*)malloc(4);
        }
        memcpy(data,&len,4);
    }
    void put() {
        char filename[64]="servers.srv";
        if(now>0) {
            sprintf(filename,"blk/%03X/%05X/servers.srv",now>>20,now&0xFFFFF);
        }
        if(!data_write(filename,true)) {
            ELOG("ERROR, failed to write servers to dir: %08X\n",now);
        }
        //std::ofstream ofs(filename);
        //if(ofs.is_open()){
        //	boost::archive::text_oarchive oa(ofs);
        //	oa << (*this);}
        //else{
        //	ELOG("ERROR, failed to write servers to dir: %08X\n",now);}
    }

    //! replaced by Parser::MsglistParser
//    void read_messagelist(uint32_t path,uint8_t* &data,uint32_t &len) { // read file for user
//        char filename[64];
//        sprintf(filename,"blk/%03X/%05X/msglist.dat",path>>20,path&0xFFFFF);
//        int fd=open(filename,O_RDONLY);
//        if(fd>=0) {
//            uint32_t msgnum=0;
//            read(fd,&msgnum,4);
//            //struct stat sb;
//            //fstat(fd,&sb);
//            //len=sb.st_size;
//            len=32+msgnum*(2+4+32);
//            data=(uint8_t*)malloc(4+len);
//            memcpy(data,&len,4);
//            read(fd,data+4,len);
//            close(fd);
//        } else {
//            len=0;
//            data=(uint8_t*)malloc(4);
//        }
//    }
    //TODO, *msg* should go to message.hpp
    /*bool msg_check(std::map<uint64_t,message_ptr>& map)
    {	hash_t hash;
        hashtree tree;
        uint32_t i=0;
        for(auto it=map.begin();it!=map.end();++it,i++){
            tree.update(it->second->sigh);}
        if(msg!=i){
            ELOG("ERROR, bad message numer\n");
            return(false);}
        tree.finish(hash);
        if(memcmp(hash,msghash,SHA256_DIGEST_LENGTH)){
            ELOG("ERROR, bad message hash\n");
            return(false);}
        return(true);
    }*/
    bool msgl_put(std::map<uint64_t,message_ptr>& map,char* cmphash) {
        hashtree tree;
        int i=0;
        for(auto it=map.begin(); it!=map.end(); ++it,i++) {
            tree.update(it->second->sigh);
        }
        msg=i;
        tree.finish(msghash);
        if(cmphash!=NULL && memcmp(cmphash,msghash,SHA256_DIGEST_LENGTH)) {
            return(false);
        }
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/msglist.dat",now>>20,now&0xFFFFF);
        int fd=open(filename,O_WRONLY|O_CREAT,0644);
        if(fd<0) { //trow or something :-)
            return(false);
        }
        uint32_t msgnum=map.size();
        write(fd,&msgnum,4); // added to make reading the file easier by user.cpp
        write(fd,msghash,SHA256_DIGEST_LENGTH);
        int n=0;
        for(auto it=map.begin(); it!=map.end(); it++) {
            write(fd,&it->second->svid,2); // we must later distinguish between double spend and normal messages ... maybe using msid==0xffffffff;
            write(fd,&it->second->msid,4);
            write(fd,it->second->sigh,SHA256_DIGEST_LENGTH); // maybe we have to set sigh=last_msg_sigh for double spend messages
            it->second->save_mnum(++n);
        } //save index for future use

//FIXME, remember to handle double spend messages !!!

        uint32_t hashes_size=tree.hashes.size()-1; // last is msghash
        if(!tree.hashes.size()) {
            hashes_size=0;
        }
        write(fd,&hashes_size,4); // needed only for debugging
        for(uint32_t i=0; i<hashes_size; i++) {
            write(fd,tree.hashes[i].hash,32);
        }
        close(fd);
        return(true);
    }
    int msgl_get(char* data) { // load only message list without hashtree
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/msglist.dat",now>>20,now&0xFFFFF);
        int fd=open(filename,O_RDONLY);
        if(fd<0) {
            return(0);
        }
        //uint32_t msgnum;
        //read(fd,&msgnum,4); // not used
        lseek(fd,4,SEEK_SET);
        int len=read(fd,data,SHA256_DIGEST_LENGTH+msg*(2+4+SHA256_DIGEST_LENGTH));
        close(fd);
        return(len);
    }
    void msgl_map(char* data,std::map<uint64_t,message_ptr>& map,uint16_t mysvid) {
        char* d=data+SHA256_DIGEST_LENGTH;
        for(uint16_t i=0; i<msg; i++,d+=2+4+SHA256_DIGEST_LENGTH) {
            message_ptr msg(new message((uint16_t*)(d),(uint32_t*)(d+2),(char*)(d+6),mysvid,now));
            msg->status |= MSGSTAT_VAL; //confirmed list of messages
            map[msg->hash.num]=msg;
        }
    }
    int msgl_load(std::map<uint64_t,message_ptr>& map,uint16_t mysvid) {
        char* data=(char*)malloc(SHA256_DIGEST_LENGTH+msg*(2+4+SHA256_DIGEST_LENGTH));
        //FIXME, do not use get to load messages and hashtree
        if(msgl_get(data)!=(int)(SHA256_DIGEST_LENGTH+msg*(2+4+SHA256_DIGEST_LENGTH))) {
            free(data);
            return(0);
        }
        if(memcmp(data,msghash,SHA256_DIGEST_LENGTH)) {
            free(data);
            return(0);
        }
        msgl_map(data,map,mysvid);
        free(data);
        return(1);
    }
    //! replaced by Parser::MsglistParser
//    bool msgl_hash_tree_get(uint16_t svid,uint32_t msid,uint32_t mnum,std::vector<hash_s>& hashes) {
//        if(!mnum) {
//            //refuse to provide hashpath without svid-msid index (mnum)
//            return(false);
//        }
//        if(!msg && !header_get()) {
//            return(false);
//        }
//        char filename[64];
//        sprintf(filename,"blk/%03X/%05X/msglist.dat",now>>20,now&0xFFFFF);
//        int fd=open(filename,O_RDONLY);
//        if(fd<0) {
//            DLOG("ERROR %s not found\n",filename);
//            return(false);
//        }
//        if((--mnum)%2) {
//#pragma pack(1)
//            struct {
//                hash_s ha;
//                uint16_t svid;
//                uint32_t msid;
//            } tmp;
//#pragma pack()
//            assert(sizeof(tmp)==32+2+4);
//            //uint8_t tmp[32+2+4]; // do not read own hash
//            lseek(fd,4+32+(2+4+32)*(mnum)-32,SEEK_SET);
//            read(fd,(char*)&tmp,32+2+4); // do not read own hash
//            //if(*(uint16_t*)(&tmp[32])!=svid || *(uint32_t*)(&tmp[32+2])!=msid)
//            if(tmp.svid!=svid || tmp.msid!=msid) {
//                DLOG("ERROR %s bad index %d %04X:%08X <> %04X:%08X\n",filename,mnum,svid,msid,
//                     tmp.svid,tmp.msid);
//                close(fd);
//                return(false);
//            }
//            DLOG("HASHTREE start %d + %d [max:%d]\n",mnum,mnum-1,msg);
//            //hashes.push_back(*(hash_s*)(&tmp[0]));
//            hashes.push_back(tmp.ha);
//        } else {
//#pragma pack(1)
//            struct {
//                uint16_t svid1;
//                uint32_t msid1;
//                hash_s ha1;
//                uint16_t svid2;
//                uint32_t msid2;
//                hash_s ha2;
//            }
//            tmp;
//#pragma pack()
//            assert(sizeof(tmp)==2+4+32+2+4+32);
//            //uint8_t tmp[2+4+32+2+4+32]; // do not read own hash
//            lseek(fd,4+32+(2+4+32)*(mnum),SEEK_SET);
//            read(fd,(char*)&tmp,2+4+32+2+4+32); // do not read own hash
//            //if(*(uint16_t*)(&tmp[0])!=svid || *(uint32_t*)(&tmp[2])!=msid)
//            if(tmp.svid1!=svid || tmp.msid1!=msid) {
//                DLOG("ERROR %s bad index %d %04X:%08X <> %04X:%08X\n",filename,mnum,svid,msid,
//                     tmp.svid1,tmp.msid1);
//                close(fd);
//                return(false);
//            }
//            if(mnum<msg-1) {
//                DLOG("HASHTREE start %d + %d [max:%d]\n",mnum,mnum+1,msg);
//                //hashes.push_back(*(hash_s*)(&tmp[2+4+32+2+4]));
//                hashes.push_back(tmp.ha2);
//            } else {
//                DLOG("HASHTREE start %d [max:%d]\n",mnum,msg);
//            }
//        }
//        uint32_t htot;
//        lseek(fd,4+32+(2+4+32)*msg,SEEK_SET);
//        read(fd,&htot,4); //needed only for debugging
//        std::vector<uint32_t>add;
//        hashtree tree;
//        tree.hashpath(mnum/2,(msg+1)/2,add);
//        for(auto n : add) {
//            DLOG("HASHTREE add %d\n",n);
//            assert(n<htot);
//            lseek(fd,4+32+(2+4+32)*msg+4+32*n,SEEK_SET);
//            hash_s phash;
//            read(fd,phash.hash,32);
//            hashes.push_back(phash);
//        }
//        close(fd);
//        //DEBUG only, confirm hash
//        hash_t nhash;
//        tree.hashpathrun(nhash,hashes);
//        if(memcmp(msghash,nhash,32)) {
//            DLOG("HASHTREE failed (path len:%d) to get msghash\n",(int)hashes.size());
//            return(false);
//        }
//        //add header hashes
//        //hashes.push_back(*(hash_s*)viphash); // removed to save on _TXS traffic
//        //hashes.push_back(*(hash_s*)oldhash); // removed to save on _TXS traffic
//        hash_s* hashmin=(hash_s*)minhash;
//        hashes.push_back(*hashmin);
//        //hashes.push_back(*(hash_s*)minhash);
//        //DEBUG only, confirm nowhash
//        //tree.addhash(nhash,viphash); // removed to save on _TXS traffic
//        //tree.addhash(nhash,oldhash); // removed to save on _TXS traffic
//        tree.addhash(nhash,minhash);
//        if(memcmp(nowhash,nhash,32)) {
//            DLOG("HASHTREE failed (path len:%d) to get nowhash\n",(int)hashes.size());
//            return(false);
//        }
//        return(true);
//    }

    uint16_t get_vipuno() {
      for(uint16_t i=1;i<nodes.size();i++){
            if(nodes[i].status & SERVER_UNO){
              return i;
            }
      }
      return 0;
    }

    void update_vipstatus() {
        uint32_t i;
        for(i=1; i<nodes.size(); i++) {
            nodes[i].status &= ~(SERVER_VIP|SERVER_UNO);
        }
        int len=0;
        char* buf=NULL;
//FIXME, handle problems with load_vip
        if(!load_vip(len,buf,viphash)) {
            update_viphash();
            load_vip(len,buf,viphash);
        }
        ELOG("VIPS %d:\n",(len/(2+32)));
        for(int i=0; i<len; i+=2+32) {
            uint16_t svid=*((uint16_t*)(&buf[i+4]));
            ELOG("VIP: %04X\n",svid);
            assert(svid>0 && svid<nodes.size());
            nodes[svid].status |= SERVER_VIP;
            if(!i) {
                nodes[svid].status |= SERVER_UNO;
            }
        }
        free(buf);
    }

    void update_viphash() {
        uint32_t i;
        vok=0;
        vno=0;

        std::vector<uint16_t> svid_rank;
        for(i=1; i<nodes.size(); i++) {
            if(nodes[i].status & SERVER_DBL ) {
                continue;
            }
            svid_rank.push_back(i);
        }
        std::stable_sort(svid_rank.begin(),svid_rank.end(),[this](const uint16_t& i,const uint16_t& j) {
            return(this->nodes[i].weight>this->nodes[j].weight);
        }); //fuck, lambda :-(
        hashtree tree(NULL); //FIXME, waste of space
        bzero(viphash,sizeof(hash_t));
        std::string data; //first VIP is master (SERVER_UNO)
        uint16_t svid=svid_rank[0];
        data.append((char*)&svid,2);
        data.append((char*)nodes[svid].pk,32);
        tree.addhash(viphash,nodes[svid].pk);
        std::map<uint16_t,hash_s> vipkeys;
        for(i=1; i<VIP_MAX&&i<svid_rank.size(); i++) { //other keys are sorted by svid
            svid=svid_rank[i];
            memcpy(vipkeys[svid].hash,nodes[svid].pk,32);
        }
        //vipkeys[svid]=(*((hash_s*)nodes[svid].pk));
        assert(i>0);
        for(auto it=vipkeys.begin(); it!=vipkeys.end(); it++) {
            data.append((char*)&it->first,2);
            data.append((char*)it->second.hash,32);
            tree.addhash(viphash,(uint8_t*)it->second.hash);
        }
        char hash[65];
        hash[64]='\0';
        ed25519_key2text(hash,viphash,32);
        char filename[128];
        sprintf(filename,"vip/%64s.vip",hash);
        int fd=open(filename,O_WRONLY|O_CREAT,0644);
        if(fd<0) {
            ELOG("ERROR opening %s, fatal\n",filename);
            exit(-1);
        }
        write(fd,data.c_str(),data.length());
        close(fd);
        vtot=(uint16_t)(i<VIP_MAX?i:VIP_MAX);
    }
    bool vip_check(uint8_t* viphash,uint8_t* vipkeys,int num) {
        hash_t newhash= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        hashtree tree(NULL); //FIXME, waste of space
        vipkeys+=2;
        for(int n=0; n<num; n++,vipkeys+=32+2) {
            //DLOG("...KEY:%016lX (%d<%d)\n",*((uint64_t*)vipkeys),n,num);
            tree.addhash(newhash,vipkeys);
        }
        return(!memcmp(newhash,viphash,32));
    }
    int vip_size(uint8_t* viphash) { //FIXME, add query cache
        char hash[65];
        hash[64]='\0';
        ed25519_key2text(hash,viphash,32);
        char filename[128];
        sprintf(filename,"vip/%64s.vip",hash);
        struct stat sb;
        stat(filename,&sb);
        return((int)sb.st_size/(2+32));
    }
    bool load_vip(int& len,char* &buf,uint8_t* vhash) {
        char hash[65];
        hash[64]='\0';
        ed25519_key2text(hash,vhash,32);
        char filename[128];
        sprintf(filename,"vip/%64s.vip",hash);
        int fd=open(filename,O_RDONLY,0644);
        if(fd<0) {
            ELOG("ERROR opening %s\n",filename);
            return(false);
        }
        struct stat sb;
        fstat(fd,&sb);
        len=sb.st_size;
        if(!len) {
            ELOG("ERROR empty %s\n",filename);
            close(fd);
            return(false);
        }
        buf=(char*)malloc(4+len);
        if(buf==NULL) {
            ELOG("ERROR malloc %d bytes for %s\n",4+len,filename);
            close(fd);
            return(false);
        }
        memcpy(buf,&len,4);
        read(fd,buf+4,len);
        close(fd);
        return(true);
    }

    void finish() {
        nod=nodes.size();
        SHA256_CTX sha256;
        update_viphash();
        hashtree tree;
        for(auto it=nodes.begin(); it<nodes.end(); it++) { // consider changing this to hashtree/hashcalendar
            DLOG("NOD: %08X %08x %08x %08X %08X %u %016lX %u\n",
                 (uint32_t)*((uint32_t*)&it->pk[0]),(uint32_t)*((uint32_t*)&it->hash[0]),(uint32_t)*((uint32_t*)&it->msha[0]),it->msid,it->mtim,it->status,it->weight,it->users);
            SHA256_Init(&sha256);
            SHA256_Update(&sha256,it->pk,sizeof(ed25519_public_key));
            SHA256_Update(&sha256,it->hash,SHA256_DIGEST_LENGTH);
            SHA256_Update(&sha256,it->msha,SHA256_DIGEST_LENGTH);
            SHA256_Update(&sha256,&it->msid,sizeof(uint32_t));
            SHA256_Update(&sha256,&it->mtim,sizeof(uint32_t));
            SHA256_Update(&sha256,&it->status,sizeof(uint32_t));
            SHA256_Update(&sha256,&it->weight,sizeof(it->weight));
            SHA256_Update(&sha256,&it->users,sizeof(uint32_t));
            hash_t hash;
            SHA256_Final(hash, &sha256);
            tree.update(hash);
        }
        tree.finish(nodhash);
        char hash[2*SHA256_DIGEST_LENGTH];
        ed25519_key2text(hash,nodhash,SHA256_DIGEST_LENGTH);
        ELOG("NODHASH sync %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        hashnow();
        //char filename[64];
        //sprintf(filename,"blk/%03X/%05X/servers.txt",now>>20,now&0xFFFFF);
        //std::ofstream sfs(filename);
        //boost::archive::text_oarchive sa(sfs);
        //sa << (*this);
        put();
        header_put();
        header_print();
        clear_undo();
    }

    void hashnow() {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256,&now,sizeof(uint32_t));
        SHA256_Update(&sha256,&msg,sizeof(uint32_t));
        SHA256_Update(&sha256,&nod,sizeof(uint32_t));
        SHA256_Update(&sha256,&div,sizeof(uint32_t));
        SHA256_Final(nowhash, &sha256);
        hashtree tree(NULL); //FIXME, waste of space
        tree.addhash(nowhash,nodhash);
        tree.addhash(nowhash,viphash);
        tree.addhash(nowhash,oldhash);
        memcpy(minhash,nowhash,32); // add message hash as last hash to reduce hash path for transactions
        tree.addhash(nowhash,msghash);
    }
    void loadlink(headlink_t& link,uint32_t path,char* oldh) {
        now=path;
        msg=link.msg;
        nod=link.nod;
        div=link.div;
        memcpy(oldhash,oldh,SHA256_DIGEST_LENGTH);
        memcpy(msghash,link.msghash,SHA256_DIGEST_LENGTH);
        memcpy(nodhash,link.nodhash,SHA256_DIGEST_LENGTH);
        memcpy(viphash,link.viphash,SHA256_DIGEST_LENGTH);
        hashnow();
    }
    void filllink(headlink_t& link) { //write to link
        link.msg=msg;
        link.nod=nod;
        link.div=div;
        memcpy(link.msghash,msghash,SHA256_DIGEST_LENGTH);
        memcpy(link.nodhash,nodhash,SHA256_DIGEST_LENGTH);
        memcpy(link.viphash,viphash,SHA256_DIGEST_LENGTH);
    }
    void loadhead(header_t& head) {
        now=head.now;
        msg=head.msg;
        nod=head.nod;
        div=head.div;
        memcpy(oldhash,head.oldhash,SHA256_DIGEST_LENGTH);
        memcpy(msghash,head.msghash,SHA256_DIGEST_LENGTH);
        memcpy(nodhash,head.nodhash,SHA256_DIGEST_LENGTH);
        memcpy(viphash,head.viphash,SHA256_DIGEST_LENGTH);
        hashnow();
    }
    void header(header_t& head) { //change this to fill head
        head.now=now;
        head.msg=msg;
        head.nod=nod;
        head.div=div;
        memcpy(head.oldhash,oldhash,SHA256_DIGEST_LENGTH);
        memcpy(head.msghash,msghash,SHA256_DIGEST_LENGTH);
        memcpy(head.nodhash,nodhash,SHA256_DIGEST_LENGTH);
        memcpy(head.viphash,viphash,SHA256_DIGEST_LENGTH);
        memcpy(head.nowhash,nowhash,SHA256_DIGEST_LENGTH);
        head.vok=vok;
        head.vno=vno;
    }
    int data_read(const char* filename,bool read_nodes) { //on change check read_servers()!!!
        uint32_t version;
        int fd=open(filename,O_RDONLY);
        if(fd<0) {
            DLOG("ERROR, failed to read servers from %s\n",filename);
            return(0);
        }
        read(fd,&version,sizeof(uint32_t)); // not used yet
        read(fd,&now,sizeof(uint32_t));
        read(fd,&msg,sizeof(uint32_t));
        read(fd,&nod,sizeof(uint32_t));
        read(fd,&div,sizeof(uint32_t));
        read(fd,oldhash,SHA256_DIGEST_LENGTH);
        read(fd,minhash,SHA256_DIGEST_LENGTH);
        read(fd,msghash,SHA256_DIGEST_LENGTH);
        read(fd,nodhash,SHA256_DIGEST_LENGTH);
        read(fd,viphash,SHA256_DIGEST_LENGTH);
        read(fd,nowhash,SHA256_DIGEST_LENGTH);
        read(fd,&vok,sizeof(uint16_t));
        read(fd,&vno,sizeof(uint16_t));
        read(fd,&vtot,sizeof(uint16_t));
        if(read_nodes && nod) {
            if(nodes.size()!=nod) {
                nodes.resize(nod);
            }
            uint32_t n=0;
            for(; n<nod; n++) {
                read(fd,nodes[n].pk,SHA256_DIGEST_LENGTH);
                read(fd,nodes[n].hash,SHA256_DIGEST_LENGTH);
                read(fd,nodes[n].msha,SHA256_DIGEST_LENGTH);
                read(fd,&nodes[n].msid,sizeof(uint32_t));
                read(fd,&nodes[n].mtim,sizeof(uint32_t));
                read(fd,&nodes[n].weight,sizeof(uint64_t));
                read(fd,&nodes[n].status,sizeof(uint32_t));
                read(fd,&nodes[n].users,sizeof(uint32_t));
                read(fd,&nodes[n].port,sizeof(uint32_t));
                read(fd,&nodes[n].ipv4,sizeof(uint32_t));
            }
        }
        close(fd);
        return(now);
    }
    int data_write(const char* filename,bool write_nodes) {
        uint32_t version=1;
        int fd=open(filename,O_WRONLY|O_CREAT,0644);
        if(fd<0) {
            ELOG("ERROR, failed to write file %s\n",filename);
            return(0);
        }
        write(fd,&version,sizeof(uint32_t)); // not used yet
        write(fd,&now,sizeof(uint32_t));
        write(fd,&msg,sizeof(uint32_t));
        write(fd,&nod,sizeof(uint32_t));
        write(fd,&div,sizeof(uint32_t));
        write(fd,oldhash,SHA256_DIGEST_LENGTH);
        write(fd,minhash,SHA256_DIGEST_LENGTH);
        write(fd,msghash,SHA256_DIGEST_LENGTH);
        write(fd,nodhash,SHA256_DIGEST_LENGTH);
        write(fd,viphash,SHA256_DIGEST_LENGTH);
        write(fd,nowhash,SHA256_DIGEST_LENGTH);
        write(fd,&vok,sizeof(uint16_t));
        write(fd,&vno,sizeof(uint16_t));
        write(fd,&vtot,sizeof(uint16_t));
        if(write_nodes && nod) {
            uint32_t n=0;
            for(; n<nod; n++) {
                write(fd,nodes[n].pk,SHA256_DIGEST_LENGTH);
                write(fd,nodes[n].hash,SHA256_DIGEST_LENGTH);
                write(fd,nodes[n].msha,SHA256_DIGEST_LENGTH);
                write(fd,&nodes[n].msid,sizeof(uint32_t));
                write(fd,&nodes[n].mtim,sizeof(uint32_t));
                write(fd,&nodes[n].weight,sizeof(uint64_t));
                write(fd,&nodes[n].status,sizeof(uint32_t));
                write(fd,&nodes[n].users,sizeof(uint32_t));
                write(fd,&nodes[n].port,sizeof(uint32_t));
                write(fd,&nodes[n].ipv4,sizeof(uint32_t));
            }
        }
        close(fd);
        return(1);
    }
    int header_get() {
        char filename[64];
        if(!now) {
            sprintf(filename,"blk/header.hdr");
        } else {
            sprintf(filename,"blk/%03X/%05X/header.hdr",now>>20,now&0xFFFFF);
        }
        uint32_t check=now;
        if(!data_read(filename,false)) {
            ELOG("ERROR, failed to read header %08X\n", now);
            return(0);
        }
        //std::ifstream ifs(filename);
        //if(ifs.is_open()){
        //	boost::archive::text_iarchive ia(ifs);
        //	ia >> now;
        //	ia >> msg;
        //	ia >> nod;
        //	ia >> div;
        //	ia >> oldhash;
        //	ia >> minhash; // only for hashtree reports
        //	ia >> msghash;
        //	ia >> nodhash;
        //	ia >> viphash;
        //	ia >> nowhash;
        //	ia >> vok;
        //	ia >> vno;}
        //else{
        //	ELOG("ERROR, failed to read header %08X\n",now);
        //	return(0);}
        if(check && check!=now) {
            ELOG("ERROR, failed to check header %08X<>%08X\n",now,check);
            return(0);
        }
        return(1);
    }
    bool save_nowhash(header_t& head) {
        char filename[64];
        sprintf(filename,"blk/%03X.now",head.now>>20);
        int fd=open(filename,O_WRONLY|O_CREAT,0644);
        if(fd<0) {
            DLOG("ERROR writing to %s\n",filename);
            return(false);
        }
        lseek(fd,((head.now&0xFFFFF)/BLOCKSEC)*32,SEEK_SET);
        if(write(fd,head.nowhash,32)!=32) {
            close(fd);
            return(false);
        }
        close(fd);
        return(true);
    }
    bool load_nowhash() {
        char filename[64];
        sprintf(filename,"blk/%03X.now",now>>20);
        int fd=open(filename,O_RDONLY);
        if(fd<0) {
            DLOG("ERROR opening %s\n",filename);
            return(false);
        }
        lseek(fd,((now&0xFFFFF)/BLOCKSEC)*32,SEEK_SET);
        if(read(fd,nowhash,32)!=32) {
            DLOG("ERROR reading %s hash %08X\n",filename,now);
            close(fd);
            return(false);
        }
        close(fd);
        return(true);
    }
    void header_put() {
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/header.hdr",now>>20,now&0xFFFFF);
        if(!data_write(filename,false)) {
            ELOG("ERROR, failed to write header\n");
        }
        //std::ofstream ofs(filename);
        //if(ofs.is_open()){
        //	boost::archive::text_oarchive oa(ofs);
        //	oa << now;
        //	oa << msg;
        //	oa << nod;
        //	oa << div;
        //	oa << oldhash;
        //	oa << minhash;
        //	oa << msghash;
        //	oa << nodhash;
        //	oa << viphash;
        //	oa << nowhash;
        //	oa << vok;
        //	oa << vno;}
        //else{
        //	ELOG("ERROR, failed to write header\n");}
    }
    void header_last() {
        char filename[64]="blk/header.hdr";
        if(!data_write(filename,false)) {
            ELOG("ERROR, failed to write last header\n");
        }
        //std::ofstream ofs("blk/header.txt");
        //if(ofs.is_open()){
        //	boost::archive::text_oarchive oa(ofs);
        //	oa << now;
        //	oa << msg;
        //	oa << nod;
        //	oa << div;
        //	oa << oldhash;
        //	oa << minhash;
        //	oa << msghash;
        //	oa << nodhash;
        //	oa << viphash;
        //	oa << nowhash;
        //	oa << vok;
        //	oa << vno;}
        //else{
        //	ELOG("ERROR, failed to write header\n");}
    }
    void header_print() {
        char hash[2*SHA256_DIGEST_LENGTH];
        ELOG("HEAD now:%08X msg:%08X nod:%08X div:%08X vok:%04X vno:%04X txs:%016lX\n",
             now,msg,nod,div,vok,vno,txs);
        ed25519_key2text(hash,oldhash,SHA256_DIGEST_LENGTH);
        ELOG("OLDHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,msghash,SHA256_DIGEST_LENGTH);
        ELOG("TXSHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,nodhash,SHA256_DIGEST_LENGTH);
        ELOG("NODHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,viphash,SHA256_DIGEST_LENGTH);
        ELOG("VIPHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,nowhash,SHA256_DIGEST_LENGTH);
        ELOG("NOWHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    }
    void header_print(header_t& head) {
        char hash[2*SHA256_DIGEST_LENGTH];
        ELOG("HEAD now:%08X msg:%08X nod:%08X div:%08X vok:%04X vno:%04X txs:%016lX\n",
             head.now,head.msg,head.nod,head.div,head.vok,head.vno,txs);
        ed25519_key2text(hash,head.oldhash,SHA256_DIGEST_LENGTH);
        ELOG("OLDHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,head.msghash,SHA256_DIGEST_LENGTH);
        ELOG("TXSHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,head.nodhash,SHA256_DIGEST_LENGTH);
        ELOG("NODHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,head.viphash,SHA256_DIGEST_LENGTH);
        ELOG("VIPHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,head.nowhash,SHA256_DIGEST_LENGTH);
        ELOG("NOWHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    }
    void save_signature(uint32_t path,uint16_t svid,uint8_t* sig,bool ok) {
        extern boost::mutex siglock;
        boost::lock_guard<boost::mutex> lock(siglock);
        char filename[64];
        bzero(filename,64);
        char filepath[64];
        bzero(filepath,64);
        //sprintf(filename,"blk/%03X/%05X/signatures.txt",path>>20,path&0xFFFFF);
        //char hash[4*SHA256_DIGEST_LENGTH];
        //ed25519_key2text(hash,sig,2*SHA256_DIGEST_LENGTH);
        //FILE *fp=fopen(filename,"a");
        //if(fp==NULL){
        //	DLOG("ERROR, creating blk/%03X/%05X/signatures.ok, very bad :-(",path>>20,path&0xFFFFF);
        //	return;}
        //fprintf(fp,"%04X\t%.*s\t%d\n",(uint32_t)svid,4*SHA256_DIGEST_LENGTH,hash,ok);
        //fclose(fp);

        sprintf(filepath,"blk/%03X/%05X/",path>>20,path&0xFFFFF);

        if(ok) {
            //vok++;
            sprintf(filename,"blk/%03X/%05X/signatures.ok",path>>20,path&0xFFFFF);
            DLOG("BLOCK ok (%s)\n",filename);
        } else {
//FIXME, no point to save "no-" signatures without corresponding block
            //vno++;
            sprintf(filename,"blk/%03X/%05X/signatures.no",path>>20,path&0xFFFFF);
            DLOG("BLOCK differs (%s)\n",filename);
        }

        DLOG("SIGNATURE,  signatures in %s\n",filename);

        svsi_t old;
        svsi_t da;
        memcpy(da,&svid,2);
        memcpy(da+2,sig,2*SHA256_DIGEST_LENGTH);
        //int fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);

        if(!boost::filesystem::exists(filepath)){
            DLOG("SIGNATURE,  CREATEBLOCK %s\n",filename);
            blockdir(path);
        }

        int fd=open(filename,O_RDWR|O_CREAT,0644);
        if(fd<0) {
            DLOG("ERROR, failed to save signatures in %s\n",filename);
            assert(0);
            return;
        }
        int num=0;
        while(read(fd,&old,sizeof(svsi_t))==sizeof(svsi_t)) {
            num++;
            if(!memcmp(&old,&da,sizeof(svsi_t))) {
                close(fd);
                return;
            }
        }
        write(fd,da,sizeof(svsi_t));
        close(fd);
        if(path==now) {
            if(ok) {
                if(num+1!=vok) {
                    vok=num+1;
                }
            } else {
                if(num+1!=vno) {
                    vno=num+1;
                }
            }
        }
    }
    bool get_signatures(uint32_t path,uint8_t* &data,uint32_t &nok)
    {
        // does not use any local data
        extern boost::mutex siglock;
        boost::lock_guard<boost::mutex> lock(siglock);

        int fd;
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/signatures.ok",path>>20,path&0xFFFFF);

        DLOG("SIGNATURE, get signatures in %s\n",filename);

        fd=open(filename,O_RDONLY);
        if(fd>=0) {
            struct stat sb;
            fstat(fd,&sb);
            if(!sb.st_size) {
                close(fd);
                return false;
            }
            nok=sb.st_size/sizeof(svsi_t);
            data=(uint8_t*)malloc(8+nok*sizeof(svsi_t));
            memcpy(data+0,&path,4);
            memcpy(data+4,&nok,4);
            read(fd,data+8,nok*sizeof(svsi_t));
            close(fd);
            return true;
        } else {
            nok=0;
            data=NULL;
            return false;
        }
    }
    void read_signatures(uint32_t path,uint8_t* &data,uint32_t &nok,uint32_t &nno)
    {
        if(!get_signatures(path,data,nok)) {
            nno=0;
            return;
        }

        extern boost::mutex siglock;
        boost::lock_guard<boost::mutex> lock(siglock);


        int fd;
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/signatures.no",path>>20,path&0xFFFFF);
        fd=open(filename,O_RDONLY);

        DLOG("SIGNATURE,  read signatures2 in %s\n",filename);

        if(fd>=0) {
            struct stat sb;
            fstat(fd,&sb);
            nno=sb.st_size/sizeof(svsi_t);
            data=(uint8_t*)realloc(data,8+nok*sizeof(svsi_t)+4+nno*sizeof(svsi_t));
            read(fd,data+8+nok*sizeof(svsi_t)+4,nno*sizeof(svsi_t));
            close(fd);
        } else {
            nno=0;
            data=(uint8_t*)realloc(data,8+nok*sizeof(svsi_t)+4);
        }
        memcpy(data+8+nok*sizeof(svsi_t),&nno,4);
    }
    bool get_signatures(uint32_t path,uint8_t* data,int nok,int nno)
    {
        // does not use any local data

        extern boost::mutex siglock;
        boost::lock_guard<boost::mutex> lock(siglock);        

        int fd;
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/signatures.ok",path>>20,path&0xFFFFF);
        fd=open(filename,O_RDONLY);

        DLOG("SIGNATURE, Get signatures from %s\n",filename);

        if(fd>=0) {
            read(fd,data,sizeof(svsi_t)*nok);
            //vok=read(fd,ok,sizeof(svsi_t)*VIP_MAX);
            //vok/=sizeof(svsi_t);
            close(fd);
        } else {
            return false;
        }
        if(!nno) {
            return true;
        }
        sprintf(filename,"blk/%03X/%05X/signatures.no",path>>20,path&0xFFFFF);
        fd=open(filename,O_RDONLY);
        if(fd>=0) {
            read(fd,data+sizeof(svsi_t)*nok,sizeof(svsi_t)*nno);
            //vno=read(fd,ok,sizeof(svsi_t)*VIP_MAX);
            //vno/=sizeof(svsi_t);
            close(fd);
        } else {
            return false;
        }
        return true;
    }
    void put_signatures(header_t& head,svsi_t* svsi) { //FIXME, save in a file named based on nowhash ".../sig_%.64s.dat"
        extern boost::mutex siglock;
        boost::lock_guard<boost::mutex> lock(siglock);

        int fd;
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/signatures.ok",head.now>>20,head.now&0xFFFFF);
        fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(fd>=0) {
            write(fd,&svsi[0],sizeof(svsi_t)*head.vok);
            close(fd);
        }
        sprintf(filename,"blk/%03X/%05X/signatures.no",head.now>>20,head.now&0xFFFFF);
        fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(fd>=0) {
            write(fd,&svsi[head.vok],sizeof(svsi_t)*head.vno);
            close(fd);
        }
    }
    void del_signatures() {
        extern boost::mutex siglock;
        boost::lock_guard<boost::mutex> lock(siglock);

        char filename[64];
        sprintf(filename,"blk/%03X/%05X/signatures.ok",now>>20,now&0xFFFFF);
        int fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(fd>=0) {
            close(fd);
        }
    }
    void check_signatures(header_t& head,svsi_t* svsi,bool save) {
        int i=0,j=0;
        for(; i<head.vok; i++) {
            uint8_t* data=(uint8_t*)&svsi[i];
            uint16_t svid;
            memcpy(&svid,data,2);
            if(!(nodes[0].status & SERVER_FST)){
                if(svid>=nodes.size()){
                    DLOG("ERROR, bad server %04X in signatures\n",svid);
                    continue;
                }
                if(!(nodes[svid].status & SERVER_VIP)){
                    DLOG("WARNING, signature from non VIP server %04X ignored\n",svid);
                    continue;
                }
                int error=ed25519_sign_open((const unsigned char*)&head,sizeof(header_t)-4,nodes[svid].pk,data+2);
                if(error){
                    char hash[4*SHA256_DIGEST_LENGTH];
                    ed25519_key2text(hash,data+2,2*SHA256_DIGEST_LENGTH);
                    ELOG("BLOCK SIGNATURE failed %.*s (%d)\n",4*SHA256_DIGEST_LENGTH,hash,svid);
                    header_print(head);
                    continue;
                } else if(save){
                    save_signature(head.now,svid,data+2,true);
                }
                if(j!=i){
                    memcpy(svsi+j,svsi+i,sizeof(svsi_t));
                }
            }
            j++;
        }
        head.vno+=head.vok;
        head.vok=j;
//FIXME, no point to send or check no-signatures ... they will never validate
        for(; i<head.vno; i++) {
            uint8_t* data=(uint8_t*)&svsi[i];
            uint16_t svid;
            memcpy(&svid,data,2);
            if(svid>=nodes.size()) {
                continue;
            }
            int error=ed25519_sign_open((const unsigned char*)&head,sizeof(header_t)-4,nodes[svid].pk,data+2);
            if(error) {
                continue;
            }
            if(j!=i) {
                memcpy(svsi+j,svsi+i,sizeof(svsi_t));
            }
            j++;
        }
        head.vno=j-head.vok;
        ELOG("READ block signatures confirmed:%d failed:%d\n",head.vok,head.vno);
    }

    bool copy_nodes(node_t* peer_node,uint16_t peer_srvn) {
        uint32_t i;
        if(nodes.size()!=peer_srvn) {
            return(false);
        }
        //FIXME, we should lock this somehow :-(, then the for test could be faster
        for(i=0; i<peer_srvn&&i<nodes.size(); i++) {
            memcpy(peer_node[i].pk,nodes[i].pk,sizeof(ed25519_public_key));
            memcpy(peer_node[i].hash,nodes[i].hash,SHA256_DIGEST_LENGTH);
            memcpy(peer_node[i].msha,nodes[i].msha,SHA256_DIGEST_LENGTH);
            peer_node[i].msid=nodes[i].msid;
            peer_node[i].mtim=nodes[i].mtim;
            peer_node[i].status=nodes[i].status;
            peer_node[i].weight=nodes[i].weight;
            peer_node[i].users=nodes[i].users;
            peer_node[i].port=nodes[i].port;
            peer_node[i].ipv4=nodes[i].ipv4;
        }
        if(nodes.size()!=i) {
            return(false);
        }
        return(true);
    }

    //FIXME, run this check on local nodes; do not load as parameter
    //FIXME, create a single nodhash calculation function
    int check_nodes(node_t* peer_node,uint16_t peer_srvn,uint8_t* peer_nodehash) {
        int i=0;
        for(auto no=nodes.begin(); no!=nodes.end()&&i<peer_srvn; no++,i++) {
            if(memcmp(no->pk,peer_node[i].pk,sizeof(ed25519_public_key))) {
                ELOG("WARNING key mismatch for peer %04X\n",i);
            }
        }
        uint8_t tmphash[SHA256_DIGEST_LENGTH]; // hash of nodes
        SHA256_CTX sha256;
        hashtree tree;
        for(i=0; i<peer_srvn; i++) { // consider changing this to hashtree/hashcalendar
            DLOG("NOD: %08x %08x %08x %08X %08X %u %016lX %u\n",
                 (uint32_t)*((uint32_t*)&peer_node[i].pk[0]),(uint32_t)*((uint32_t*)&peer_node[i].hash[0]),(uint32_t)*((uint32_t*)&peer_node[i].msha[0]),
                 peer_node[i].msid,peer_node[i].mtim,peer_node[i].status,peer_node[i].weight,peer_node[i].users);
            SHA256_Init(&sha256);
            SHA256_Update(&sha256,peer_node[i].pk,sizeof(ed25519_public_key));
            SHA256_Update(&sha256,peer_node[i].hash,SHA256_DIGEST_LENGTH);
            SHA256_Update(&sha256,peer_node[i].msha,SHA256_DIGEST_LENGTH);
            SHA256_Update(&sha256,&peer_node[i].msid,sizeof(uint32_t));
            SHA256_Update(&sha256,&peer_node[i].mtim,sizeof(uint32_t));
            SHA256_Update(&sha256,&peer_node[i].status,sizeof(uint32_t));
            SHA256_Update(&sha256,&peer_node[i].weight,sizeof(int64_t));
            SHA256_Update(&sha256,&peer_node[i].users,sizeof(uint32_t));
            hash_t hash;
            SHA256_Final(hash, &sha256);
            tree.update(hash);
        }
        tree.finish(tmphash);
        char hash[2*SHA256_DIGEST_LENGTH];
        ed25519_key2text(hash,tmphash,SHA256_DIGEST_LENGTH);
        ELOG("NODHASH sync %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        return(memcmp(tmphash,peer_nodehash,SHA256_DIGEST_LENGTH));
    }

    void overwrite(header_t& head,node_t* peer_node) {
        now=head.now;
        msg=head.msg;
        nod=head.nod;
        div=head.div;
        memcpy(oldhash,head.oldhash,SHA256_DIGEST_LENGTH);
        memcpy(msghash,head.msghash,SHA256_DIGEST_LENGTH);
        memcpy(nodhash,head.nodhash,SHA256_DIGEST_LENGTH);
        memcpy(viphash,head.viphash,SHA256_DIGEST_LENGTH);
        memcpy(nowhash,head.nowhash,SHA256_DIGEST_LENGTH);
        vok=head.vok;
        vno=head.vno;
        if(nodes.size()<nod) {
            nodes.resize(nod);
        }
        for(uint32_t i=0; i<nod; i++) { // consider changing this to hashtree/hashcalendar
            memcpy(nodes[i].pk,peer_node[i].pk,sizeof(ed25519_public_key));
            memcpy(nodes[i].hash,peer_node[i].hash,SHA256_DIGEST_LENGTH);
            memcpy(nodes[i].msha,peer_node[i].msha,SHA256_DIGEST_LENGTH);
            nodes[i].msid=peer_node[i].msid;
            nodes[i].mtim=peer_node[i].mtim;
            nodes[i].status=peer_node[i].status;
            nodes[i].weight=peer_node[i].weight;
            nodes[i].users=peer_node[i].users;
            nodes[i].port=peer_node[i].port;
            nodes[i].ipv4=peer_node[i].ipv4;
        }
    }

    uint32_t nextblock() { //returns period_start
        DLOG("NEXT BLOCK");

        now+=BLOCKSEC;
        int num=(now/BLOCKSEC)%BLOCKDIV;
        if(!num) {
            uint64_t sum=0;
            for(uint16_t n=1; n<nodes.size(); n++) {
                sum+=nodes[n].weight;
            }
            if(sum<TOTALMASS) {
                if(sum<TOTALMASS/2) {
                    ELOG("WARNING, very small account sum %016lX\n",sum);
                }
                //div=(uint32_t)((double)(TOTALMASS-sum)/(double)(sum>>32));
                div=(uint32_t)((double)(TOTALMASS-sum)/(double)(sum>>16));
            } else {
                div=0;
            }
            //ELOG("NEW DIVIDEND %08X (%.8f)\n",div,(float)(div)/0xFFFFFFFF);
            ELOG("NEW DIVIDEND %08X (%.8f) (diff:%016lX,div:%.8lf)\n",
                 div,(float)(div)/0xFFFF,TOTALMASS-sum,(double)(TOTALMASS-sum)/(double)sum);
        }
        blockdir();
        //change log directory
        {
            extern boost::mutex flog;
            extern FILE* stdlog;
            char filename[32];
            sprintf(filename,"blk/%03X/%05X/log.txt",now>>20,now&0xFFFFF);
            flog.lock();
            fclose(stdlog);
            stdlog=fopen(filename,"a");
            uint32_t ntime=time(NULL);
            fprintf(stdlog,"START: %08X\n",ntime);
            flog.unlock();
        }
        //FIXME, update VIP status now
        update_vipstatus();
        msg=0;
        txs=0;
        return(now-num*BLOCKSEC);
    }

    void blockdir() { //not only dir ... should be called blockstart
        DLOG("BLOCKDIR NOW %04X\n", now);
        char pathname[64];
        sprintf(pathname,"blk/%03X",now>>20);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X",now>>20,now&0xFFFFF);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X/und",now>>20,now&0xFFFFF);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X/log",now>>20,now&0xFFFFF);
        mkdir(pathname,0755);
        for(auto n=nodes.begin(); n!=nodes.end(); n++) {
            n->changed.resize(1+n->users/64);
        }
        uint32_t nextnow=now+BLOCKSEC;
        sprintf(pathname,"blk/%03X",nextnow>>20);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X",nextnow>>20,nextnow&0xFFFFF); // to make space for moved files
        mkdir(pathname,0755);
    }

    void blockdir(uint32_t blockTime) { //not only dir ... should be called blockstart
        DLOG("BLOCKDIR TIME %04X", blockTime);

        char pathname[64];
        sprintf(pathname,"blk/%03X",blockTime>>20);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X",blockTime>>20,blockTime&0xFFFFF);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X/und",blockTime>>20,blockTime&0xFFFFF);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X/log",blockTime>>20,blockTime&0xFFFFF);
        mkdir(pathname,0755);
        for(auto n=nodes.begin(); n!=nodes.end(); n++) {
            n->changed.resize(1+n->users/64);
        }
        uint32_t nextnow=blockTime+BLOCKSEC;
        sprintf(pathname,"blk/%03X",nextnow>>20);
        mkdir(pathname,0755);
        sprintf(pathname,"blk/%03X/%05X",nextnow>>20,nextnow&0xFFFFF); // to make space for moved files
        mkdir(pathname,0755);
    }

    void clean_old(uint16_t svid) {
        char pat[8];
        ELOG("CLEANING by %04X\n",svid);
        sprintf(pat,"%04X",svid);
        for(int i=MAX_UNDO; i<MAX_UNDO+100; i+=10) {
#ifdef DEBUG
            uint32_t path=now-i*BLOCKSEC*(0x400/BLOCKSEC);
#else
            uint32_t path=now-i*BLOCKSEC;
#endif
            int fd,dd;
            struct dirent* dat;
            char pathname[64];
            DIR* dir;
            sprintf(pathname,"blk/%03X/%05X",path>>20,path&0xFFFFF);
            dir=opendir(pathname);
            if(dir==NULL) {
                DLOG("UNLINK: no such dir %s\n",pathname);
                continue;
            }
            dd=dirfd(dir);
            if((fd=openat(dd,"clean.txt",O_RDONLY))>=0) {
                DLOG("UNLINK: dir %s clean\n",pathname);
                close(fd);
                closedir(dir);
                continue;
            }
            while((dat=readdir(dir))!=NULL) { // remove messages from other nodes
                if(dat->d_type==DT_REG &&
                        strlen(dat->d_name)==20 &&
                        dat->d_name[16]=='.' &&
                        (dat->d_name[17]=='m' || dat->d_name[17]=='u') &&
                        (dat->d_name[18]=='s' || dat->d_name[18]=='n') &&
                        (dat->d_name[19]=='g' || dat->d_name[19]=='d') &&
                        (dat->d_name[3]!=pat[0] ||
                         dat->d_name[4]!=pat[1] ||
                         dat->d_name[5]!=pat[2] ||
                         dat->d_name[6]!=pat[3])) {
                    DLOG("UNLINK: %s/%s\n",pathname,dat->d_name);
                    unlinkat(dd,dat->d_name,0);
                }
            }
            unlinkat(dd,"delta.txt",0);
            unlinkat(dd,"log.txt",0);
            sprintf(pathname,"blk/%03X/%05X/log",path>>20,path&0xFFFFF);
            boost::filesystem::path logd(pathname);
            boost::filesystem::remove_all(logd);
            sprintf(pathname,"blk/%03X/%05X/und",path>>20,path&0xFFFFF);
            boost::filesystem::path undd(pathname);
            boost::filesystem::remove_all(undd);
            if((fd=openat(dd,"clean.txt",O_WRONLY|O_CREAT,0644))>=0) {
                close(fd);
            }
            closedir(dir);
        }
    }

    uint32_t nowh32(void) {
        uint32_t* p=(uint32_t*)nowhash;
        return(*p);
    }

    uint32_t nowh64(void) {
        uint64_t* p=(uint64_t*)nowhash;
        return(*p);
    }

//private:
//	//boost::mutex mtx_; can not be coppied :-(
//	friend class boost::serialization::access;
//	template<class Archive> void serialize(Archive & ar, const unsigned int version)
//	{	//uint32_t old; // remove later
//		try{
//			ar & now;
//			if(version>2) ar & msg;
//			if(version>2) ar & nod;
//			if(version>3) ar & div;
//			if(version>0) ar & oldhash;
//			if(version>4) ar & minhash;
//			if(version>1) ar & msghash;
//			if(version>1) ar & nodhash;
//			if(version>4) ar & viphash;
//			if(version>0) ar & nowhash;
//			if(version>2) ar & vok;
//			if(version>2) ar & vno;
//			if(version>4) ar & vtot;
//			ar & nodes; }
//		catch (std::exception& e){
//			ELOG("ERROR in boost serialize: %s\n",e.what());
//			exit(-1);}
//
//	}
};
//BOOST_CLASS_VERSION(servers, 5)

#endif // SERVERS_HPP
