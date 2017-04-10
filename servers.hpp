#ifndef SERVERS_HPP
#define SERVERS_HPP

//FIXME, concider separating header and nodes into header.hpp and node.hpp
#pragma pack(1)
/*typedef struct headlink_s { // header links sent when syncing
	uint32_t msg; // number of transactions in block, FIXME, should be uint16_t
	uint32_t nod; // number of nodes in block, this could be uint16_t later, FIXME, should be uint16_t
	uint32_t div; // dividend
	uint8_t msghash[SHA256_DIGEST_LENGTH]; // hash of transactions
	//uint8_t txshash[SHA256_DIGEST_LENGTH]; // hash of transactions
	uint8_t nodhash[SHA256_DIGEST_LENGTH]; // hash of nodes
} headlink_t;*/
typedef uint8_t svsi_t[2+(2*SHA256_DIGEST_LENGTH)]; // server_id + signature
typedef struct node_s {
	ed25519_public_key pk; // public key
	uint8_t hash[SHA256_DIGEST_LENGTH]; // hash of accounts (XOR of checksums)
	//uint8_t xash[SHA256_DIGEST_LENGTH]; // hash of additional data
	uint8_t msha[SHA256_DIGEST_LENGTH]; // hash of last message
	uint32_t msid; // last message, server closed if msid==0xffffffff
	uint32_t mtim; // time of last message
	//uint32_t mtim; // block of last message, FIXME, consider adding
	 int64_t weight; // weight in units of 8TonsOfMoon (-1%) (in MoonBlocks)
	uint32_t status; // placeholder for future status settings, can include hash type
	//uint32_t weight; // weight in units of 8TonsOfMoon (-1%) (in MoonBlocks)
	uint32_t users; // placeholder for users (size)
	uint32_t port;
	uint32_t ipv4;
	//char host[64];
} node_t;
#pragma pack()
	
class node
{
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
	{	//bzero(host,64);
	}
	ed25519_public_key pk; // public key
	uint8_t ohash[SHA256_DIGEST_LENGTH]; // hash of accounts
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
private:
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version)
	{	
		ar & pk;
	 	if(version<3){	ar & ohash; memcpy(hash,ohash,32);}
		else{		ar & hash;}
		//if(version>0) ar & msha;
		ar & msha;
	 	ar & msid;
	 	ar & mtim;
		ar & weight; //changed order !!!
	 	ar & status; //changed order !!!
	 	//if(version==0) ar & in;
	 	//if(version==0) ar & out;
	 	//if(version==0) ar & addr;
	 	//if(version==1) ar & host;
	 	//if(version==1) ar & port;
	 	////ar & status;
		//if(version>0) ar & weight;
		////ar & weight;
	 	if(version>1) ar & users;
	 	if(version>1) ar & port;
	 	if(version>1) ar & ipv4;
	}
};
BOOST_CLASS_VERSION(node, 3)
class servers // also a block
{
public:
	uint32_t now; // start time of the block
	//uint32_t old; // time of last block
	uint32_t msg; // number of transactions in block, FIXME, should be uint16_t
	uint32_t nod; // number of nodes in block, FIXME, should be uint16_t
        uint32_t div; // dividend
	uint8_t oldhash[SHA256_DIGEST_LENGTH]; // previous hash
	uint8_t msghash[SHA256_DIGEST_LENGTH]; // hash of transactions
	//uint8_t txshash[SHA256_DIGEST_LENGTH]; // hash of transactions
	uint8_t nodhash[SHA256_DIGEST_LENGTH]; // hash of nodes
	uint8_t nowhash[SHA256_DIGEST_LENGTH]; // current hash
	uint16_t vok;
	uint16_t vno;
	std::vector<node> nodes; //FIXME, this vector can fail to allocate RAM, do tests with more than 32k nodes!
	//std::vector<void*> users; // place holder for user accounts
	servers():
		now(0),
		//old(0), //FIXME, remove, oldhash will be '000...000' if old!=now-BLOCKSEC
		msg(0),
		nod(0),
		div(0),
		oldhash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		msghash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		//txshash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		nodhash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		nowhash{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		vok(0),
		vno(0) 
	{}

	int init(uint32_t newnow)
	{	uint16_t num=0;
		 int64_t stw=TOTALMASS/(nodes.size()-1)*0.99; //FIXME, remove initial tax of 1%
		uint64_t sum=0;
                now=newnow;
		blockdir();
		for(auto it=nodes.begin();it<nodes.end();it++,num++){
			memset(it->msha,0xff,SHA256_DIGEST_LENGTH); //TODO, start servers this way too
			memcpy(it->msha,&num,2); // always start with a unique hash
			it->msid=0;
			it->mtim=now; // blockchain start time in nodes[0]
			it->status=0;
			if(num){
				if(num<=VIP_MAX){
					it->status|=SERVER_VIP;}
				it->users=1;
				// create the first user
				user_t u;
				init_user(u,num,0,stw-num,it->pk,now,num,0);
				put_user(u,num,0);
				//update_nodehash(num);
				memcpy(it->hash,u.csum,SHA256_DIGEST_LENGTH);
				it->weight=u.weight;
				sum+=u.weight;}
			else{
				bzero(it->hash,SHA256_DIGEST_LENGTH);
				bzero(it->pk,SHA256_DIGEST_LENGTH);
				it->users=0;
				it->weight=0;}}
		fprintf(stderr,"INIT: weight diff: %016lX\n",TOTALMASS-sum);
		//nodes.begin()->weight=TOTALMASS-sum;
		assert(num>0);
		finish();
		return(num<VIP_MAX?num:VIP_MAX);
	}

