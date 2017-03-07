#include "main.hpp"

int main(int argc, char* argv[])
{
  options opt;
  opt.get(argc,argv);
  try{
    server s(opt);
    office o(opt,s);
    s.ofip=&o;
    std::string line;
    while (std::getline(std::cin,line)){
      if(line[0]=='.' && line[1]=='\0'){
        break;}
      //FIXME, send a broadcast
      uint32_t now=time(NULL);
      usertxs txs(TXSTYPE_CON,0,0,now);
      //o.message.append((char*)txs.data,txs.size);
      uint32_t msid;
      uint32_t mpos;
      o.add_msg(txs.data,txs.size,0,msid,mpos);}
    std::cerr << "Shutting down\n";
    o.stop();
    s.stop(); }
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";}
  return 0;
}

// office <-> client
void office::start_accept()
{ if(!run){ return;}
  client_ptr c(new client(*io_services_[next_io_service_++],*this));
  std::cerr<<"OFFICE online ["<<next_io_service_<<"]\n";
  if(next_io_service_>=CLIENT_POOL){
    next_io_service_=0;}
  //while(clients_.size()>=MAXCLIENTS || srv_.do_sync){
  //  //crerate client timeout inside the client
  //  std::cerr<<"OFFICE offline\n";
  //  boost::this_thread::sleep(boost::posix_time::seconds(1));}
  acceptor_.async_accept(c->socket(),boost::bind(&office::handle_accept,this,c,boost::asio::placeholders::error));
}
void office::handle_accept(client_ptr c,const boost::system::error_code& error)
{ //uint32_t now=time(NULL);
  if(!run){ return;}
  std::cerr<<"OFFICE new ticket (total open:"<<clients_.size()<<")\n";
  if(clients_.size()>=MAXCLIENTS || srv_.do_sync || message.length()>MESSAGE_TOO_LONG){
    std::cerr<<"OFFICE busy, dropping connection\n";}
  else if (!error){
    client_.lock();
    clients_.insert(c);
    client_.unlock();
    try{
      c->start();}
    catch (std::exception& e){
      std::cerr << "Client exception: " << e.what() << "\n";
      leave(c);}}
  start_accept(); //FIXME, change this to a non blocking office
  //std::cerr<<"OFFICE rest\n";
  //boost::this_thread::sleep(boost::posix_time::seconds(1));//do this to test if function is blocking
  //std::cerr<<"OFFICE done\n";
  //start_accept(); //FIXME, change this to a non blocking office
}
void office::leave(client_ptr c)
{ client_.lock();
  clients_.erase(c);
  client_.unlock();
}

// server <-> office
void server::ofip_update_block(uint32_t period_start,uint32_t now,message_queue& commit_msgs,uint32_t newdiv)
{	ofip->update_block(period_start,now,commit_msgs,newdiv);
}
void server::ofip_start(uint32_t myusers)
{	ofip->start(myusers);
}
void server::ofip_gup_push(gup_t& g)
{	ofip->gup.push(g);
}
void server::ofip_add_remote_deposit(uint32_t user,int64_t weight)
{	ofip->add_remote_deposit(user,weight);
}

