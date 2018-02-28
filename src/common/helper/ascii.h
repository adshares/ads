#ifndef ASCII_H
#define ASCII_H

#include <stdio.h>
#include <stdint.h>
#include <sstream>

namespace Helper {

void ed25519_printkey(uint8_t* key,int len);
void ed25519_text2key(uint8_t* key,const char* text,int len);
void ed25519_key2text(char* text,const uint8_t* key,int len);

void ed25519_key2text(std::stringstream& text,const uint8_t* key,int len);
}

#endif // ASCII_H