	void xor4(uint64_t* to,uint64_t* from)
	{	to[0]^=from[0];
	 	to[1]^=from[1];
	 	to[2]^=from[2];
	 	to[3]^=from[3];
	}

	void user_csum(user_t& u,uint16_t peer,uint32_t uid)
	{	SHA256_CTX sha256;
		SHA256_Init(&sha256);
		SHA256_Update(&sha256,&u,sizeof(user_t)-4*sizeof(uint64_t));
		SHA256_Update(&sha256,&peer,sizeof(uint16_t));
		SHA256_Update(&sha256,&uid,sizeof(uint32_t));
		SHA256_Final((uint8_t*)u.csum,&sha256);
	}

	uint16_t add_node(user_t& ou,uint16_t unode,uint32_t user)
	{	uint16_t peer=nodes.size();
		node nn;
		nn.mtim=now;
		nn.users=1;
		memcpy(nn.pk,ou.pkey,32);
		memset(nn.msha,0xff,SHA256_DIGEST_LENGTH); //TODO, start servers this way too
		memcpy(nn.msha,&peer,2); // always start with a unique hash
	 	nodes.push_back(nn);
		user_t nu;
		init_user(nu,peer,0,BANK_MIN_TMASS,ou.pkey,now,unode,user);
		put_user(nu,peer,0);
		//update_nodehash(peer);
		memcpy(nn.hash,ou.csum,SHA256_DIGEST_LENGTH); //???
		return(peer);
	}

	void put_node(user_t& ou,uint16_t peer,uint16_t node,uint32_t user) //executed in block mode, no lock needed
	{	std::map<uint32_t,user_t> undo;
	 	user_t nu;
		get_user(nu,peer,0);
		undo[0]=nu;
		xor4(nodes[peer].hash,nu.csum);
		save_undo(peer,undo,0);
		init_user(nu,peer,0,nu.weight+BANK_MIN_TMASS,ou.pkey,now,node,user); // weight does not change
		put_user(nu,peer,0);
		xor4(nodes[peer].hash,nu.csum);
		nodes[peer].mtim=now;
	}

	void init_user(user_t& u,uint16_t peer,uint32_t uid,int64_t weight,uint8_t* pk,uint32_t when,uint16_t node,uint16_t user)
	{	memset(&u,0,sizeof(user_t));
		memset(u.hash,0xff,SHA256_DIGEST_LENGTH); //TODO, start servers this way too
		memcpy(u.hash,&uid,4); // always start with a unique hash
		memcpy(u.hash+4,&peer,2); // always start with a unique hash
		u.msid=1; // always >0 to help identify holes in delta files
                u.time=when;
		u.node=node;
		u.user=user;
                u.lpath=when;
                u.rpath=when; //-START_AGE;
		u.weight=weight;
		memcpy(u.pkey,pk,SHA256_DIGEST_LENGTH);
		user_csum(u,peer,uid);
        }

	void put_user(user_t& u,uint16_t peer,uint32_t uid)
	{	char filename[64];
		sprintf(filename,"usr/%04X.dat",peer);
		int fd=open(filename,O_WRONLY|O_CREAT,0644);
		if(fd<0){ std::cerr << "ERROR, failed to open account file "<<filename<<", fatal\n";
			exit(-1);}
		lseek(fd,uid*sizeof(user_t),SEEK_SET);
		write(fd,&u,sizeof(user_t));
		close(fd);
		if(nodes[peer].users<=uid){ //consider locking
			nodes[peer].users=uid+1;
			nodes[peer].changed.resize(1+uid/64);}
	}

	void get_user(user_t& u,uint16_t peer,uint32_t uid)
	{	char filename[64];
		sprintf(filename,"usr/%04X.dat",peer);
		int fd=open(filename,O_RDONLY);
		if(fd<0){ std::cerr << "ERROR, failed to open account file "<<filename<<", fatal\n";
			exit(-1);}
		lseek(fd,uid*sizeof(user_t),SEEK_SET);
		read(fd,&u,sizeof(user_t));
		close(fd);
	}

