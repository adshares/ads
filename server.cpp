#include "main.hpp"

void server::join(peer_ptr p)
{	peer_.lock();
	peers_.insert(p);
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::print, _1));
	peer_.unlock();
}
void server::leave(peer_ptr p)
{	peer_.lock();
	if(peers_.find(p)!=peers_.end()){
		peers_.erase(p);}
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::print, _1));
	peer_.unlock();
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
void server::deliver(message_ptr msg,uint16_t svid)
{	peer_.lock();
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid==svid){
			(*pi)->deliver(msg);
			break;}}
	peer_.unlock();
}
void server::fillknown(message_ptr msg) //use random order
{	static uint32_t r=0;
 	std::vector<uint16_t> v;
	peer_.lock();
	v.reserve(peers_.size());
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		v.pushback((*pi)->svid);}
	uint32_t n=v.size();
	for(int i=0;i<n;i++){
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
{	peer_ptr new_peer(new peer(io_service_,*this,true,srvs_,opts_));
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
		peer_ptr new_peer(new peer(io_service_,*this,false,srvs_,opts_));
		boost::asio::async_connect(new_peer->socket(),iterator,boost::bind(&peer::start,new_peer));}
	catch (std::exception& e){
		std::cerr << "Connection: " << e.what() << "\n";}
}
