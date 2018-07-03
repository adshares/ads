#ifndef HLOG_H
#define HLOG_H

#include <boost/property_tree/ptree.hpp>
#include <openssl/sha.h>

#include "ascii.h"
#include "default.hpp"
#include "helper/blockfilereader.h"
#include "helper/filewrapper.h"

#define HLOG_USO 1
#define HLOG_UOK 2
#define HLOG_USR 3
#define HLOG_BKY 4
#define HLOG_SBS 5
#define HLOG_UBS 6
#define HLOG_BNK 7
#define HLOG_GET 8

namespace Helper {

#pragma pack(1)
typedef struct {
    uint8_t type;
    uint32_t auser;
    uint32_t buser;
    uint64_t atxid;
    uint64_t btxid;
} blg_uso_t;
typedef struct {
    uint8_t type;
    uint16_t abank;
    uint32_t auser;
    uint32_t buser;
    uint64_t btxid;
} blg_uok_t;
typedef struct {
    uint8_t type;
    uint32_t auser;
    uint16_t bbank;
    uint64_t atxid;
} blg_usr_t;
typedef struct {
    uint8_t type;
    uint16_t bbank;
    uint64_t atxid;
} blg_bky_t;
typedef struct {
    uint8_t type;
    uint16_t bbank;
    uint32_t status;
    uint64_t atxid;
} blg_sbs_t;
typedef struct {
    uint8_t type;
    uint16_t bbank;
    uint32_t status;
    uint64_t atxid;
} blg_ubs_t;
typedef struct {
    uint8_t type;
    uint32_t auser;
    uint16_t bbank;
    uint64_t atxid;
} blg_bnk_t;
typedef struct {
    uint8_t type;
    uint32_t auser;
    uint16_t bbank;
    uint32_t buser;
    uint64_t atxid;
} blg_get_t;
#pragma pack()

class Hlog {

public:
    Hlog();

    Hlog(char* buffer, uint32_t size);

    Hlog(uint32_t path);

    Hlog(boost::property_tree::ptree& pt, char* filename);

    Hlog(const Hlog &hlog);
    Hlog& operator=(Hlog hlog);

    virtual ~Hlog();

    void printJson(boost::property_tree::ptree& pt);

    void load();

    void finish(hash_t &hash,int &l);

    bool save(char* buf,int len);

    void read_uso(boost::property_tree::ptree& pt, char* buffer);
    void read_uso(boost::property_tree::ptree& pt);

    bool save_uso(uint32_t auser,uint32_t buser,uint64_t atxid,uint64_t btxid);

    void read_uok(boost::property_tree::ptree& pt, char *buffer);
    void read_uok(boost::property_tree::ptree& pt);

    bool save_uok(uint16_t abank,uint32_t auser,uint32_t buser,uint64_t btxid);

    void read_usr(boost::property_tree::ptree& pt, char *buffer);
    void read_usr(boost::property_tree::ptree& pt);

    bool save_usr(uint32_t auser,uint16_t bbank,uint64_t atxid);

    void read_bky(boost::property_tree::ptree& pt, char *buffer);
    void read_bky(boost::property_tree::ptree& pt);

    bool save_bky(uint16_t bbank,uint64_t atxid);

    void read_sbs(boost::property_tree::ptree& pt, char *buffer);
    void read_sbs(boost::property_tree::ptree& pt);

    bool save_sbs(uint16_t bbank,uint32_t status,uint64_t atxid);

    void read_ubs(boost::property_tree::ptree& pt, char *buffer);
    void read_ubs(boost::property_tree::ptree& pt);

    bool save_ubs(uint16_t bbank,uint32_t status,uint64_t atxid);

    void read_bnk(boost::property_tree::ptree& pt, char *buffer);
    void read_bnk(boost::property_tree::ptree& pt);

    bool save_bnk(uint32_t auser,uint16_t bbank,uint64_t atxid);

    void read_get(boost::property_tree::ptree& pt, char *buffer);
    void read_get(boost::property_tree::ptree& pt);

    bool save_get(uint32_t auser,uint16_t bbank,uint32_t buser,uint64_t atxid);

    char* txid(const uint64_t& ppi);

    int total;
    char *data;
  private:
    std::shared_ptr<Helper::BlockFileReader> fd;
    Helper::FileWrapper m_save_file;
    char filename[64];
    char txid_text[20];
    SHA256_CTX sha256;
};

}

#endif // HLOG_H