	bool check_user(uint16_t peer,uint32_t uid)
	{	if(peer>=nodes.size()){
			fprintf(stderr,"ERROR, bad user %04X:%08X (bad node)\n",peer,uid);
			return(false);}
	 	if(nodes[peer].users<=uid){
			fprintf(stderr,"ERROR, bad user %04X:%08X [%08X]\n",peer,uid,nodes[peer].users);
			return(false);}
		return(true);
	}

	bool find_key(uint8_t* pkey,uint8_t* skey)
	{	FILE* fp=fopen("key/key.txt","r");
		char line[128];
		if(fp==NULL){
			return(false);}
		while(fgets(line,128,fp)>0){
			if(line[0]=='#' || strlen(line)<64){
				continue;}
			hash_t pk;
			ed25519_text2key(skey,line,32); // do not send last hash
			ed25519_publickey(skey,pk);
			if(!memcmp(pkey,pk,32)){
				fclose(fp);
				return(true);}}
		fclose(fp);
		return(false);
	}

	bool check_nodehash(uint16_t peer)
	{	assert(peer);
                char filename[64];
		sprintf(filename,"usr/%04X.dat",peer);
		int fd=open(filename,O_RDONLY);
		if(fd<0){ std::cerr << "ERROR, failed to open account file "<<filename<<", fatal\n";
			exit(-1);}
		uint64_t csum[4]={0,0,0,0};
		uint32_t end=nodes[peer].users;
		 int64_t weight=0;
		for(uint32_t i=0;i<end;i++){
			user_t u;
			if(sizeof(user_t)!=read(fd,&u,sizeof(user_t))){
				fprintf(stderr,"ERROR, failed to read %04X:%08X from %s\n",peer,i,filename);
				exit(-1);}
//FIXME, remove
        fprintf(stderr,"USER:%04X:%08X m:%08X t:%08X s:%04X b:%04X u:%08X l:%08X r:%08X v:%016lX\n",
          peer,i,u.msid,u.time,u.stat,u.node,u.user,u.lpath,u.rpath,u.weight);
			xor4(csum,u.csum);
			weight+=u.weight;}
		close(fd);
		if(nodes[peer].weight!=weight){
			fprintf(stderr,"ERROR: check_node: bad weight sum\n");
			return(false);}
		if(memcmp(nodes[peer].hash,csum,4*sizeof(uint64_t))){
			fprintf(stderr,"ERROR: check_node: bad hash\n");
			return(false);}
		return(true);
	}

	void save_undo(uint16_t svid,std::map<uint32_t,user_t>& undo,uint32_t users)
	{	char filename[64];
		sprintf(filename,"blk/%03X/%05X/und/%04X.dat",now>>20,now&0xFFFFF,svid);
		int fd=open(filename,O_WRONLY|O_CREAT,0644);
		if(fd<0){
			std::cerr<<"ERROR, failed to open bank undo "<<svid<<", fatal\n";
			exit(-1);}
		if(nodes[svid].users<=users){ //consider locking
			nodes[svid].users=users;
			nodes[svid].changed.resize(1+users/64);}
		for(auto it=undo.begin();it!=undo.end();it++){
			int i=(it->first)/64;
			int j=1<<((it->first)%64);
			if(nodes[svid].changed[i]&j){
				return;}
			nodes[svid].changed[i]|=j;
			lseek(fd,(it->first)*sizeof(user_t),SEEK_SET);
			write(fd,&it->second,sizeof(user_t));}
		close(fd);
	}

	void clear_undo()
	{	for(auto n=nodes.begin();n!=nodes.end();n++){
			n->changed.clear();}
	}

