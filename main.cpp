//#define _GNU_SOURCE
#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/container/flat_set.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <dirent.h>
#include <fcntl.h>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <set>
#include <stack>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include "default.hpp"
#include "ed25519/ed25519.h"
#include "hash.hpp"
#include "user.hpp"
#include "options.hpp"
#include "message.hpp"
#include "servers.hpp"
#include "candidate.hpp"
#include "server.hpp"
#include "peer.hpp"
#include "office.hpp"
#include "client.hpp"

boost::mutex flog;
FILE* stdlog=NULL;
candidate_ptr nullcnd;
message_ptr nullmsg;

int main(int argc, char* argv[])
{ stdlog=fopen("log.txt","a");
  options opt;
  opt.get(argc,argv);
  try{
    server s(opt);
    office o(opt,s);
    s.ofip=&o;
    s.start();
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
      o.add_msg(txs.data,txs,msid,mpos);}
    ELOG("Shutting down\n");
    o.stop();
    s.stop(); }
  catch (std::exception& e){
    ELOG("Exception: %s\n",e.what());}
  fclose(stdlog);
  return 0;
}

// office <-> client
void office::start_accept()
{ if(!run){ return;}
  client_ptr c(new client(*io_services_[next_io_service_++],*this));
  DLOG("OFFICE online %d\n",next_io_service_);
  if(next_io_service_>=CLIENT_POOL){
    next_io_service_=0;}
  acceptor_.async_accept(c->socket(),boost::bind(&office::handle_accept,this,c,boost::asio::placeholders::error));
}
void office::handle_accept(client_ptr c,const boost::system::error_code& error)
{ //uint32_t now=time(NULL);
  if(!run){ return;}
  DLOG("OFFICE new ticket (total open:%ld)\n",clients_.size());
  if(clients_.size()>=MAXCLIENTS || srv_.do_sync || message.length()>MESSAGE_TOO_LONG){
    ELOG("OFFICE busy, delaying connection\n");}
  while(clients_.size()>=MAXCLIENTS || srv_.do_sync || message.length()>MESSAGE_TOO_LONG){
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));}
  if (!error){
    client_.lock();
    clients_.insert(c);
    client_.unlock();
    try{
      c->start();}
    catch (std::exception& e){
      DLOG("Client exception: %s\n",e.what());
      leave(c);}}
  start_accept(); //FIXME, change this to a non blocking office
}
void office::leave(client_ptr c)
{ client_.lock();
  clients_.erase(c);
  client_.unlock();
}

// server <-> office
void server::ofip_update_block(uint32_t period_start,uint32_t now,message_map& commit_msgs,uint32_t newdiv)
{	ofip->update_block(period_start,now,commit_msgs,newdiv);
}
void server::ofip_process_log(uint32_t now)
{	ofip->process_log(now);
}
void server::ofip_init(uint32_t myusers)
{	ofip->init(myusers);
}
void server::ofip_start()
{	ofip->start();
}
bool server::ofip_get_msg(uint32_t msid,std::string& line)
{	return(ofip->get_msg(msid,line));
}
void server::ofip_del_msg(uint32_t msid)
{	ofip->del_msg(msid);
}
void server::ofip_gup_push(gup_t& g)
{	ofip->gup.push(g);
}
void server::ofip_add_remote_deposit(uint32_t user,int64_t weight)
{	ofip->add_remote_deposit(user,weight);
}
void server::ofip_add_remote_user(uint16_t abank,uint32_t auser,uint8_t* pkey)
{	ofip->add_remote_user(abank,auser,pkey);
}
void server::ofip_delete_user(uint32_t auser)
{	ofip->delete_user(auser);
}
void server::ofip_change_pkey(uint8_t* pkey)
{	memcpy(ofip->pkey,pkey,32);
}

