#ifndef HLOG_HPP
#define HLOG_HPP

#define HLOG_USO 1
#define HLOG_UOK 2
#define HLOG_USR 3
#define HLOG_BKY 4
#define HLOG_SBS 5
#define HLOG_UBS 6
#define HLOG_BNK 7
#define HLOG_GET 8

#pragma pack(1)
typedef struct {uint8_t type;uint32_t auser;uint32_t buser;uint64_t atxid;uint64_t btxid;} blg_uso_t;
typedef struct {uint8_t type;uint16_t abank;uint32_t auser;uint32_t buser;uint64_t btxid;} blg_uok_t;
typedef struct {uint8_t type;uint32_t auser;uint16_t bbank;uint64_t atxid;} blg_usr_t;
typedef struct {uint8_t type;uint16_t bbank;uint64_t atxid;} blg_bky_t;
typedef struct {uint8_t type;uint16_t bbank;uint32_t status;uint64_t atxid;} blg_sbs_t;
typedef struct {uint8_t type;uint16_t bbank;uint32_t status;uint64_t atxid;} blg_ubs_t;
typedef struct {uint8_t type;uint32_t auser;uint16_t bbank;uint64_t atxid;} blg_bnk_t;
typedef struct {uint8_t type;uint32_t auser;uint16_t bbank;uint32_t buser;uint64_t atxid;} blg_get_t;
#pragma pack()

class hlog
{ 
public:

  hlog(boost::property_tree::ptree& pt,char* filename) : //log,filename
	total(0),
	data(NULL),
	fd(-1)
  { SHA256_Init(&sha256);
    fd=open(filename,O_RDONLY);
    if(fd<0){
      pt.put("error_read","true"); 
      return;}
    uint8_t type;
    boost::property_tree::ptree logs;
    while(read(fd,&type,1)>0){
      boost::property_tree::ptree log;
      switch(type){
        case(HLOG_USO): read_uso(log); break;
        case(HLOG_UOK): read_uok(log); break;
        case(HLOG_USR): read_usr(log); break;
        case(HLOG_BKY): read_bky(log); break;
        case(HLOG_SBS): read_sbs(log); break;
        case(HLOG_UBS): read_ubs(log); break;
        case(HLOG_BNK): read_bnk(log); break;
        case(HLOG_GET): read_get(log); break;
        default: 
          pt.add_child("logs",logs);
          pt.put("error_parse","true"); 
          return;}
      logs.push_back(std::make_pair("",log));}
    char hash[65]; hash[64]='\0';
    hash_t fin;
    finish(fin,total);
    ed25519_key2text(hash,fin,32);
    pt.add_child("logs",logs);
    pt.put("hash",hash);
    return;
  }

  hlog(uint32_t path) :
	total(0),
	data(NULL),
	fd(-1)
  { sprintf(filename,"blk/%03X/%05X/hlog.hlg",path>>20,path&0xFFFFF);
    SHA256_Init(&sha256);
  }

  ~hlog()
  { if(fd>=0){
      close(fd);
      fd=-1;}
    if(data!=NULL){
      free(data);
      data=NULL;}
  }

  void load()
  { fd=open(filename,O_RDONLY);
    if(fd<0){
      data=(char*)malloc(4);
      bzero(data,4);
      return;}
    struct stat sb;
    fstat(fd,&sb);
    total=sb.st_size;
    data=(char*)malloc(4+total);
    read(fd,data+4,total);
    memcpy(data,&total,4);
  }

  void finish(hash_t &hash,int &l)
  { SHA256_Final(hash, &sha256);
    l=total;
  }

  bool save(char* buf,int len)
  { if(fd<0){
      fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
      if(fd<0){
        ELOG("ERROR, failed to open %s\n",filename);
        return(false);}
      DLOG("DEBUG, opened %s\n",filename);}
    SHA256_Update(&sha256,buf,len);
    total+=len;
    return(write(fd,buf,len)==len);
  }

  static char* txid(const uint64_t& ppi)
  { static char text[20];
    ppi_t *p=(ppi_t*)&ppi;
    sprintf(text,"%04X%08X%04X",p->v16[2],p->v32[0],p->v16[3]);
    return(text);
  }

