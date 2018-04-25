#include "ascii.h"
#include <string.h>
#include <assert.h>
#include <cstdlib>
#include <iomanip>

namespace Helper {

void ed25519_printkey(uint8_t* key,int len) {
    char *text=(char*)malloc(2*len+1);
    text[2*len]='\0';
    ed25519_key2text(text,key,len);
    fprintf(stderr,"KEY:%s\n",text);
    free(text);
}

void ed25519_text2key(uint8_t* key,const char* text,int len) { // len = key length in bytes
    char x[3]="00";
    assert(strlen(text)>=(size_t)2*len);
    //try{
    for( int i=0; i<len; i++) {
        x[0]=text[2*i+0];
        x[1]=text[2*i+1];
        key[i]=strtoul(x,NULL,16);
    }
    //}
    //catch (std::exception& e){
    //	std::cerr << "Text2Key Exception: " << e.what() << "\n";}
}

void ed25519_key2text(char* text,const uint8_t* key,int len) { // len = key length in bytes
    for(int i=0; i<len; i++) {
        char tmp[3];
        snprintf(tmp,3,"%02X",(int)(key[i]));
        memcpy(text+i*2,tmp,2);
    }
}

void ed25519_key2text(std::stringstream& text, const uint8_t* key, int len) {
    text.fill('0');
    for ( int i = 0 ; i < len ; ++i ) {
        text << std::uppercase<< std::setw(2) << std::hex << static_cast<unsigned int>(key[i]);
    }
}

std::string ed25519_key2text(const uint8_t* key, int len) {
    std::stringstream text{};
    text.fill('0');
    for ( int i = 0 ; i < len ; ++i ) {
        text << std::uppercase<< std::setw(2) << std::hex << static_cast<unsigned int>(key[i]);
    }
    return text.str();
}

void text2key(const std::string& key, std::string& text) {
    int len = key.length();
    for(int i=0; i< len; i+=2)
    {
        std::string byte = key.substr(i,2);
        unsigned char chr = (unsigned char) (int)strtol(byte.c_str(), nullptr, 16);
        text.push_back(chr);
    }
}

}

