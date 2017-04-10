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

	hashtree(void*) {} // no initialization

	~hashtree() {}

	void addhash(uint8_t* hash,uint8_t* add) //hash(a,b)==hash(b,a)
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
	
	void update(uint8_t* hash)
	{	
//char text[2*SHA256_DIGEST_LENGTH];
//ed25519_key2text(text,hash,SHA256_DIGEST_LENGTH);
//fprintf(stderr,"UPDATE HASH %.*s\n",2*SHA256_DIGEST_LENGTH,text);
		if(!hash_loaded[0]){
//fprintf(stderr,"       STORE as 0\n");
			memcpy(hash_[0],hash,SHA256_DIGEST_LENGTH);
			hash_loaded[0]=1;
			return;}
		addhash(hash_[0],hash);
		hash_loaded[0]=0;
		for(int i=1;i<MAXTREE;i++){
			if(!hash_loaded[i]){
//fprintf(stderr,"       STORE as %d\n",i);
				memcpy(hash_[i],hash_[i-1],SHA256_DIGEST_LENGTH);
				hash_loaded[i]=1;
				return;}
			addhash(hash_[i],hash_[i-1]);
			hash_loaded[i]=0;}
	}

	void finish(uint8_t* hash)
	{	int i;
		for(i=0;i<MAXTREE;i++){
			if(hash_loaded[i]){
//fprintf(stderr,"       START as %d\n",i);
				memcpy(hash,hash_[i],SHA256_DIGEST_LENGTH);
				break;}}
		if(i==MAXTREE){
			bzero(hash,SHA256_DIGEST_LENGTH);
			return;}
		for(i++;i<MAXTREE;i++){
			if(hash_loaded[i]){
//fprintf(stderr,"       ADD      %d\n",i);
				addhash(hash,hash_[i]);}}
	}

private:
        uint8_t hash_loaded[MAXTREE];
        uint8_t hash_[MAXTREE][SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
};
#endif // HASH_HPP