	void get()
	{	std::ifstream ifs("servers.txt");
		if(ifs.is_open()){
			boost::archive::text_iarchive ia(ifs);
			ia >> (*this);}
		std::cout << std::to_string((int)nodes.size()) << " servers loaded\n";
	}
	void get(uint32_t path)
	{	char filename[64];
	 	if(path==0){
			get();
			return;}
		sprintf(filename,"blk/%03X/%05X/servers.txt",path>>20,path&0xFFFFF);
		std::ifstream ifs(filename);
		if(ifs.is_open()){
			boost::archive::text_iarchive ia(ifs);
			ia >> (*this);}
		assert(now==path);
		std::cout << std::to_string((int)nodes.size()) << " servers loaded\n";
	}
	void put() //uint32_t path) //FIXME, not used
	{	char filename[64]="servers.txt";
	 	if(now>0){
			sprintf(filename,"blk/%03X/%05X/servers.txt",now>>20,now&0xFFFFF);}
		std::ofstream ofs(filename);
		if(ofs.is_open()){
			boost::archive::text_oarchive oa(ofs);
			oa << (*this);}
		else{
			std::cerr<<"ERROR, failed to write servers to dir:"<<now<<"\n";}

	}
	void hashmsg(std::map<uint64_t,message_ptr>& map)
	{	hashtree tree;
		int i=0;
		for(auto it=map.begin();it!=map.end();++it,i++){
			tree.update(it->second->sigh);}
		msg=i;
		tree.finish(msghash);
	}
	bool msg_check(std::map<uint64_t,message_ptr>& map)
	{	hash_t hash;
		hashtree tree;
		uint32_t i=0;
		for(auto it=map.begin();it!=map.end();++it,i++){
			tree.update(it->second->sigh);}
		if(msg!=i){
			fprintf(stderr,"ERROR, bad message numer\n");
			return(false);}
		tree.finish(hash);
		if(memcmp(hash,msghash,SHA256_DIGEST_LENGTH)){
			fprintf(stderr,"ERROR, bad message hash\n");
			return(false);}
		return(true);
	}
	void msg_put(std::map<uint64_t,message_ptr>& map) // FIXME, add this also for std::map<uint64_t,message_ptr>
	{	hashmsg(map);
	 	char filename[64];
		sprintf(filename,"blk/%03X/%05X/msglist.dat",now>>20,now&0xFFFFF);
		int fd=open(filename,O_WRONLY|O_CREAT,0644);
		if(fd<0){ //trow or something :-)
			return;}
		write(fd,msghash,SHA256_DIGEST_LENGTH);
		for(auto it=map.begin();it!=map.end();it++){
			write(fd,&it->second->svid,2); // we must later distinguish between double spend and normal messages ... maybe using msid==0xffffffff;
			write(fd,&it->second->msid,4);
			write(fd,it->second->sigh,SHA256_DIGEST_LENGTH);} // maybe we have to set sigh=last_msg_sigh for double spend messages
		close(fd);
	}
	int msg_get(char* data)
	{	char filename[64];
		sprintf(filename,"blk/%03X/%05X/msglist.dat",now>>20,now&0xFFFFF);
		int fd=open(filename,O_RDONLY);
		if(fd<0){
			return(0);}
		int len=read(fd,data,SHA256_DIGEST_LENGTH+msg*(2+4+SHA256_DIGEST_LENGTH));
		close(fd);
		return(len);
	}
	void msg_map(char* data,std::map<uint64_t,message_ptr>& map,uint16_t mysvid)
	{	char* d=data+SHA256_DIGEST_LENGTH;
		for(uint16_t i=0;i<msg;i++,d+=2+4+SHA256_DIGEST_LENGTH){
			message_ptr msg(new message((uint16_t*)(d),(uint32_t*)(d+2),(char*)(d+6),mysvid,now)); // uint32_t alignment ???
			map[msg->hash.num]=msg;}
	}
	int load_msglist(std::map<uint64_t,message_ptr>& map,uint16_t mysvid)
	{	char* data=(char*)malloc(SHA256_DIGEST_LENGTH+msg*(2+4+SHA256_DIGEST_LENGTH));
		if(msg_get(data)!=(int)(SHA256_DIGEST_LENGTH+msg*(2+4+SHA256_DIGEST_LENGTH))){
			free(data);
			return(0);}
		if(memcmp(data,msghash,SHA256_DIGEST_LENGTH)){
			free(data);
			return(0);}
		msg_map(data,map,mysvid);
		free(data);
		return(1);
	}

