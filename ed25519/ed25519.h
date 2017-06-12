#ifndef ED25519_H
#define ED25519_H

#include <stdlib.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned char ed25519_signature[64];
typedef unsigned char ed25519_public_key[32];
typedef unsigned char ed25519_secret_key[32];

typedef unsigned char curved25519_key[32];

void ed25519_publickey(const ed25519_secret_key sk, ed25519_public_key pk);
int ed25519_sign_open(const unsigned char *m, size_t mlen, const ed25519_public_key pk, const ed25519_signature RS);
void ed25519_sign(const unsigned char *m, size_t mlen, const ed25519_secret_key sk, const ed25519_public_key pk, ed25519_signature RS);

int ed25519_sign_open_batch(const unsigned char **m, size_t *mlen, const unsigned char **pk, const unsigned char **RS, size_t num, int *valid);

void ed25519_randombytes_unsafe(void *out, size_t count);

void curved25519_scalarmult_basepoint(curved25519_key pk, const curved25519_key e);

//LESZEK
int ed25519_sign_open2(const unsigned char *m, size_t mlen,const unsigned char *m2, size_t mlen2, const ed25519_public_key pk, const ed25519_signature RS);
void ed25519_sign2(const unsigned char *m, size_t mlen, const unsigned char *m2, size_t mlen2, const ed25519_secret_key sk, const ed25519_public_key pk, ed25519_signature RS);
int ed25519_sign_open_batch2(const unsigned char **m, size_t *mlen, const unsigned char **m2, size_t *mlen2, const unsigned char **pk, const unsigned char **RS, size_t num, int *valid);
void ed25519_printkey(uint8_t* key,int len);
void ed25519_text2key(uint8_t* key,const char* text,int len);
void ed25519_key2text(char* text,const uint8_t* key,int len);

#if defined(__cplusplus)
}
/*//LESZEK
typedef unsigned char hash_t[32]; // consider reducing this to uint64_t[2]
typedef struct {hash_t hash;} hash_s;
typedef struct hash_cmp {
  bool operator()(const hash_s& i,const hash_s& j) const {int k=memcmp(i.hash,j.hash,sizeof(hash_t)); return(k<0);}
} hash_cmp_t;*/
#endif

#endif // ED25519_H
