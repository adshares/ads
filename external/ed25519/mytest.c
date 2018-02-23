#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/time.h>
#include "openssl/sha.h"
#include "ed25519.h"

#define SIZE (48*1024) //60*1024 is too large :-(
#define NUM (1)

int main(int argc, char** argv)
{	ed25519_public_key pk[SIZE];
 	ed25519_secret_key sk[SIZE];
	ed25519_signature  sg[SIZE];
 	const unsigned char* pkp[SIZE];
	const unsigned char* sgp[SIZE];
	int valid[SIZE];
	size_t mlen[SIZE];
        SHA256_CTX sha256;
        struct timeval tv;
	uint64_t ot,nt;
	int n,i;
	double sp;

	for(i=0;i<SIZE;i++){
		bzero(pk[i],sizeof(ed25519_public_key));
		bzero(sk[i],sizeof(ed25519_secret_key));
		bzero(sg[i],sizeof(ed25519_signature));
		pkp[i]=(const unsigned char*)&pk[i];
		sgp[i]=(const unsigned char*)&sg[i];}

	fprintf(stdout,"START\n");
	gettimeofday(&tv,(struct timezone*)0);
	ot=1000000*tv.tv_sec+tv.tv_usec;
	for(n=0;n<NUM;n++){
		for(i=0;i<SIZE;i++){
			SHA256_Init(&sha256);
			if(!i){
				SHA256_Update(&sha256,"start",5);
				SHA256_Update(&sha256,"start",5);}
			else{
				SHA256_Update(&sha256,sk[i-1],32);
				SHA256_Update(&sha256,sk[i-1],32);}
		        SHA256_Final(sk[i],&sha256);}}
	gettimeofday(&tv,(struct timezone*)0);
	nt=1000000*tv.tv_sec+tv.tv_usec;
	sp=(double)(NUM*SIZE)*1000000.0/(double)(nt-ot);
	fprintf(stdout,"HASH: %.3f/s [%.3fMiB/s]\n",sp,sp*64/1024/1024);

	ot=nt;
	for(n=0;n<NUM;n++){
		for(i=0;i<SIZE;i++){
			ed25519_publickey(sk[i],pk[i]);}}
	gettimeofday(&tv,(struct timezone*)0);
	nt=1000000*tv.tv_sec+tv.tv_usec;
	sp=(double)(NUM*SIZE)*1000000.0/(double)(nt-ot);
	fprintf(stdout,"PKEY: %.3f/s\n",sp);

	ot=nt;
	for(n=0;n<NUM;n++){
		for(i=0;i<SIZE;i++){
			mlen[i]=32;
			ed25519_sign(pk[i],32,sk[i],pk[i],sg[i]);}}
	gettimeofday(&tv,(struct timezone*)0);
	nt=1000000*tv.tv_sec+tv.tv_usec;
	sp=(double)(NUM*SIZE)*1000000.0/(double)(nt-ot);
	fprintf(stdout,"SIGN: %.3f/s [%.3fMiB/s]\n",sp,sp*64/1024/1024);

	ot=nt;
	for(n=0;n<NUM;n++){
		for(i=0;i<SIZE;i++){
			if(ed25519_sign_open(pk[i],32,pk[i],sg[i])){
				fprintf(stderr,"ERROR\n");
				return(-1);}}}
	gettimeofday(&tv,(struct timezone*)0);
	nt=1000000*tv.tv_sec+tv.tv_usec;
	sp=(double)(NUM*SIZE)*1000000.0/(double)(nt-ot);
	fprintf(stdout,"OPEN: %.3f/s [%.3fMiB/s]\n",sp,sp*64/1024/1024);

	ot=nt;
	ed25519_sign_open_batch(pkp,mlen,pkp,sgp,SIZE,valid);
	gettimeofday(&tv,(struct timezone*)0);
	nt=1000000*tv.tv_sec+tv.tv_usec;
	sp=(double)(NUM*SIZE)*1000000.0/(double)(nt-ot);
	fprintf(stdout,"BATC: %.3f/s [%.3fMiB/s]\n",sp,sp*64/1024/1024);
	return(0);
}

