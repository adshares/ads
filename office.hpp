#ifndef OFFICE_HPP
#define OFFICE_HPP

#include "main.hpp"

/*typedef struct halo_c{
  uint32_t clid;
  uint16_t svid;
  uint16_t len; // 64k max tx len
} halo_t;*/
 
class client;
typedef boost::shared_ptr<client> client_ptr;

class office
{
public:
  office(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint,options& opts,server& srv) :
    io_service_(io_service),
    acceptor_(io_service, endpoint),
    opts_(opts),
    srv_(srv)
  { 
    start_accept();
  }

  ~office()
  { 
  }

  void start_accept(); // main.cpp
  void handle_accept(client_ptr c,const boost::system::error_code& error); // main.cpp
  void leave(client_ptr c); // main.cpp

private:
  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  options& opts_;
  server& srv_;
  std::set<client_ptr> clients_;
  boost::mutex client_;
};

#endif
