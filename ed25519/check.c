#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "openssl/sha.h"
#include "ed25519.h"

void readkey(uint8_t* key,char* text,int len)
{	int i;
	char x[3]="00";
	assert(strlen(text)==2*len);
	for(i=0;i<len;i++){
		x[0]=text[2*i+0];
		x[1]=text[2*i+1];
		key[i]=strtoul(x,NULL,16);}
}

void printkey(FILE* fp,const uint8_t* key,int len)
{	int i;
	for(i=0;i<len;i++){
		fprintf(fp,"%02X",key[i]);}
	fprintf(fp,"\n");
}

int main(int argc, char** argv)
{	ed25519_public_key pk;
	ed25519_signature  sg;

	assert(sizeof(pk)==SHA256_DIGEST_LENGTH);
	if(argc<=3){
		fprintf(stderr,"USAGE: %s sig pk message\n",argv[0]);
		return(-1);}
	readkey(sg,argv[1],64);
	fprintf(stdout,"SG: ");
	printkey(stdout,sg,64);
	readkey(pk,argv[2],32);
	fprintf(stdout,"PK: ");
	printkey(stdout,pk,32);
	if(ed25519_sign_open((const unsigned char*)argv[3],strlen(argv[3]),pk,sg)==0){
		fprintf(stdout,"OK!\n");}
	else{
		fprintf(stdout,"WRONG!\n");}
	return(0);
}

