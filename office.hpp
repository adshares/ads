#ifndef OFFICE_HPP
#define OFFICE_HPP

#include "main.hpp"

class client;
typedef boost::shared_ptr<client> client_ptr;

//before optimising remember that the required througput here is much lower than the throughput of the server
//consider running the processing in a single thread

class office
{
public:
  office(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint,options& opts,server& srv) :
    io_service_(io_service),
    acceptor_(io_service, endpoint),
    opts_(opts),
    srv_(srv),
    fd(0),
    message_sent(0)
  { svid=opts_.svid,
    users=srv_.last_srvs_.nodes[svid].users;
    deposit.resize(users), // a buffer to enable easy resizing of the account vector
    std::cerr<<"OFFICE ("<<svid<<") open\n";
    // prapare local register [this could be in RAM later or be on a RAM disk]
    mkdir("ofi",0755);
    char filename[64];
    sprintf(filename,"ofi/%04X.dat",svid);
    fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0644);
    if(!fd){
      std::cerr<<"ERROR, failed to open office register\n";}
    msid=srv_.msid_;

    run=true;
    start_accept();
    clock_thread = new boost::thread(boost::bind(&office::clock, this));
  }

  ~office()
  { if(fd){
      close(fd);}
  }

  void stop() // send and empty txs queue
  { run=false;
    clock_thread->interrupt();
    clock_thread->join();
    //submit message
  }

  void clock()
  { while(run){
      uint32_t now=time(NULL);
      //TODO, clear hanging clients
      boost::this_thread::sleep(boost::posix_time::seconds(2));
      if(message.empty()){
        srv_.break_silence(now,message);
        continue;}
      if(message.length()<MESSAGE_LEN_OK && message_sent+MESSAGE_WAIT>now){
	std::cerr<<"WARNING, waiting for more messages\n";
        continue;}
      if(!srv_.accept_message(msid)){
	std::cerr<<"WARNING, server not ready for a new message ("<<msid<<")\n";
        continue;}
      message_.lock();
      std::cerr<<"SENDING new message (old msid:"<<msid<<")\n";
      msid=srv_.write_message(message);
      message.clear();
      message_.unlock();
      message_sent=now;}
  }

  bool check_account(uint16_t cbank,uint32_t cuser)
  { if(cbank==svid && cuser<users){
      return(true);}
    if(srv_.last_srvs_.nodes.size()<=cbank){
      return(false);}
    if(srv_.last_srvs_.nodes[cbank].users<=cuser){
      return(false);}
    return(true);
  }

  void get_user(user_t& u,uint16_t cbank,uint32_t cuser)
  { assert(cbank==svid);
    assert(cuser<users);
    u.id=0;
    file_.lock();
    lseek(fd,cuser*sizeof(user_t),SEEK_SET);
    read(fd,&u,sizeof(user_t));
    u.weight+=deposit[cuser];
    if(u.id){
      file_.unlock();
      return;}
    srv_.last_srvs_.get_user(u,cbank,cuser); //watch out for deadlocks
    u.weight+=deposit[cuser];
    deposit[cuser]=0;
    assert(u.weight>=0);
    lseek(fd,cuser*sizeof(user_t),SEEK_SET);
    write(fd,&u,sizeof(user_t));
    file_.unlock();
  }

  void set_user(user_t& u,uint16_t cbank,uint32_t cuser, int64_t deduct)
  { assert(cbank==svid);
    assert(cuser<users);
    file_.lock();
    u.weight+=deposit[cuser]-deduct;
    deposit[cuser]=0;
    assert(u.weight>=0);
    lseek(fd,cuser*sizeof(user_t),SEEK_SET);
    write(fd,&u,sizeof(user_t));
    file_.unlock();
  }

  void add_deposit(uint32_t cuser, int64_t cadd)
  { file_.lock();
    deposit[cuser]+=cadd;
    file_.unlock();
  }

  void add_msg(uint8_t* msg,int len, int64_t fee)
  { if(!run){ return;}
    message_.lock();
    message.append((char*)msg,len);
    message_.unlock();
    mfee+=BANK_PROFIT(fee); //do we need this?
  }

  void lock_user(uint32_t cuser) // not used
  { users_[cuser & 0xff].lock();
  }

  void unlock_user(uint32_t cuser) // not used
  { users_[cuser & 0xff].unlock();
  }

  void start_accept(); // main.cpp
  void handle_accept(client_ptr c,const boost::system::error_code& error); // main.cpp, currently blocking :-(
  //void leave(client_ptr c); // main.cpp

  bool run;
  uint16_t svid;
  uint32_t users; //number of users of the bank
  std::vector<int64_t> deposit; //resizing will require a stop of processing
  boost::mutex users_[0x10]; //used ???
  std::string message;
  boost::mutex message_;
private:
  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  options& opts_;
  server& srv_;
  std::set<client_ptr> clients_;
  boost::mutex client_;
  int fd; // user file descriptor
  boost::mutex file_;
  uint32_t msid;
   int64_t mfee; // let's see if we need this
  uint32_t message_sent;
  boost::thread* clock_thread;
};

#endif
