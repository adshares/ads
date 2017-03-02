#ifndef HASH_HPP
#define HASH_HPP

#define MAXTREE 32

class hashtree
{
public:
	hashtree()
	{	bzero(hash_loaded,MAXTREE*sizeof(uint8_t));
		bzero(hash_,MAXTREE*SHA256_DIGEST_LENGTH*sizeof(uint8_t));
	}

	~hashtree() {}

	void addhash(uint8_t* hash,uint8_t* add) //hash(a,b)==hash(b,a)
	{	SHA256_Init(&sha256);
		for(i=0;i<SHA256_DIGEST_LENGTH;i++){
			if(hash[i]<add[i]){
				break;}
			if(hash[i]>add[i]){
				SHA256_Update(&sha256,add,SHA256_DIGEST_LENGTH);
				SHA256_Update(&sha256,hash,SHA256_DIGEST_LENGTH);
				SHA256_Final(hash,&sha256);}}
		SHA256_Update(&sha256,hash,SHA256_DIGEST_LENGTH);
		SHA256_Update(&sha256,add,SHA256_DIGEST_LENGTH);
		SHA256_Final(hash,&sha256);
		return;
	}
	
	void update(uint8_t* hash)
	{	if(!hash_loaded[0]){
			memcpy(hash_[0],hash,SHA256_DIGEST_LENGTH);
			hash_loaded[0]=1;
			return;}
		addhash(hash_[0],hash);
		hash_loaded[0]=0;
		for(i=1;i<MAXTREE;i++){
			if(!hash_loaded[i]){
				memcpy(hash_[i],hash_[i-1],SHA256_DIGEST_LENGTH);
				hash_loaded[i]=1;
				return;}
			addhash(hash_[i],hash_[i-1]);
			hash_loaded[i]=0;}
	}

	void finish(uint8_t* hash)
	{	for(i=0;i<MAXTREE;i++){
			if(hash_loaded[i]){
				memcpy(hash,hash_[i],SHA256_DIGEST_LENGTH);
				break;}}
		for(;i<MAXTREE;i++){
			if(hash_loaded[i]){
				addhash(hash,hash_[i]);}}
	}

private:
	int i;
        uint8_t hash_loaded[MAXTREE];
        uint8_t hash_[MAXTREE][SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
};
#endif // HASH_HPP