	void finish()
	{	nod=nodes.size();
		SHA256_CTX sha256;
		hashtree tree;
		for(auto it=nodes.begin();it<nodes.end();it++){ // consider changing this to hashtree/hashcalendar
			fprintf(stderr,"NOD: %08x %08x %08x %08X %08X %u %016lX %u\n",
				(uint32_t)*((uint32_t*)&it->pk[0]),(uint32_t)*((uint32_t*)&it->hash[0]),(uint32_t)*((uint32_t*)&it->msha[0]),it->msid,it->mtim,it->status,it->weight,it->users);
			SHA256_Init(&sha256);
			SHA256_Update(&sha256,it->pk,sizeof(ed25519_public_key));
			SHA256_Update(&sha256,it->hash,SHA256_DIGEST_LENGTH);
			SHA256_Update(&sha256,it->msha,SHA256_DIGEST_LENGTH);
			SHA256_Update(&sha256,&it->msid,sizeof(uint32_t));
			SHA256_Update(&sha256,&it->mtim,sizeof(uint32_t));
			SHA256_Update(&sha256,&it->status,sizeof(uint32_t));
			SHA256_Update(&sha256,&it->weight,sizeof(uint32_t));
			SHA256_Update(&sha256,&it->users,sizeof(uint32_t));
			hash_t hash;
			SHA256_Final(hash, &sha256);
			tree.update(hash);}
		tree.finish(nodhash);
		char hash[2*SHA256_DIGEST_LENGTH];
		ed25519_key2text(hash,nodhash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"NODHASH sync %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		hashnow();
		char filename[64];
		sprintf(filename,"blk/%03X/%05X/servers.txt",now>>20,now&0xFFFFF);
		std::ofstream sfs(filename);
		boost::archive::text_oarchive sa(sfs);
		sa << (*this);
		header_put();
		header_print();
		clear_undo();
	}

	void hashnow()
	{	SHA256_CTX sha256;
	 	SHA256_Init(&sha256);
		SHA256_Update(&sha256,&now,sizeof(uint32_t));
		SHA256_Update(&sha256,&msg,sizeof(uint32_t));
		SHA256_Update(&sha256,&nod,sizeof(uint32_t));
		SHA256_Update(&sha256,&div,sizeof(uint32_t));
		SHA256_Final(nowhash, &sha256);
		hashtree tree(NULL); //FIXME, waste of space
		tree.addhash(nowhash,nodhash);
		tree.addhash(nowhash,msghash);
		tree.addhash(nowhash,oldhash);
	}
	void loadlink(headlink_t& link,uint32_t path,char* oldh)
	{	now=path;
		msg=link.msg;
		nod=link.nod;
		div=link.div;
		memcpy(oldhash,oldh,SHA256_DIGEST_LENGTH);
		memcpy(msghash,link.msghash,SHA256_DIGEST_LENGTH);
		memcpy(nodhash,link.nodhash,SHA256_DIGEST_LENGTH);
		hashnow();
	}
	void filllink(headlink_t& link)//write to link
	{	link.msg=msg;
		link.nod=nod;
		link.div=div;
		memcpy(link.msghash,msghash,SHA256_DIGEST_LENGTH);
		memcpy(link.nodhash,nodhash,SHA256_DIGEST_LENGTH);
	}
	void loadhead(header_t& head)
	{	now=head.now;
		msg=head.msg;
		nod=head.nod;
		div=head.div;
		memcpy(oldhash,head.oldhash,SHA256_DIGEST_LENGTH);
		memcpy(msghash,head.msghash,SHA256_DIGEST_LENGTH);
		memcpy(nodhash,head.nodhash,SHA256_DIGEST_LENGTH);
		hashnow();
	}
	void header(header_t& head)//change this to fill head
	{	head.now=now;
		head.msg=msg;
		head.nod=nod;
		head.div=div;
		memcpy(head.oldhash,oldhash,SHA256_DIGEST_LENGTH);
		memcpy(head.msghash,msghash,SHA256_DIGEST_LENGTH);
		memcpy(head.nodhash,nodhash,SHA256_DIGEST_LENGTH);
		memcpy(head.nowhash,nowhash,SHA256_DIGEST_LENGTH);
		head.vok=vok;
		head.vno=vno;
	}
	int header_get()
	{	char filename[64];
		sprintf(filename,"blk/%03X/%05X/header.txt",now>>20,now&0xFFFFF);
		std::ifstream ifs(filename);
		uint32_t check=0;
		if(ifs.is_open()){
			boost::archive::text_iarchive ia(ifs);
			ia >> check;
			ia >> msg;
			ia >> nod;
			ia >> div;
			ia >> oldhash;
			ia >> msghash;
			ia >> nodhash;
			ia >> nowhash;
			ia >> vok;
			ia >> vno;}
		else{
			std::cerr<<"ERROR, failed to read header\n";
			return(0);}
		if(check!=now){
			std::cerr<<"ERROR, failed to check header\n";
			return(0);}
		return(1);
	}
	void header_put()
	{	char filename[64];
		sprintf(filename,"blk/%03X/%05X/header.txt",now>>20,now&0xFFFFF);
		std::ofstream ofs(filename);
		if(ofs.is_open()){
			boost::archive::text_oarchive oa(ofs);
			oa << now;
			oa << msg;
			oa << nod;
			oa << div;
			oa << oldhash;
			oa << msghash;
			oa << nodhash;
			oa << nowhash;
			oa << vok;
			oa << vno;}
		else{
			std::cerr<<"ERROR, failed to write header\n";}
	}
	void header_print()
	{	char hash[2*SHA256_DIGEST_LENGTH];
	 	fprintf(stderr,"HEAD now:%08X msg:%08X nod:%08X div:%08X vok:%04X vno:%04X\n",now,msg,nod,div,vok,vno);
		ed25519_key2text(hash,oldhash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"OLDHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		ed25519_key2text(hash,msghash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"TXSHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		ed25519_key2text(hash,nodhash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"NODHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		ed25519_key2text(hash,nowhash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"NOWHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
	}
	void header_print(header_t& head)
	{	char hash[2*SHA256_DIGEST_LENGTH];
	 	fprintf(stderr,"HEAD now:%08X msg:%08X nod:%08X div:%08X vok:%04X vno:%04X\n",head.now,head.msg,head.nod,head.div,head.vok,head.vno);
		ed25519_key2text(hash,head.oldhash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"OLDHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		ed25519_key2text(hash,head.msghash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"TXSHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		ed25519_key2text(hash,head.nodhash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"NODHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		ed25519_key2text(hash,head.nowhash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"NOWHASH %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
	}
	void save_signature(uint16_t svid,uint8_t* sig,bool ok)
	{	char filename[64];
		sprintf(filename,"blk/%03X/%05X/signatures.txt",now>>20,now&0xFFFFF);
		char hash[4*SHA256_DIGEST_LENGTH];
		ed25519_key2text(hash,sig,2*SHA256_DIGEST_LENGTH);
		//mtx_.lock();
		FILE *fp=fopen(filename,"a");
		fprintf(fp,"%04X\t%.*s\t%d\n",(uint32_t)svid,4*SHA256_DIGEST_LENGTH,hash,ok);
		fclose(fp);
		//mtx_.unlock();
		//assert(vok+vno<VIP_MAX-1);
		if(ok){
			std::cerr << "BLOCK ok\n";
			vok++;
			sprintf(filename,"blk/%03X/%05X/signatures.ok",now>>20,now&0xFFFFF);}
		else{
//FIXME, no point to save no signatures without corresponding block
			std::cerr << "BLOCK differs\n";
			vno++;
			sprintf(filename,"blk/%03X/%05X/signatures.no",now>>20,now&0xFFFFF);}
		svsi_t da;
		memcpy(da,&svid,2);
		memcpy(da+2,sig,2*SHA256_DIGEST_LENGTH);
		int fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);
                write(fd,da,sizeof(svsi_t));
		close(fd);
	}
	void get_signatures(uint32_t path,uint8_t* data,int nok,int nno) // does not use any local data
	{	int fd;
		char filename[64];
		sprintf(filename,"blk/%03X/%05X/signatures.ok",path>>20,path&0xFFFFF);
		fd=open(filename,O_RDONLY);
		if(fd>=0){
			read(fd,data,sizeof(svsi_t)*nok);
			//vok=read(fd,ok,sizeof(svsi_t)*VIP_MAX);
			//vok/=sizeof(svsi_t);
			close(fd);}
		sprintf(filename,"blk/%03X/%05X/signatures.no",path>>20,path&0xFFFFF);
		fd=open(filename,O_RDONLY);
		if(fd>=0){
			read(fd,data+sizeof(svsi_t)*nok,sizeof(svsi_t)*nno);
			//vno=read(fd,ok,sizeof(svsi_t)*VIP_MAX);
			//vno/=sizeof(svsi_t);
			close(fd);}
	}
	void put_signatures(header_t& head,svsi_t* svsi) //FIXME, save in a file named based on nowhash ".../sig_%.64s.dat"
	{	int fd;
		char filename[64];
		sprintf(filename,"blk/%03X/%05X/signatures.ok",head.now>>20,head.now&0xFFFFF);
		fd=open(filename,O_WRONLY|O_CREAT,0644);
		if(fd>=0){
			write(fd,&svsi[0],sizeof(svsi_t)*head.vok);
			close(fd);}
		sprintf(filename,"blk/%03X/%05X/signatures.no",head.now>>20,head.now&0xFFFFF);
		fd=open(filename,O_WRONLY|O_CREAT,0644);
		if(fd>=0){
			write(fd,&svsi[head.vok],sizeof(svsi_t)*head.vno);
			close(fd);}
	}
	void check_signatures(header_t& head,svsi_t* svsi)
	{	int i=0,j=0;
		for(;i<head.vok;i++){
			uint8_t* data=(uint8_t*)&svsi[i];
			uint16_t svid;
			memcpy(&svid,data,2);
			if(svid>=nodes.size()){
				continue;}
			int error=ed25519_sign_open((const unsigned char*)&head,sizeof(header_t)-4,nodes[svid].pk,data+2);
			if(error){
				char hash[4*SHA256_DIGEST_LENGTH];
				ed25519_key2text(hash,data+2,2*SHA256_DIGEST_LENGTH);
				fprintf(stderr,"BLOCK SIGNATURE failed %.*s (%d)\n",4*SHA256_DIGEST_LENGTH,hash,svid);
				header_print(head);
				continue;}
			if(j!=i){
				memcpy(svsi+j,svsi+i,sizeof(svsi_t));}
			j++;}
		head.vno+=head.vok;
		head.vok=j;
//FIXME, no point to send or check no-signatures ... they will never validate
		for(;i<head.vno;i++){
			uint8_t* data=(uint8_t*)&svsi[i];
			uint16_t svid;
			memcpy(&svid,data,2);
			if(svid>=nodes.size()){
				continue;}
			int error=ed25519_sign_open((const unsigned char*)&head,sizeof(header_t)-4,nodes[svid].pk,data+2);
			if(error){
				continue;}
			if(j!=i){
				memcpy(svsi+j,svsi+i,sizeof(svsi_t));}
			j++;}
		head.vno=j-head.vok;
          	std::cerr << "READ block signatures confirmed:"<<head.vok<<" failed:"<<head.vno<<"\n";
	}

	bool copy_nodes(node_t* peer_node,uint16_t peer_srvn)
	{	uint32_t i;
		if(nodes.size()!=peer_srvn){
			return(false);}
		//FIXME, we should lock this somehow :-(, then the for test could be faster
		for(i=0;i<peer_srvn&&i<nodes.size();i++){
			memcpy(peer_node[i].pk,nodes[i].pk,sizeof(ed25519_public_key));
			memcpy(peer_node[i].hash,nodes[i].hash,SHA256_DIGEST_LENGTH);
			memcpy(peer_node[i].msha,nodes[i].msha,SHA256_DIGEST_LENGTH);
			peer_node[i].msid=nodes[i].msid;
			peer_node[i].mtim=nodes[i].mtim;
			peer_node[i].status=nodes[i].status;
			peer_node[i].weight=nodes[i].weight;
			peer_node[i].users=nodes[i].users;
			peer_node[i].port=nodes[i].port;
			peer_node[i].ipv4=nodes[i].ipv4;}
	 	if(nodes.size()!=i){
			return(false);}
		return(true);
	}

	//FIXME, run this check on local nodes; do not load as parameter
	//FIXME, create a single nodhash calculation function
	int check_nodes(node_t* peer_node,uint16_t peer_srvn,uint8_t* peer_nodehash)
	{	int i=0;
		for(auto no=nodes.begin();no!=nodes.end()&&i<peer_srvn;no++,i++){
			if(memcmp(no->pk,peer_node[i].pk,sizeof(ed25519_public_key))){
				std::cerr<<"WARNING key mismatch for peer "<<i<<"\n";}}
		uint8_t tmphash[SHA256_DIGEST_LENGTH]; // hash of nodes
		SHA256_CTX sha256;
		hashtree tree;
		for(i=0;i<peer_srvn;i++){ // consider changing this to hashtree/hashcalendar
			fprintf(stderr,"NOD: %08x %08x %08x %08X %08X %u %016lX %u\n",
				(uint32_t)*((uint32_t*)&peer_node[i].pk[0]),(uint32_t)*((uint32_t*)&peer_node[i].hash[0]),(uint32_t)*((uint32_t*)&peer_node[i].msha[0]),
				peer_node[i].msid,peer_node[i].mtim,peer_node[i].status,peer_node[i].weight,peer_node[i].users);
			SHA256_Init(&sha256);
			SHA256_Update(&sha256,peer_node[i].pk,sizeof(ed25519_public_key));
			SHA256_Update(&sha256,peer_node[i].hash,SHA256_DIGEST_LENGTH);
			SHA256_Update(&sha256,peer_node[i].msha,SHA256_DIGEST_LENGTH);
			SHA256_Update(&sha256,&peer_node[i].msid,sizeof(uint32_t));
			SHA256_Update(&sha256,&peer_node[i].mtim,sizeof(uint32_t));
			SHA256_Update(&sha256,&peer_node[i].status,sizeof(uint32_t));
			SHA256_Update(&sha256,&peer_node[i].weight,sizeof(uint32_t));
			SHA256_Update(&sha256,&peer_node[i].users,sizeof(uint32_t));
			hash_t hash;
			SHA256_Final(hash, &sha256);
			tree.update(hash);}
		tree.finish(tmphash);
		char hash[2*SHA256_DIGEST_LENGTH];
		ed25519_key2text(hash,tmphash,SHA256_DIGEST_LENGTH);
		fprintf(stderr,"NODHASH sync %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
		return(memcmp(tmphash,peer_nodehash,SHA256_DIGEST_LENGTH));
	}

	void overwrite(header_t& head,node_t* peer_node)
	{ 	now=head.now;
		msg=head.msg;
		nod=head.nod;
		div=head.div;
		memcpy(oldhash,head.oldhash,SHA256_DIGEST_LENGTH);
		memcpy(msghash,head.msghash,SHA256_DIGEST_LENGTH);
		memcpy(nodhash,head.nodhash,SHA256_DIGEST_LENGTH);
		memcpy(nowhash,head.nowhash,SHA256_DIGEST_LENGTH);
		vok=head.vok;
		vno=head.vno;
		if(nodes.size()<nod){
			nodes.resize(nod);}
		for(uint32_t i=0;i<nod;i++){ // consider changing this to hashtree/hashcalendar
			memcpy(nodes[i].pk,peer_node[i].pk,sizeof(ed25519_public_key));
			memcpy(nodes[i].hash,peer_node[i].hash,SHA256_DIGEST_LENGTH);
			memcpy(nodes[i].msha,peer_node[i].msha,SHA256_DIGEST_LENGTH);
			nodes[i].msid=peer_node[i].msid;
			nodes[i].mtim=peer_node[i].mtim;
			nodes[i].status=peer_node[i].status;
			nodes[i].weight=peer_node[i].weight;
			nodes[i].users=peer_node[i].users;
			nodes[i].port=peer_node[i].port;
			nodes[i].ipv4=peer_node[i].ipv4;}
	}

	int update_vip()
	{	uint32_t i;
		vok=0;
		vno=0;
		std::vector<uint16_t> svid_rank;
		for(i=0;i<nodes.size();i++){
			if(nodes[i].status & SERVER_DBL){
				continue;}
			nodes[i].status &= ~SERVER_VIP;
			svid_rank.push_back(i);}
		std::sort(svid_rank.begin(),svid_rank.end(),[this](const uint16_t& i,const uint16_t& j){return(this->nodes[i].weight>this->nodes[j].weight);}); //fuck, lambda :-(
		for(i=0;i<VIP_MAX&&i<svid_rank.size();i++){
			nodes[svid_rank[i]].status |= SERVER_VIP;}
		assert(i>0);
		return(i<VIP_MAX?i:VIP_MAX);
	}

	uint32_t nextblock() //returns period_start
	{	now+=BLOCKSEC;
		int num=(now/BLOCKSEC)%BLOCKDIV;
		if(!num){
			uint64_t sum=0;
			for(uint16_t n=1;n<nodes.size();n++){
				sum+=nodes[n].weight;}
			if(sum<TOTALMASS){
				if(sum<TOTALMASS/2){
					fprintf(stderr,"WARNING, very small account sum %016lX\n",sum);}
				//div=(uint32_t)((double)(TOTALMASS-sum)/(double)(sum>>32));
				div=(uint32_t)((double)(TOTALMASS-sum)/(double)(sum>>16));}
			else{
				div=0;}
			//fprintf(stderr,"NEW DIVIDEND %08X (%.8f)\n",div,(float)(div)/0xFFFFFFFF);
			fprintf(stderr,"NEW DIVIDEND %08X (%.8f) (diff:%016lX,div:%.8lf)\n",
                          div,(float)(div)/0xFFFF,TOTALMASS-sum,(double)(TOTALMASS-sum)/(double)sum);}
		blockdir();
		return(now-num*BLOCKSEC);
	}

	void blockdir() //not only dir ... should be called blockstart
	{	char pathname[64];
		sprintf(pathname,"blk/%03X",now>>20);
		mkdir(pathname,0755);
		sprintf(pathname,"blk/%03X/%05X",now>>20,now&0xFFFFF);
		mkdir(pathname,0755);
		sprintf(pathname,"blk/%03X/%05X/und",now>>20,now&0xFFFFF);
		mkdir(pathname,0755);
		sprintf(pathname,"blk/%03X/%05X/log",now>>20,now&0xFFFFF);
		mkdir(pathname,0755);
		for(auto n=nodes.begin();n!=nodes.end();n++){
			n->changed.resize(1+n->users/64);}
                uint32_t nextnow=now+BLOCKSEC;
		sprintf(pathname,"blk/%03X/%05X",nextnow>>20,nextnow&0xFFFFF); // to make space for moved files
		mkdir(pathname,0755);
	}

	void clean_old(uint16_t svid)
	{	char pat[8];
		fprintf(stderr,"CLEANING by %04X\n",svid);
		sprintf(pat,"%04X",svid);
		for(int i=1;i<5;i++){
			uint32_t path=now-i*BLOCKDIV*BLOCKSEC; // remove from last div periods
			int fd,dd;
			struct dirent* dat;
		 	char pathname[64];
			DIR* dir;
			sprintf(pathname,"blk/%03X/%05X",path>>20,path&0xFFFFF);
			dir=opendir(pathname);
			if(dir==NULL){
				fprintf(stderr,"UNLINK: no such dir %s\n",pathname);
				continue;}
			dd=dirfd(dir);
			if((fd=openat(dd,"clean.txt",O_RDONLY))>=0){
				fprintf(stderr,"UNLINK: dir %s clean\n",pathname);
				close(fd);
				closedir(dir);
				continue;}
			while((dat=readdir(dir))!=NULL){ // remove messages from other nodes
				if(dat->d_type==DT_REG &&
				   strlen(dat->d_name)==20 &&
				   dat->d_name[16]=='.' &&
				   dat->d_name[17]=='m' &&
				   dat->d_name[18]=='s' &&
				   dat->d_name[19]=='g' &&
				   (dat->d_name[3]!=pat[0] ||
				    dat->d_name[4]!=pat[1] ||
				    dat->d_name[5]!=pat[2] ||
				    dat->d_name[6]!=pat[3])){
					fprintf(stderr,"UNLINK: %s/%s\n",pathname,dat->d_name);
					unlinkat(dd,dat->d_name,0);}}
			if((fd=openat(dd,"clean.txt",O_WRONLY|O_CREAT,0644))>=0){
				close(fd);}
			closedir(dir);}
	}

private:
	//boost::mutex mtx_; can not be coppied :-(
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version)
	{	//uint32_t old; // remove later
		ar & now;
		if(version>2) ar & msg;
		if(version>2) ar & nod;
		if(version>3) ar & div;
		if(version>0) ar & oldhash;
		if(version>1) ar & msghash;
		if(version>1) ar & nodhash;
		if(version>0) ar & nowhash;
		if(version>2) ar & vok;
		if(version>2) ar & vno;
		ar & nodes;
	}
};
BOOST_CLASS_VERSION(servers, 4)

#endif // SERVERS_HPP