// server <-> peer
//void server::join(peer_ptr p)
//{	peer_.lock();
//	peers_.insert(p);
//	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::print, _1));
//	peer_.unlock();
//}
//void server::leave(peer_ptr p) // not used !
//{	assert(0);
//	peer_.lock();
//	if(peers_.find(p)!=peers_.end()){
//		missing_sent_remove(p->svid);
//		peers_.erase(p);}
//	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::print, _1));
//	peer_.unlock();
//	p->stop();
//}
void server::peer_clean()
{	peer_.lock();
	for(auto pj=peers_.begin();pj!=peers_.end();){
                auto pi=pj++;
		if((*pi)->killme){
			DLOG("KILL PEER %04X\n",(*pi)->svid);
			missing_sent_remove((*pi)->svid);
			//DLOG("DISCONNECT PEER %04X stop\n",(*pi)->svid);
			(*pi)->stop();
			//DLOG("DISCONNECT PEER %04X erase\n",(*pi)->svid);
			peers_.erase(*pi);
			//DLOG("DISCONNECT PEER %04X end\n",(*pi)->svid);
			continue;}}
	peer_.unlock();
}
void server::disconnect(uint16_t svid)
{	peer_.lock();
	for(auto pj=peers_.begin();pj!=peers_.end();){
                auto pi=pj++;
		if((*pi)->svid==svid){
			DLOG("DISCONNECT PEER %04X\n",(*pi)->svid);
			(*pi)->killme=true;
			continue;}}
	peer_.unlock();
}
const char* server::peers_list()
{	static std::string list;
	list="";
 	peer_.lock();
	if(!peers_.size()){
 		peer_.unlock();
		return("");}
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		list+=",";
		list+=std::to_string((*pi)->svid);
		if((*pi)->killme){
			list+="*";}}
 	peer_.unlock();
	return(list.c_str()+1);
}
void server::connected(std::vector<uint16_t>& list)
{	peer_.lock();
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid){
			list.push_back((*pi)->svid);}}
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
			peer_.unlock();
			return 1;}}
	peer_.unlock();
	return 0;
}
void server::get_more_headers(uint32_t now) //use random order
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
		DLOG("REQUEST more headers from peer %04X\n",(*pi)->svid);
		(*pi)->request_next_headers(now);}
}
void server::fillknown(message_ptr msg) //use random order
{	static uint32_t r=0;
 	std::vector<uint16_t> v;
	peer_.lock();
	v.reserve(peers_.size());
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid){
			v.push_back((*pi)->svid);}}
	uint32_t n=v.size();
	for(uint32_t i=0;i<n;i++){
		msg->know_insert(v[(r+i)%n]);} //insertion order important
	peer_.unlock();
	r++;
}
int server::deliver(message_ptr msg,uint16_t svid)
{	peer_.lock();
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid==svid){
			(*pi)->deliver(msg);
			peer_.unlock();
			return(1);}}
	peer_.unlock();
	msg->sent_erase(svid);
	return(0);
}
void server::deliver(message_ptr msg)
{	peer_.lock();
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::deliver, _1, msg));
	peer_.unlock();
}
int server::update(message_ptr msg,uint16_t svid)
{	peer_.lock();
	for(auto pi=peers_.begin();pi!=peers_.end();pi++){
		if((*pi)->svid==svid){
			(*pi)->update(msg);
			peer_.unlock();
			return(1);}}
	peer_.unlock();
	msg->sent_erase(svid);
	return(0);
}
void server::update(message_ptr msg)
{	peer_.lock();
	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::update, _1, msg));
	peer_.unlock();
}
//void server::svid_msid_rollback(message_ptr msg)
//{	assert(0); //do not use this, this causes sync problems if peers is in different block
//	peer_.lock();
//	std::for_each(peers_.begin(),peers_.end(),boost::bind(&peer::svid_msid_rollback, _1, msg));
//	peer_.unlock();
//}
void server::start_accept()
{	peer_ptr new_peer(new peer(*this,true,srvs_,opts_));
	//new_peer->iostart();
	acceptor_.async_accept(new_peer->socket(),boost::bind(&server::peer_accept,this,new_peer,boost::asio::placeholders::error));
}
void server::peer_accept(peer_ptr new_peer,const boost::system::error_code& error)
{	if (!error){
		peer_.lock();
		peers_.insert(new_peer);
		peer_.unlock();
		new_peer->accept();}
	else{
		new_peer->stop();}
		//peer_.lock();
		//peers_.erase(new_peer);
		//peer_.unlock();
	start_accept();
}
void server::connect(std::string peer_address)
{	try{
		DLOG("TRY connecting to address %s\n",peer_address.c_str());
		const char* port=SERVER_PORT;
		char* p=strchr((char*)peer_address.c_str(),':');
		if(p!=NULL){
			peer_address[p-peer_address.c_str()]='\0';
			port=p+1;}
		boost::asio::ip::tcp::resolver resolver(io_service_);
		boost::asio::ip::tcp::resolver::query query(peer_address.c_str(),port);
		boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
		peer_ptr new_peer(new peer(*this,false,srvs_,opts_));
		peer_.lock();
		peers_.insert(new_peer);
		peer_.unlock();
                //new_peer->iostart();
		boost::asio::async_connect(new_peer->socket(),iterator,
			boost::bind(&peer::connect,new_peer,boost::asio::placeholders::error));}
	catch (std::exception& e){
		DLOG("Connection: %s\n",e.what());}
}
void server::connect(uint16_t svid)
{	try{
		DLOG("TRY connecting to peer %04X\n",svid);
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
		peer_.lock();
		peers_.insert(new_peer);
		peer_.unlock();
                //new_peer->iostart();
		boost::asio::async_connect(new_peer->socket(),iterator,
			boost::bind(&peer::connect,new_peer,boost::asio::placeholders::error));}
	catch (std::exception& e){
		DLOG("Connection: %s\n",e.what());}
}
