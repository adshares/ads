#ifndef HASH_HPP
#define HASH_HPP

#define MAXTREE 32

class hashtree
{
public:
	hashtree()
	{	bzero(hash_loaded,MAXTREE*sizeof(uint8_t));
		bzero(hash_,MAXTREE*sizeof(hash_s));
	}

	hashtree(void*) {} // no initialization

	~hashtree() {}

	uint32_t bits(uint32_t v)
	{	uint32_t c;
#if INTPTR_MAX == INT64_MAX
		if(v<0x4000){ // option 1, for at most 14-bit values in v:
			c =  (v * 0x200040008001ULL & 0x111111111111111ULL) % 0xf;}
		else{if(v<0x1000000){ // option 2, for at most 24-bit values in v:
			c =  ((v & 0xfff) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;
			c += (((v & 0xfff000) >> 12) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;}
		else{ // option 3, for at most 32-bit values in v:
			c =  ((v & 0xfff) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;
			c += (((v & 0xfff000) >> 12) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;
			c += ((v >> 24) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;}}
#elif INTPTR_MAX == INT32_MAX
		for(c=0;v;c++){
			v&=v-1;}
#else
#error Unknown pointer size or missing size macros!
#endif
		return(c);
	}

	int hashpath(uint32_t hashnum,uint32_t hashmax,std::vector<uint32_t>& add)
	{	uint32_t posnum=(hashnum<<1)-bits(hashnum); 
		uint32_t posmax=(hashmax<<1)-bits(hashmax);
		uint32_t diff=1;
		for(;diff<posmax;){
			uint32_t posadd;
			if(hashnum & 1){
				posadd=posnum-diff;
				posnum=posnum+1;}
			else{
				posadd=posnum+diff;
				posnum=posadd+1;}
			hashnum=hashnum>>1;
			diff=(diff<<1)+1;
			if(diff>=posmax){
				if(posadd>=posmax){
					posadd=posmax-1;}
				if(posnum>posmax){
					posnum=posmax;}}
				if(posadd>=posmax){
					}
				else{
					add.push_back(posadd);}}
		return(posnum);
	}

	void addhash(uint8_t* hash,const uint8_t* add) //hash(a,b)==hash(b,a)
	{	SHA256_Init(&sha256);
		for(int i=0;i<SHA256_DIGEST_LENGTH;i++){
			if(hash[i]<add[i]){
				break;}
			if(hash[i]>add[i]){
				SHA256_Update(&sha256,add,SHA256_DIGEST_LENGTH);
				SHA256_Update(&sha256,hash,SHA256_DIGEST_LENGTH);
				SHA256_Final(hash,&sha256);
				return;}}
		SHA256_Update(&sha256,hash,SHA256_DIGEST_LENGTH);
		SHA256_Update(&sha256,add,SHA256_DIGEST_LENGTH);
		SHA256_Final(hash,&sha256);
		return;
	}
	
	void hashpathrun(uint8_t* hash,std::vector<hash_s>& hashes)
	{	if(!hashes.size()){
			return;}
		memcpy(hash,hashes[0].hash,32);
		if(hashes.size()==1){
			return;}
		for(int i=1;i<(int)hashes.size();i++){
			addhash(hash,hashes[i].hash);}
	}

	int update(uint8_t* hash) //returns number of new hashes
	{	int i=1;
		if(!hash_loaded[0]){
			memcpy(hash_[0].hash,hash,SHA256_DIGEST_LENGTH);
			hash_loaded[0]=1;
			return(0);}
		addhash(hash_[0].hash,hash);
		hashes.push_back(hash_[0]);
		hash_loaded[0]=0;
		for(;i<MAXTREE;i++){
			if(!hash_loaded[i]){
				memcpy(hash_[i].hash,hash_[i-1].hash,SHA256_DIGEST_LENGTH);
				hash_loaded[i]=1;
				return(i);}
			addhash(hash_[i].hash,hash_[i-1].hash);
			hashes.push_back(hash_[i]);
			hash_loaded[i]=0;}
		return(i);
	}

	int finish(uint8_t* hash)
	{	int i;
		for(i=0;i<MAXTREE;i++){
			if(hash_loaded[i]){
				memcpy(hash,hash_[i].hash,SHA256_DIGEST_LENGTH);
				break;}}
		if(i==MAXTREE){
			bzero(hash,SHA256_DIGEST_LENGTH);
			return(0);}
		for(i++;i<MAXTREE;i++){
			if(hash_loaded[i]){
				addhash(hash,hash_[i].hash);
				hashes.push_back(*(hash_s*)hash);
				}}
		return(1);
	}

	std::vector<hash_s> hashes;
        hash_s hash_[MAXTREE]; //not private any more, needed to save hashtree
private:
        uint8_t hash_loaded[MAXTREE];
	SHA256_CTX sha256;
};
#endif // HASH_HPP
