#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "openssl/sha.h"
#include "ed25519.h"

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <ctype.h>

//apt-get install libmysqlclient-dev
//gcc -fPIC -I /usr/include/mysql/ -shared -o ed25519.so ed25519.c
//mv ed25519.so /usr/lib/mysql/plugin/ed25519.so
/*
DROP FUNCTION ed25519check;
CREATE FUNCTION ed25519check RETURNS INTEGER SONAME "ed25519.so";
SET @text="ala ma kota";
SET @pkey="67D3B5EAF0C0BF6B5A602D359DAECC86A7A74053490EC37AE08E71360587C870";
SET @sign="D84390BC71059145276757A64CA6655EBEEC0537343AF8E1F2E7FBD64F1F6F1339A7FA3D096004677E1EBE4A31BFC51FA79A1B479E0889656B2D174ABC75CB00";
SELECT ed25519check(@text,@pkey,@sign);
*/

void ed25519_readkey(uint8_t* key,char* text,int len)
{ int i;
  char x[3]="00";
  for(i=0;i<len;i++){
    x[0]=text[2*i+0];
    x[1]=text[2*i+1];
    key[i]=strtoul(x,NULL,16);}
}
my_bool ed25519check_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if(args->arg_count!=3 ||
    args->arg_type[0]!=STRING_RESULT ||
    args->arg_type[1]!=STRING_RESULT ||
    args->arg_type[2]!=STRING_RESULT ){
    strcpy(message,"USAGE: ed25519check message_string pubkey_hex_string signature_hex_string");
    return 1;}
  if(args->lengths[1]!=64){
    strcpy(message,"ERROR: bad pubkey_hex_string length, expected 64");
    return 1;}
  if(args->lengths[2]!=128){
    strcpy(message,"ERROR: bad signature_hex_string length, expected 128");
    return 1;}
  return 0;
}
longlong ed25519check(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args,
  char *is_null, char *error __attribute__((unused)))
{ ed25519_public_key pk;
  ed25519_signature  sg;

  ed25519_readkey(pk,args->args[1],32);
  ed25519_readkey(sg,args->args[2],64);
  if(ed25519_sign_open((const unsigned char*)args->args[0],args->lengths[0],pk,sg)==0){
    return(1);}
  return(0);
}
void ed25519check_deinit(UDF_INIT *initid __attribute__((unused)))
{
}