  void read_uso(boost::property_tree::ptree& pt)
  { blg_uso_t log;
    log.type=HLOG_USO;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_USO);
    pt.put("name","create_account_confirmed");
    pt.put("auser",log.auser);
    pt.put("buser",log.buser);
    pt.put("atxid",txid(log.atxid));
    pt.put("btxid",txid(log.btxid));
  }

  bool save_uso(uint32_t auser,uint32_t buser,uint64_t atxid,uint64_t btxid)
  { blg_uso_t log;
    log.type=HLOG_USO;
    log.auser=auser;
    log.buser=buser;
    log.atxid=atxid;
    log.btxid=btxid;
    return(save((char*)&log,sizeof(log)));
  }

  void read_uok(boost::property_tree::ptree& pt)
  { blg_uok_t log;
    log.type=HLOG_UOK;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_UOK);
    pt.put("name","create_account_late");
    pt.put("abank",log.abank);
    pt.put("auser",log.auser);
    pt.put("buser",log.buser);
    pt.put("btxid",txid(log.btxid));
  }

  bool save_uok(uint16_t abank,uint32_t auser,uint32_t buser,uint64_t btxid)
  { blg_uok_t log;
    log.type=HLOG_UOK;
    log.abank=abank;
    log.auser=auser;
    log.buser=buser;
    log.btxid=btxid;
    return(save((char*)&log,sizeof(log)));
  }

  void read_usr(boost::property_tree::ptree& pt)
  { blg_usr_t log;
    log.type=HLOG_USR;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_USR);
    pt.put("name","create_account_failed");
    pt.put("auser",log.auser);
    pt.put("bbank",log.bbank);
    pt.put("atxid",txid(log.atxid));
  }

  bool save_usr(uint32_t auser,uint16_t bbank,uint64_t atxid)
  { blg_usr_t log;
    log.type=HLOG_USR;
    log.auser=auser;
    log.bbank=bbank;
    log.atxid=atxid;
    return(save((char*)&log,sizeof(log)));
  }

  void read_bky(boost::property_tree::ptree& pt)
  { blg_bky_t log;
    log.type=HLOG_BKY;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_BKY);
    pt.put("name","node_unlocked");
    pt.put("bbank",log.bbank);
    pt.put("atxid",txid(log.atxid));
  }

  bool save_bky(uint16_t bbank,uint64_t atxid)
  { blg_bky_t log;
    log.type=HLOG_BKY;
    log.bbank=bbank;
    log.atxid=atxid;
    return(save((char*)&log,sizeof(log)));
  }

  void read_sbs(boost::property_tree::ptree& pt)
  { blg_sbs_t log;
    log.type=HLOG_SBS;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_SBS);
    pt.put("name","set_node_status");
    pt.put("bbank",log.bbank);
    pt.put("status",log.status);
    pt.put("atxid",txid(log.atxid));
  }

  bool save_sbs(uint16_t bbank,uint32_t status,uint64_t atxid)
  { blg_sbs_t log;
    log.type=HLOG_SBS;
    log.bbank=bbank;
    log.status=status;
    log.atxid=atxid;
    return(save((char*)&log,sizeof(log)));
  }

  void read_ubs(boost::property_tree::ptree& pt)
  { blg_ubs_t log;
    log.type=HLOG_UBS;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_UBS);
    pt.put("name","unset_node_status");
    pt.put("bbank",log.bbank);
    pt.put("status",log.status);
    pt.put("atxid",txid(log.atxid));
  }

  bool save_ubs(uint16_t bbank,uint32_t status,uint64_t atxid)
  { blg_ubs_t log;
    log.type=HLOG_UBS;
    log.bbank=bbank;
    log.status=status;
    log.atxid=atxid;
    return(save((char*)&log,sizeof(log)));
  }

  void read_bnk(boost::property_tree::ptree& pt)
  { blg_bnk_t log;
    log.type=HLOG_BNK;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_BNK);
    pt.put("name","create_node");
    pt.put("auser",log.auser);
    pt.put("bbank",log.bbank);
    pt.put("atxid",txid(log.atxid));
  }

  bool save_bnk(uint32_t auser,uint16_t bbank,uint64_t atxid)
  { blg_bnk_t log;
    log.type=HLOG_BNK;
    log.auser=auser;
    log.bbank=bbank;
    log.atxid=atxid;
    return(save((char*)&log,sizeof(log)));
  }

  void read_get(boost::property_tree::ptree& pt)
  { blg_get_t log;
    log.type=HLOG_GET;
    read(fd,(char*)&log+1,sizeof(log)-1);
    SHA256_Update(&sha256,(char*)&log,sizeof(log));
    pt.put("type",HLOG_GET);
    pt.put("name","retrieve_funds");
    pt.put("auser",log.auser);
    pt.put("bbank",log.bbank);
    pt.put("buser",log.buser);
    pt.put("atxid",txid(log.atxid));
  }

  bool save_get(uint32_t auser,uint16_t bbank,uint32_t buser,uint64_t atxid)
  { blg_get_t log;
    log.type=HLOG_BNK;
    log.auser=auser;
    log.bbank=bbank;
    log.buser=buser;
    log.atxid=atxid;
    return(save((char*)&log,sizeof(log)));
  }

  int total;
  char *data;
private:
  int fd;
  char filename[64];
  SHA256_CTX sha256;
};

#endif // HLOG_HPP