// server <-> peer
void server::join(peer_ptr p)
{	peer_.lock();
	peers_.insert(p);
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::print, _1));
	peer_.unlock();
}
void server::leave(peer_ptr p)
{	peer_.lock();
	if(peers_.find(p)!=peers_.end()){
		missing_sent_remove(p->svid);
		p->stop();
		peers_.erase(p);}
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::print, _1));
	peer_.unlock();
}
void server::disconnect(uint16_t svid)
{	peer_.lock();
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid==svid){
			missing_sent_remove(svid);
			(*pi)->stop();
			peers_.erase(*pi);
			break;}}
	peer_.unlock();
}
bool server::connected(uint16_t svid)
{	peer_.lock();
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid==svid){
			peer_.unlock();
			return(true);}}
	peer_.unlock();
	return(false);
}
int server::duplicate(peer_ptr p)
{	peer_.lock();
	for(const peer_ptr r : peers_){
		if(r!=p && r->svid==p->svid){
			//peers_.erase(p);
			peer_.unlock();
			return 1;}}
	peer_.unlock();
	return 0;
}
int server::deliver(message_ptr msg,uint16_t svid)
{	peer_.lock();
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid==svid){
			msg->sent.insert(svid);
			(*pi)->deliver(msg);
			peer_.unlock();
			return(1);}}
	peer_.unlock();
	msg->sent.erase(svid);
	return(0);
}
void server::get_more_headers(uint32_t now,uint8_t* nowhash) //use random order
{	peer_.lock();
	auto pi=peers_.begin();
	if(!peers_.size()){
		peer_.unlock();
		return;}
	if(peers_.size()>1){
		int64_t num=((uint64_t)random())%peers_.size();
		advance(pi,num);}
	peer_.unlock();
	if(!(*pi)->do_sync){
		fprintf(stderr,"REQUEST more headers from peer %04X\n",(*pi)->svid);
		(*pi)->handle_next_headers(now,nowhash);}
}
void server::fillknown(message_ptr msg) //use random order
{	static uint32_t r=0;
 	std::vector<uint16_t> v;
	peer_.lock();
	v.reserve(peers_.size());
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		v.push_back((*pi)->svid);}
	uint32_t n=v.size();
	for(uint32_t i=0;i<n;i++){
		msg->know_insert(v[(r+i)%n]);} //insertion order important
	peer_.unlock();
	r++;
}
void server::deliver(message_ptr msg)
{	peer_.lock();
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::deliver, _1, msg));
	peer_.unlock();
}
void server::update(message_ptr msg)
{	peer_.lock();
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::update, _1, msg));
	peer_.unlock();
}
void server::svid_msid_rollback(message_ptr msg)
{	peer_.lock();
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::svid_msid_rollback, _1, msg));
	peer_.unlock();
}
void server::start_accept()
{	peer_ptr new_peer(new peer(*this,true,srvs_,opts_));
 	//peer_ptr new_peer(new peer(io_service_,*this,true,srvs_,opts_));
        //http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/example/http/server2/io_service_pool.cpp
	acceptor_.async_accept(new_peer->socket(),boost::bind(&server::handle_accept,this,new_peer,boost::asio::placeholders::error));
}
void server::handle_accept(peer_ptr new_peer,const boost::system::error_code& error)
{	if (!error){
		new_peer->start();}
	start_accept();
}
void server::connect(std::string peer_address)
{	try{
		const char* port=SERVER_PORT;
		char* p=strchr((char*)peer_address.c_str(),':');
		if(p!=NULL){
			peer_address[p-peer_address.c_str()]='\0';
			port=p+1;}
		boost::asio::ip::tcp::resolver resolver(io_service_);
		boost::asio::ip::tcp::resolver::query query(peer_address.c_str(),port);
		boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
		peer_ptr new_peer(new peer(*this,false,srvs_,opts_));
		boost::asio::async_connect(new_peer->socket(),iterator,boost::bind(&peer::start,new_peer));}
	catch (std::exception& e){
		std::cerr << "Connection: " << e.what() << "\n";}
}
void server::connect(uint16_t svid)
{	try{
		//char ipv4t[32];
		//sprintf(ipv4t,"%s",inet_ntoa(srvs_.nodes[svid].ipv4));
		struct in_addr addr;
		addr.s_addr=srvs_.nodes[svid].ipv4;
		char portt[32];
		sprintf(portt,"%u",srvs_.nodes[svid].port);
		boost::asio::ip::tcp::resolver resolver(io_service_);
		boost::asio::ip::tcp::resolver::query query(inet_ntoa(addr),portt);
		boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
		peer_ptr new_peer(new peer(*this,false,srvs_,opts_));
		boost::asio::async_connect(new_peer->socket(),iterator,boost::bind(&peer::start,new_peer));}
	catch (std::exception& e){
		std::cerr << "Connection: " << e.what() << "\n";}
}
