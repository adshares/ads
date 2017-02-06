#ifndef PEER_HPP
#define PEER_HPP

class peer : public boost::enable_shared_from_this<peer>
{
public:
  peer(boost::asio::io_service& io_service,server& srv,bool in,servers& srvs,options& opts) :
      svid(0),
      socket_(io_service),
      server_(srv),
      incoming_(in),
      srvs_(srvs),
      opts_(opts),
      addr(""),
      port(0),
      do_sync(1),
      files_out(0),
      files_in(0),
      bytes_out(0),
      bytes_in(0),
      BLOCK_MODE_SERVER(0),
      BLOCK_MODE_PEER(0)
  {  read_msg_ = boost::make_shared<message>();
  }

  ~peer()
  { if(port){
      std::cerr << "Client left   " << addr << ":" << port << "\n";} //LESZEK
  }

  boost::asio::ip::tcp::socket& socket()
  { return socket_;
  }

  void print()
  { if(port){
      std::cerr << "Client on     " << addr << ":" << port << "\n";} //LESZEK
    else{
      std::cerr << "Client available\n";}
  }

  void start()
  {
//FIXME, we should run our own io_service !!! ... need to check later if this is the case
    addr = socket_.remote_endpoint().address().to_string();
    //ipv4 = socket_.remote_endpoint().address().to_v4().to_ulong();
    port = socket_.remote_endpoint().port();
    server_.join(shared_from_this());
    if(incoming_){
      std::cerr << "Client joined " << socket_.remote_endpoint().address().to_string() << ":" << socket_.remote_endpoint().port() << "\n";}
    else{
      std::cerr << "Joining server " << socket_.remote_endpoint().address().to_string() << ":" << socket_.remote_endpoint().port() << "\n";
      //message_ptr msg=server_.write_handshake(ipv4,port,0,0);
      // send naive handshake message
      message_ptr msg=server_.write_handshake(0,sync_hs); // sets sync_hs
      msg->print("; start");
      send_sync(msg);}
    // read authentication data, maybe start with (synchronous?) authenticate()
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
  }

  void update(message_ptr msg)
  { if(do_sync){
      return;}
    assert(msg->len>4+64);
    if(msg->peer == svid){
      msg->print("; NO ECHO");
      return;}
    msg->mtx_.lock();
    if(msg->know.find(svid) != msg->know.end()){
      msg->print("; NO UPDATE");
      msg->mtx_.unlock();
      return;}
    msg->know.insert(svid);
    msg->mtx_.unlock();
    message_ptr put_msg(new message()); // gone ??? !!!
    switch(msg->hashtype()){
      case MSGTYPE_TXS:
        put_msg->data[0]=MSGTYPE_PUT;
        put_msg->data[1]=msg->hashval(svid); //msg->data[4+(svid%64)]; // convert to peer-specific hash
        memcpy(put_msg->data+2,&msg->msid,4);
        memcpy(put_msg->data+6,&msg->svid,2);
        break;
      case MSGTYPE_CND:
        put_msg->data[0]=MSGTYPE_CNP;
        put_msg->data[1]=msg->hashval(svid); //msg->data[4+(svid%64)]; // convert to peer-specific hash
        memcpy(put_msg->data+2,&msg->msid,4);
        memcpy(put_msg->data+6,&msg->svid,2);
        break;
      case MSGTYPE_BLK:
        put_msg->data[0]=MSGTYPE_BLP;
        put_msg->data[1]=msg->hashval(svid); //msg->data[4+(svid%64)]; // convert to peer-specific hash
        memcpy(put_msg->data+2,&msg->msid,4);
        memcpy(put_msg->data+6,&msg->svid,2);
        break;
      case MSGTYPE_DBL:
        put_msg->data[0]=MSGTYPE_DBP;
        put_msg->data[1]=0;
        memcpy(put_msg->data+2,&msg->msid,4);
        memcpy(put_msg->data+6,&msg->svid,2);
        break;
      default:
        std::cerr << "FATAL ERROR: bad message type\n";
        exit(-1);}
    mtx_.lock();
      put_msg->svid=msg->svid;
      put_msg->msid=msg->msid;
      put_msg->hash.num=put_msg->dohash(put_msg->data);
      fprintf(stderr,"HASH update:%016lX [%016lX] (%04X) %d:%d\n",put_msg->hash.num,*((uint64_t*)put_msg->data),svid,msg->svid,msg->msid); // could be bad allignment
    if(BLOCK_MODE_SERVER){
      wait_msgs_.push_back(put_msg);
      mtx_.unlock();
      return;}
    bool no_write_in_progress = write_msgs_.empty();
    write_msgs_.push_back(put_msg);
    if (no_write_in_progress) {
      int len=message_len(write_msgs_.front());
      boost::asio::async_write(socket_,boost::asio::buffer(write_msgs_.front()->data,len),
        boost::bind(&peer::handle_write,shared_from_this(),boost::asio::placeholders::error)); }
    mtx_.unlock();
  }

  int message_len(message_ptr msg) // shorten candidate vote messages if possible
  { if(do_sync){
      return(msg->len);}
    if(msg->data[0]==MSGTYPE_CNG && msg->len>4+64+10+sizeof(hash_t)){//shorten message if peer knows this hash
      //hash_s cand;
      //memcpy(cand.hash,msg->data+4+64+10,sizeof(hash_t));
      //candidate_ptr c_ptr=server_.known_candidate(cand,0);
      hash_s* cand=(hash_s*)(msg->data+4+64+10);
      candidate_ptr c_ptr=server_.known_candidate(*cand,0);
      if(c_ptr!=NULL && c_ptr->peers.find(svid)!=c_ptr->peers.end()){ // send only the hash, not the whole message
	std::cerr<<"WARNING, truncating cnd message\n";
        return(4+64+10+sizeof(hash_t));}}
    if(msg->data[0]==MSGTYPE_BLG && msg->len>4+64+10){
      header_t* h=(header_t*)(msg->data+4+64+10);
      if(peer_hs.head.now==h->now && !memcmp(peer_hs.head.nowhash,h->nowhash,SHA256_DIGEST_LENGTH)){
	std::cerr<<"WARNING, truncating blk message\n";
        return(4+64+10);}}
    return(msg->len);
  }

  void deliver(message_ptr msg)
  { if(do_sync){
      return;}
    msg->mtx_.lock();//TODO, do this right before sending the message ?
    msg->know.insert(svid);
    if(msg->status==MSGSTAT_SAV){
      if(!msg->load()){
        fprintf(stderr,"DELIVER problem for %d:%d\n",msg->svid,msg->msid);
        msg->mtx_.unlock();
        return;}}
    msg->busy.insert(svid);
    msg->mtx_.unlock();
    mtx_.lock();
    if(BLOCK_MODE_SERVER){
      wait_msgs_.push_back(msg);
      mtx_.unlock();
      return;}
    if(msg->data[0]==MSGTYPE_STP){
      BLOCK_MODE_SERVER=1;}
    bool no_write_in_progress = write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (no_write_in_progress){
      int len=message_len(write_msgs_.front());
      boost::asio::async_write(socket_,boost::asio::buffer(write_msgs_.front()->data,len),
        boost::bind(&peer::handle_write,shared_from_this(),boost::asio::placeholders::error));
    }
    mtx_.unlock();
  }

  void send_sync(message_ptr put_msg)
  { //FIXME, make this a blocking write
    assert(do_sync);
    put_msg->busy_insert(svid);//TODO, do this right before sending the message ?
    mtx_.lock(); //most likely no lock needed
    boost::asio::write(socket_,boost::asio::buffer(put_msg->data,put_msg->len));
    //bool no_write_in_progress = write_msgs_.empty();
    //write_msgs_.push_back(put_msg);
    //if(no_write_in_progress){
    //std::cerr<<"SENDING in sync mode "<<write_msgs_.front()->len<<" bytes\n";
    //  boost::asio::async_write(socket_,boost::asio::buffer(write_msgs_.front()->data,write_msgs_.front()->len),
    //    boost::bind(&peer::handle_write,shared_from_this(),boost::asio::placeholders::error));}
    //else{
    //std::cerr<<"SENDING in sync mode busy("<<write_msgs_.size()<<"), queued("<<write_msgs_.back()->len<<"B)\n";}
    mtx_.unlock();
  }

  void handle_write(const boost::system::error_code& error) //TODO change this later, dont send each message separately if possible
  {
std::cerr << "HANDLE WRITE start\n";
    if (!error) {
      mtx_.lock();
      write_msgs_.front()->mtx_.lock();
      write_msgs_.front()->busy.erase(svid); // will not work if same message queued 2 times
      write_msgs_.front()->sent.insert(svid);
      write_msgs_.front()->mtx_.unlock();
      bytes_out+=write_msgs_.front()->len;
      files_out++;
      if(write_msgs_.front()->data[0]==MSGTYPE_STP){
        BLOCK_MODE_SERVER=2;
        write_msgs_.pop_front();
        if(BLOCK_MODE_PEER){ // TODO what is this ???
          mtx_.unlock();
          write_peer_missing_messages();
          return;}
        mtx_.unlock();
        return;}
      if(!server_.do_sync && !do_sync && (write_msgs_.front()->data[0]==MSGTYPE_PUT || write_msgs_.front()->data[0]==MSGTYPE_DBP)){
//FIXME, do not add messages that are in last block
        if(svid_msid_new[write_msgs_.front()->svid]<write_msgs_.front()->msid){
          svid_msid_new[write_msgs_.front()->svid]=write_msgs_.front()->msid; // maybe a lock on svid_msid_new would help
          fprintf(stderr,"UPDATE PEER SVID_MSID: %d:%d\n",write_msgs_.front()->svid,write_msgs_.front()->msid);}}
std::cerr << "HANDLE WRITE sent "<<write_msgs_.front()->len<<" bytes\n";
      write_msgs_.pop_front();
      if (!write_msgs_.empty()) {
        //FIXME, now load the message from db if needed !!! do not do this when inserting in write_msgs_, unless You do not worry about RAM but worry about speed
        int len=message_len(write_msgs_.front());
std::cerr << "HANDLE WRITE sending "<<len<<" bytes\n";
        boost::asio::async_write(socket_,boost::asio::buffer(write_msgs_.front()->data,len),
          boost::bind(&peer::handle_write,shared_from_this(),boost::asio::placeholders::error)); }
      mtx_.unlock(); }
    else {
      std::cerr << "WRITE error\n";
      server_.leave(shared_from_this()); }
  }

  void handle_read_header(const boost::system::error_code& error)
  {
    if(error || !read_msg_->header(svid)){
      std::cerr << "READ error\n";
      server_.leave(shared_from_this());
      return;}
    bytes_in+=read_msg_->len;
    files_in++;
    read_msg_->know.insert(svid);
    if(read_msg_->len==message::header_length){
      if(!read_msg_->svid || srvs_.nodes.size()<=read_msg_->svid){ //unknown svid
        std::cerr << "ERROR message from unknown server "<< read_msg_->svid <<"\n";
        server_.leave(shared_from_this());
        return;}
      if(read_msg_->data[0]==MSGTYPE_PUT || read_msg_->data[0]==MSGTYPE_CNP || read_msg_->data[0]==MSGTYPE_BLP || read_msg_->data[0]==MSGTYPE_DBP){
        fprintf(stderr,"HASH offered:%016lX [%016lX] (%04X)\n",read_msg_->hash.num,*((uint64_t*)read_msg_->data),svid); // could be bad allignment
        if(read_msg_->data[0]==MSGTYPE_PUT){
          read_msg_->data[0]=MSGTYPE_GET;}
        if(read_msg_->data[0]==MSGTYPE_CNP){
          read_msg_->data[0]=MSGTYPE_CNG;}
        if(read_msg_->data[0]==MSGTYPE_BLP){
          read_msg_->data[0]=MSGTYPE_BLG;}
        if(read_msg_->data[0]==MSGTYPE_DBP){
          read_msg_->data[0]=MSGTYPE_DBG;}
        if(read_msg_->data[0]==MSGTYPE_GET || read_msg_->data[0]==MSGTYPE_DBG){
          mtx_.lock(); //concider locking svid_msid_new
          if(svid_msid_new[read_msg_->svid]<read_msg_->msid){
            svid_msid_new[read_msg_->svid]=read_msg_->msid;
            fprintf(stderr,"UPDATE PEER SVID_MSID: %d:%d\n",read_msg_->svid,read_msg_->msid);}
          mtx_.unlock();}
        if(server_.message_insert(read_msg_)>0){ //NEW, make sure to insert in correct containers
          if(read_msg_->data[0]==MSGTYPE_CNG ||
              (read_msg_->data[0]==MSGTYPE_BLG && server_.last_srvs_.now>=read_msg_->msid && (server_.last_srvs_.nodes[read_msg_->svid].status & SERVER_VIP)) ||
              read_msg_->data[0]==MSGTYPE_DBG ||
              (read_msg_->data[0]==MSGTYPE_GET && srvs_.nodes[read_msg_->svid].msid==read_msg_->msid-1 && server_.check_msgs_size()<MAX_CHECKQUE)){
            std::cerr << "REQUESTING MESSAGE from "<<svid<<" ("<<read_msg_->svid<<":"<<read_msg_->msid<<")\n";
            read_msg_->busy_insert(svid);
            deliver(read_msg_);}}} // request message if not known (inserted)
      else if(read_msg_->data[0]==MSGTYPE_GET || read_msg_->data[0]==MSGTYPE_CNG || read_msg_->data[0]==MSGTYPE_BLG || read_msg_->data[0]==MSGTYPE_DBG){
        fprintf(stderr,"HASH requested:%016lX [%016lX] (%04X)\n",read_msg_->hash.num,*((uint64_t*)read_msg_->data),svid); // could be bad allignment
	if(do_sync){
          read_msg_->path=peer_path;
          if(read_msg_->load()){
            std::cerr << "PROVIDING MESSAGE\n";
            send_sync(read_msg_);} //deliver will not work in do_sync mode
          else{
            std::cerr << "FAILED answering request for "<<read_msg_->svid<<":"<<read_msg_->msid<<" from "<<svid<<" (message not found in:"<<peer_path<<")\n";}}
        else{
          message_ptr msg=server_.message_find(read_msg_,svid);
          if(msg!=NULL){
            if(msg->len>message::header_length){
              if(msg->sent.find(svid)!=msg->sent.end()){
                //if(peer_hs.head.now+BLOCKSEC < msg->path ){ //FIXME, check correct condition !
                  std::cerr << "REJECTING download request for "<<msg->svid<<":"<<msg->msid<<" from "<<svid<<"\n";
                //}
                //else{
                //  std::cerr << "ACCEPTING download request for "<<msg->svid<<":"<<msg->msid<<" from "<<svid<<"\n";
                //  deliver(msg);}
                }
              else{
                std::cerr << "PROVIDING MESSAGE\n";
                msg->sent_insert(svid);
                deliver(msg);}} // must force deliver without checks
            else{ // no real message available
              std::cerr << "BAD get request from " << std::to_string(svid) << "\n";}}
          else{// concider loading from db if available
            std::cerr <<"MESSAGE "<< read_msg_->svid <<":"<< read_msg_->msid <<" not found, concider loading from db\n\n\n";}}}
      else if(read_msg_->data[0]==MSGTYPE_HEA){
	write_headers();}
      else if(read_msg_->data[0]==MSGTYPE_SER){ //servers request
	write_servers();}
      else if(read_msg_->data[0]==MSGTYPE_TXL){ //txs list request
	write_txslist();}
      else if(read_msg_->data[0]==MSGTYPE_USG){
	write_bank();}
      else if(read_msg_->data[0]==MSGTYPE_PAT){ //set current sync block
        memcpy(&peer_path,read_msg_->data+1,4);
	std::cerr<<"DEBUG, got sync path "<<peer_path<<"\n";}
      else if(read_msg_->data[0]==MSGTYPE_SOK){
        uint32_t now;
        memcpy(&now,read_msg_->data+1,4);
        std::cerr << "Authenticated, peer in sync at "<<now<<"\n";
        update_sync();
        do_sync=0;}
      else{
        int n=read_msg_->data[0];
        std::cerr << "ERROR message type " << std::to_string(n) << "received \n";
        server_.leave(shared_from_this());
        return;}
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));}
    else{
      if(read_msg_->data[0]==MSGTYPE_STP){
        std::cerr << "PEER in block mode\n";
        //could enter a different (sync) read sequence here;
        //read_msg_->data=(uint8_t*)std::realloc(read_msg_->data,read_msg_->len);
        assert(read_msg_->len==1+SHA256_DIGEST_LENGTH);
        boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_->data+message::header_length,read_msg_->len-message::header_length),
          boost::bind(&peer::handle_read_stop,shared_from_this(),boost::asio::placeholders::error));
	return;}
      if(read_msg_->data[0]==MSGTYPE_TXP){
        std::cerr << "READ txslist header\n";
        boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_->data+message::header_length,read_msg_->len-message::header_length),
          boost::bind(&peer::handle_read_txslist,shared_from_this(),boost::asio::placeholders::error));
	return;}
      if(read_msg_->data[0]==MSGTYPE_USR){
        //FIXME, accept only if needed !!
        std::cerr << "READ bank "<<read_msg_->svid<<"["<<read_msg_->len<<"] \n";
        boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_->data+message::header_length,read_msg_->len*sizeof(user_t)),
          boost::bind(&peer::handle_read_bank,shared_from_this(),boost::asio::placeholders::error));
	return;}
      if(read_msg_->data[0]==MSGTYPE_BLK){
        std::cerr << "READ block header\n";
        assert(read_msg_->len==4+64+10+sizeof(header_t) || read_msg_->len==4+64+10);
        boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_->data+message::header_length,read_msg_->len-message::header_length),
          boost::bind(&peer::handle_read_block,shared_from_this(),boost::asio::placeholders::error));
	return;}
      std::cerr << "WAIT read body\n";
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data+message::header_length,read_msg_->len-message::header_length),
        boost::bind(&peer::handle_read_body,shared_from_this(),boost::asio::placeholders::error));}
  }

  void update_sync(void) // send current inventory (all txs and dbl messages)
  { std::vector<uint64_t>txs;
    std::vector<uint64_t>dbl;
    server_.update_list(txs,dbl,svid);
    std::string data;
//TODO update svid_msid_new ??? [work without]
    for(auto it=txs.begin();it!=txs.end();it++){
      data.append((const char*)&(*it),sizeof(uint64_t));}
    for(auto it=dbl.begin();it!=dbl.end();it++){
      data.append((const char*)&(*it),sizeof(uint64_t));}
    message_ptr put_msg(new message(data.size()));
    memcpy(put_msg->data,data.c_str(),data.size());
    std::cerr<<"SENDING update with "<<data.size()<<" bytes\n";
    send_sync(put_msg);
  }

  void write_headers()
  { uint32_t from;
    memcpy(&from,read_msg_->data+1,4);
    uint32_t to=sync_hs.head.now;
    uint32_t num=((to-from)/BLOCKSEC)-1;
    if(num<=0){
      std::cerr<<"ERROR, failed to provide correct request (from:"<<from<<" to:"<<to<<")\n";
      server_.leave(shared_from_this()); // consider updateing client
      return;}
    std::cerr<<"SENDING block headers starting after "<<from<<" and ending before "<<to<<" ("<<num<<")\n"; 
    message_ptr put_msg(new message(SHA256_DIGEST_LENGTH+sizeof(headlink_t)*num));
    char* data=(char*)put_msg->data;
    for(uint32_t now=from+BLOCKSEC;now<to;now+=BLOCKSEC){
      headlink_t link;
      servers linkservers;
      linkservers.now=now;
      if(!linkservers.header_get()){
	std::cerr<<"ERROR, failed to provide header links\n";
        server_.leave(shared_from_this()); // consider updateing client
        return;}
      if(now==from+BLOCKSEC){
	memcpy(data,(const char*)linkservers.oldhash,SHA256_DIGEST_LENGTH);
	data+=SHA256_DIGEST_LENGTH;}
      linkservers.filllink(link);
      memcpy(data,(const char*)&link,sizeof(headlink_t));
      data+=sizeof(headlink_t);}
    assert(data==(char*)(put_msg->data+SHA256_DIGEST_LENGTH+sizeof(headlink_t)*num));
    send_sync(put_msg);
  }

  void handle_read_headers()
  { uint32_t to=peer_hs.head.now;
    servers sync_ls; //FIXME, use only the header data not "servers"
    sync_ls.loadhead(sync_hs.head);
    server_.peer_.lock();
    if(server_.headers.size()){
      std::cerr<<"USE last header\n";
      sync_ls=server_.headers.back();} // FIXME, use pointers/references maybe
    server_.peer_.unlock();
    uint32_t from=sync_ls.now;
    if(from>=to){
      return;}
    uint32_t num=(to-from)/BLOCKSEC;
    std::vector<servers> headers(num); //TODO, consider changing this to a list
    if(num>1){
      //if(server_.slow_sync(false,headers)<0){
      //  return;}
      std::cerr<<"SENDING block headers request\n";
      message_ptr put_msg(new message());
      put_msg->data[0]=MSGTYPE_HEA;
      memcpy(put_msg->data+1,&from,4);
      send_sync(put_msg);
      std::cerr<<"READING block headers starting after "<<from<<" and ending before "<<to<<" ("<<(num-1)<<")\n"; 
      char* data=(char*)malloc(SHA256_DIGEST_LENGTH+sizeof(headlink_t)*(num-1)); assert(peer_nods!=NULL);
      int len=boost::asio::read(socket_,boost::asio::buffer(data,SHA256_DIGEST_LENGTH+sizeof(headlink_t)*(num-1)));
      if(len!=(int)(SHA256_DIGEST_LENGTH+sizeof(headlink_t)*(num-1))){
        std::cerr << "READ headers error\n";
        free(data);
        server_.leave(shared_from_this());
        return;}
      //if(memcmp(data,sync_hs.head.oldhash,SHA256_DIGEST_LENGTH)){
      //  std::cerr << "ERROR, initial oldhash mismatch :-(\n";
      //  char hash[2*SHA256_DIGEST_LENGTH];
      //  ed25519_key2text(hash,data,SHA256_DIGEST_LENGTH);
      //  fprintf(stderr,"OLDHASH got  %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
      //  ed25519_key2text(hash,sync_hs.head.oldhash,SHA256_DIGEST_LENGTH);
      //  fprintf(stderr,"OLDHASH have %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
      //  free(data);
      //  server_.leave(shared_from_this());
      //  return;}
      char* d=data+SHA256_DIGEST_LENGTH;
      //reed hashes and compare
      for(uint32_t i=0,now=from+BLOCKSEC;now<to;now+=BLOCKSEC,i++){
        headlink_t* link=(headlink_t*)d;
        d+=sizeof(headlink_t);
        if(!i){
          headers[i].loadlink(*link,now,data);}
        else{
          headers[i].loadlink(*link,now,(char*)headers[i-1].nowhash);}
        headers[i].header_print();
        } //assert(i==num-1);
      free(data);
      if(memcmp(headers[num-2].nowhash,peer_hs.head.oldhash,SHA256_DIGEST_LENGTH)){
        std::cerr << "ERROR, hashing header chain :-(\n";
        char hash[2*SHA256_DIGEST_LENGTH];
        ed25519_key2text(hash,headers[num-2].nowhash,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"NOWHASH nowhash %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,peer_hs.head.oldhash,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"NOWHASH oldhash %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        server_.leave(shared_from_this());
        return;}}
    headers[num-1].loadhead(peer_hs.head);
    //if(memcmp(headers[num-1].nowhash,peer_hs.head.nowhash,SHA256_DIGEST_LENGTH)){
    //  std::cerr << "ERROR, hashing header chain end :-(\n";
    //  server_.leave(shared_from_this());
    //  return;}
    if(memcmp(headers[0].oldhash,sync_ls.nowhash,SHA256_DIGEST_LENGTH)){
      std::cerr << "ERROR, initial oldhash mismatch :-(\n";
      char hash[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(hash,headers[0].oldhash,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"NOWHASH got  %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
      ed25519_key2text(hash,sync_ls.nowhash,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"NOWHASH have %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
      server_.leave(shared_from_this());
      return;}
    std::cerr << "HASHES loaded\n";
    //server_.slow_sync(true,headers);
    server_.add_headers(headers);
    // send current sync path
    server_.peer_.lock();
    if(srvs_.now && server_.do_sync){
      message_ptr put_msg(new message());
      put_msg->data[0]=MSGTYPE_PAT;
      memcpy(put_msg->data+1,&srvs_.now,4);
      send_sync(put_msg);}
    server_.peer_.unlock();
    return;
  }

  void write_txslist()
  { servers header;
    memcpy(&header.now,read_msg_->data+1,4);
    if(!header.header_get()){
      std::cerr<<"FAILED to read header "<<header.now<<" for svid:"<<svid<<"\n"; //TODO, consider responding with error
      return;}
    int len=8+SHA256_DIGEST_LENGTH+header.txs*(2+4+SHA256_DIGEST_LENGTH);
    message_ptr put_msg(new message(len));
    put_msg->data[0]=MSGTYPE_TXP;
    memcpy(put_msg->data+1,&len,3); //bigendian
    memcpy(put_msg->data+4,&header.now,4);
    if(header.txs_get((char*)(put_msg->data+8))!=(int)(SHA256_DIGEST_LENGTH+header.txs*(2+4+SHA256_DIGEST_LENGTH))){
      std::cerr<<"FAILED to read txslist "<<header.now<<" for svid:"<<svid<<"\n"; //TODO, consider responding with error
      return;}
    std::cerr<<"SENDING block txslist for block "<<header.now<<" to svid:"<<svid<<"\n";
    send_sync(put_msg);
  }

  void handle_read_txslist(const boost::system::error_code& error)
  { if(error){
      std::cerr << "ERROR reading txslist\n";
      server_.leave(shared_from_this());
      return;}
    servers header;
    memcpy(&header.now,read_msg_->data+4,4);
    if(server_.get_txslist!=header.now){
      std::cerr << "ERROR got wrong txslist id\n"; // consider updating server
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      return;}
    header.header_get();
    if(read_msg_->len!=(8+SHA256_DIGEST_LENGTH+header.txs*(2+4+SHA256_DIGEST_LENGTH))){
      std::cerr << "ERROR got wrong txslist length\n"; // consider updating server
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      return;}
    if(memcmp(read_msg_->data+8,header.txshash,SHA256_DIGEST_LENGTH)){
      std::cerr << "ERROR got wrong txslist txshash\n"; // consider updating server
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      return;}
    std::map<uint64_t,message_ptr> map;
    header.txs_map((char*)(read_msg_->data+8),map,opts_.svid);
    server_.put_txslist(header.now,map);
    read_msg_ = boost::make_shared<message>();
    boost::asio::async_read(socket_,
      boost::asio::buffer(read_msg_->data,message::header_length),
      boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
  }

  void write_bank()
  { uint32_t path=read_msg_->msid;
    uint16_t bank=read_msg_->svid;

//FIXME !!!
//FIXME !!! check if the first undo directory is needed !!!
//FIXME !!! if peer starts at 'path' and receives messages in 'path' then it has to start from state before path
//FIXME !!! then the path/und files are needed to start in correct position
//FIXME !!!

    if(1+(srvs_.now-path)/BLOCKSEC>MAX_UNDO){
      std::cerr<<"ERROR, too old sync point\n";
      server_.leave(shared_from_this());
      return;}
    //check the numer of users at block time
    servers s;
    s.get(path);
    if(bank>=s.nodes.size()){
      std::cerr<<"ERROR, bad bank at sync point\n";
      server_.leave(shared_from_this());
      return;}
    std::vector<int> ud;
    uint32_t users=s.nodes[bank].users;
    //TODO, consider checking that the final hash is correct
    char filename[64];
    sprintf(filename,"usr/%04X.dat",bank);
    int fd=open(filename,O_RDONLY);
    int ld=0;
    if(fd<0){
      std::cerr<<"ERROR, failed to open bank, weird !!!\n";
      server_.leave(shared_from_this());
      return;}
    for(uint32_t block=path+BLOCKSEC;block<=srvs_.now;block++){
      sprintf(filename,"%08X/und/%04X.dat",block,bank);
      int fd=open(filename,O_RDONLY);
      if(fd<0){
        continue;}
      fprintf(stderr,"USING bank %04X block %08X undo %s\n",bank,path,filename);
      ud.push_back(fd);}
    if(ud.size()){
      ld=ud.back();}
    int msid=0;
     int64_t weight=0;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for(uint32_t user=0;user<users;msid++){
      uint32_t end=user+MESSAGE_USRCHUNK;
      if(end>users){
        end=users;}
      int len=end-user;
      message_ptr put_msg(new message(8+len*sizeof(user_t))); //6Mb working space
      put_msg->data[0]=MSGTYPE_USR;
      memcpy(put_msg->data+1,&len,3); // this is number of users (max 0x10000)
      memcpy(put_msg->data+4,&msid,2); // this is the chunk id
      memcpy(put_msg->data+6,&bank,2); // this is the bank
      user_t* u=(user_t*)(put_msg->data+8);
      for(;user<end;user++,u++){
        u->msid=0;
        int junk=0;
        for(auto it=ud.begin();it!=ud.end();it++,junk++){
          read(*it,(char*)u,sizeof(user_t));
          if(u->msid){
            fprintf(stderr,"USING bank %04X undo user %08X file %d\n",bank,user,junk);
            lseek(fd,sizeof(user_t),SEEK_CUR);
            for(it++;it!=ud.end();it++){
              lseek(*it,sizeof(user_t),SEEK_CUR);}
            goto NEXTUSER;}}
        read(fd,u,sizeof(user_t));
        if(ld){ //confirm again that the undo file has not changed
          user_t v;
          v.msid=0;
          lseek(ld,-sizeof(user_t),SEEK_CUR);
          read(ld,&v,sizeof(user_t));
          if(v.msid){ //overwrite user info
            memcpy((char*)u,&v,sizeof(user_t));}}
        NEXTUSER:;
        //print user
        fprintf(stderr,"USER:%08X m:%08X t:%08X s:%04X b:%04X u:%08X l:%08X r:%08X v:%016lX\n",
          user,u->msid,u->time,u->stat,u->node,u->user,u->lpath,u->rpath,u->weight);
        weight+=u->weight;
        SHA256_Update(&sha256,u,sizeof(user_t));}
      fprintf(stderr,"SENDING bank %04X block %08X msid %08X max user %08X sum %016lX\n",bank,path,msid,user,s.nodes[bank].weight);
      send_sync(put_msg); // send even if we have errors
      if(user==users){
        uint8_t hash[32];
        SHA256_Final(hash,&sha256);
//FIXME, should compare with previous hash !!!
        if(s.nodes[bank].weight!=weight){
          //unlink(filename); //TODO, enable this later
          fprintf(stderr,"ERROR sending bank %04X bad sum %016lX<>%016lX\n",bank,s.nodes[bank].weight,weight);
          server_.leave(shared_from_this());
          return;}
        if(memcmp(s.nodes[bank].hash,hash,32)){
          //unlink(filename); //TODO, enable this later
          std::cerr << "ERROR sending bank "<<bank<<" (bad hash)\n";
          server_.leave(shared_from_this());
          return;}}}
  }

  void handle_read_bank(const boost::system::error_code& error)
  { static uint16_t last_bank=0;
    static uint16_t last_msid=0;
    static  int64_t weight=0;
    static SHA256_CTX sha256;
    int fd;
    if(error){
      std::cerr << "ERROR reading message\n";
      server_.leave(shared_from_this());
      return;}
    if(!server_.do_sync){
      std::cerr << "DEBUG ignore usr message\n";
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      return;}
    //read_msg_->read_head();
    uint16_t bank=read_msg_->svid;
    if(!bank || bank>=server_.last_srvs_.nodes.size()){
      std::cerr << "ERROR reading bank "<<bank<<" (bad svid)\n";
      server_.leave(shared_from_this());
      return;}
    //if(!read_msg_->msid || read_msg_->msid>=server_.last_srvs_.now){
    //  std::cerr << "ERROR reading bank "<<bank<<" (bad block)\n";
    //  server_.leave(shared_from_this());
    //  return;}
    if(read_msg_->len+0x10000*read_msg_->msid>server_.last_srvs_.nodes[bank].users){
      std::cerr << "ERROR reading bank "<<bank<<" (too many users)\n";
      server_.leave(shared_from_this());
      return;}
    std::cerr << "NEED bank "<<bank<<" ?\n";
    uint64_t hnum=server_.need_bank(bank);
    if(!hnum){
      std::cerr << "NEED bank "<<bank<<" NO\n";
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      return;}
    std::cerr << "PROCESSING bank "<<bank<<"\n";
    char filename[64];
    sprintf(filename,"usr/%04X.dat.%04X",bank,svid);
    if(!read_msg_->msid){
      last_bank=bank;
      last_msid=0;
      weight=0;
      SHA256_Init(&sha256);
      fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);}
    else{
      if(last_bank!=bank||last_msid!=read_msg_->msid-1){
        unlink(filename);
        std::cerr << "ERROR reading bank "<<bank<<" (incorrect message order)\n";
        server_.leave(shared_from_this());
        return;}
      last_msid=read_msg_->msid;
      fd=open(filename,O_WRONLY|O_APPEND,0644);}
    if(fd<0){ //trow or something :-)
      std::cerr << "ERROR creating bank "<<bank<<" file\n";
      exit(-1);}
    if((int)(read_msg_->len*sizeof(user_t))!=write(fd,read_msg_->data+8,read_msg_->len*sizeof(user_t))){
      close(fd);
      unlink(filename);
      std::cerr << "ERROR writing bank "<<bank<<" file\n";
      exit(-1);}
    close(fd);
    user_t* u=(user_t*)(read_msg_->data+8);
    for(uint32_t i=0;i<read_msg_->len;i++,u++){
      weight+=u->weight;
      SHA256_Update(&sha256,u,sizeof(user_t));}
    if(read_msg_->len+0x10000*read_msg_->msid<server_.last_srvs_.nodes[bank].users){
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      return;}
    uint8_t hash[32];
    SHA256_Final(hash,&sha256);
    if(memcmp(server_.last_srvs_.nodes[bank].hash,hash,32)){
      //unlink(filename); //TODO, enable this later
      std::cerr << "ERROR reading bank "<<bank<<" (bad hash)\n";
      server_.leave(shared_from_this());
      return;}
    if(server_.last_srvs_.nodes[bank].weight!=weight){
      //unlink(filename); //TODO, enable this later
      std::cerr << "ERROR reading bank "<<bank<<" (bad sum)\n";
      server_.leave(shared_from_this());
      return;}
    char new_name[64];
    sprintf(new_name,"usr/%04X.dat",bank);
    rename(filename,new_name);
    std::cerr << "PROCESSED bank "<<bank<<"\n";
    server_.have_bank(hnum);
    std::cerr << "CONTINUE\n";
    read_msg_ = boost::make_shared<message>();
    boost::asio::async_read(socket_,
      boost::asio::buffer(read_msg_->data,message::header_length),
      boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
  }

  void write_servers()
  { uint32_t now;
    memcpy(&now,read_msg_->data+1,4);
    fprintf(stderr,"SENDING block servers for block %08X\n",now);
    if(server_.last_srvs_.now!=now){
      //FIXME, try getting data from repository
      std::cerr<<"ERROR, bad time "<<server_.last_srvs_.now<<"<>"<<now<<"\n";
      server_.leave(shared_from_this());
      return;}
    message_ptr put_msg(new message(server_.last_srvs_.nod*sizeof(node_t)));
    if(!server_.last_srvs_.copy_nodes((node_t*)put_msg->data,server_.last_srvs_.nod)){
      std::cerr<<"ERROR, failed to copy nodes :-(\n";
      server_.leave(shared_from_this());
      return;}
    send_sync(put_msg);
  }

  void handle_read_servers()
  { 
    if(server_.fast_sync(false,peer_hs.head,peer_nods,peer_svsi)<0){
      return;}
    //send 
    std::cerr<<"SENDING block servers request\n";
    message_ptr put_msg(new message());
    put_msg->data[0]=MSGTYPE_SER;
    memcpy(put_msg->data+1,&peer_hs.head.now,4);
    send_sync(put_msg);
    peer_nods=(node_t*)malloc(peer_hs.head.nod*sizeof(node_t)); assert(peer_nods!=NULL);
    int len=boost::asio::read(socket_,boost::asio::buffer(peer_nods,peer_hs.head.nod*sizeof(node_t)));
    if(len!=(int)(peer_hs.head.nod*sizeof(node_t))){
      std::cerr << "READ servers error\n";
      free(peer_svsi);
      free(peer_nods);
      server_.leave(shared_from_this());
      return;}
    if(server_.last_srvs_.check_nodes(peer_nods,peer_hs.head.nod,peer_hs.head.nodhash)){
      std::cerr << "SERVERS incompatible with hash\n";
      char hash[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(hash,sync_hs.head.nodhash,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"NODHASH peer %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
      free(peer_svsi);
      free(peer_nods);
      server_.leave(shared_from_this());
      return;}
    std::cerr<<"FINISH SYNC\n";
    server_.fast_sync(true,peer_hs.head,peer_nods,peer_svsi); // should use last_srvs_ instead of sync_...
    free(peer_svsi);
    free(peer_nods);
  }

  int authenticate() //FIXME, don't send last block because signatures are missing; send the one before last
  { uint32_t now=time(NULL);
    uint32_t blocknow=now-(now%BLOCKSEC);
    memcpy(&peer_hs,read_msg_->data+4+64+10,sizeof(handshake_t));
    srvs_.header_print(peer_hs.head);
    //memcpy(&sync_head,&peer_hs.head,sizeof(header_t));
    msid=read_msg_->msid;
    svid=read_msg_->svid;
    if(read_msg_->svid==opts_.svid){ std::cerr << "ERROR: connecting to myself\n";
      return(0);}
    if(server_.duplicate(shared_from_this())){ std::cerr << "ERROR: server already connected\n";
      return(0);}
    if(peer_hs.head.nod>srvs_.nodes.size() && incoming_){ std::cerr << "ERROR: too high number of servers for incomming connection\n";
      return(0);}
    if(read_msg_->now>now+5 || read_msg_->now<now-5){ std::cerr << "ERROR: bad time " << std::to_string(read_msg_->now) << "<>" << std::to_string(now) << "\n";
      return(0);}
    if(peer_hs.msid>server_.msid_){ //FIXME, we will check this later, maybe not needed here
      std::cerr<<"WARNING, possible message loss, should run full resync ("<<peer_hs.msid<<">"<<server_.msid_<<")\n";}
    if(peer_hs.msid && peer_hs.msid==server_.msid_ && memcmp(peer_hs.msha,srvs_.nodes[opts_.svid].msha,SHA256_DIGEST_LENGTH)){ //FIXME, we should check this later, maybe not needed
      std::cerr<<"WARNING, last message hash mismatch, should run full resync\n";}
    if(incoming_){
      message_ptr sync_msg=server_.write_handshake(svid,sync_hs); // sets sync_hs
      sync_msg->print("; send welcome");
      send_sync(sync_msg);}
    if(peer_hs.head.now==sync_hs.head.now){
      if(memcmp(peer_hs.head.oldhash,sync_hs.head.oldhash,SHA256_DIGEST_LENGTH)){
        char hash[2*SHA256_DIGEST_LENGTH];
        std::cerr << "ERROR oldhash mismatch, FIXME, move back more blocks to sync\n";
        ed25519_key2text(hash,sync_hs.head.oldhash,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"HASH have %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,peer_hs.head.oldhash,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"HASH got  %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        return(0);}
      if(memcmp(peer_hs.head.nowhash,sync_hs.head.nowhash,SHA256_DIGEST_LENGTH)){
        char hash[2*SHA256_DIGEST_LENGTH];
        std::cerr << "WARNING nowhash mismatch, not tested :-( move back one block to sync\n";
        ed25519_key2text(hash,sync_hs.head.nowhash,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"HASH have %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
        ed25519_key2text(hash,peer_hs.head.nowhash,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"HASH got  %.*s\n",2*SHA256_DIGEST_LENGTH,hash);}}
    if(!server_.do_sync){ //we are in sync
      if(peer_hs.head.now>sync_hs.head.now){
        std::cerr << "ERROR not ready to connect with this server\n";
        return(0);}
      if(peer_hs.head.now<sync_hs.head.now){
        std::cerr << "Authenticated, provide sync data\n";
        //write_sync(); // peer will disconnect if peer does not want the data
	int vok=sync_hs.head.vok;
	int vno=sync_hs.head.vno;
        int size=sizeof(svsi_t)*(vok+vno);
        message_ptr put_msg(new message(size));
        server_.last_srvs_.get_signatures(sync_hs.head.now,put_msg->data,vok,vno);//TODO, check if server_.last_srvs_ has public keys
        std::cerr<<"SENDING last block signatures "<<vok<<"+"<<vno<<" ("<<size<<" bytes)\n";
        send_sync(put_msg);
        return(1);}
      std::cerr << "Authenticated, peer in sync\n";
      update_sync();
      do_sync=0;
      return(1);}
    // try syncing from this server
    //if(peer_hs.head.now!=srvs_.now-BLOCKSEC)
    if(peer_hs.head.now!=blocknow-BLOCKSEC){
      std::cerr << "PEER not in sync\n";
      return(0);}
    if(peer_hs.head.vok<server_.vip_max/2 && (!opts_.mins || peer_hs.head.vok<opts_.mins)){
      std::cerr << "PEER not enough signatures\n";
      return(0);}
    std::cerr << "Authenticated, expecting sync data ("<<(uint32_t)((peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t))<<" bytes)\n";
    peer_svsi=(svsi_t*)malloc((peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t)); //FIXME, send only vok
    int len=boost::asio::read(socket_,boost::asio::buffer(peer_svsi,(peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t)));
    if(len!=(int)((peer_hs.head.vok+peer_hs.head.vno)*sizeof(svsi_t))){
      std::cerr << "READ block signatures error\n";
      free(peer_svsi);
      return(0);}
    std::cerr<<"BLOCK sigatures recieved ok:"<<peer_hs.head.vok<<" no:"<<peer_hs.head.vno<<"\n";
    server_.last_srvs_.check_signatures(peer_hs.head,peer_svsi);//TODO, check if server_.last_srvs_ has public keys
    if(peer_hs.head.vok<server_.vip_max/2 && (!opts_.mins || peer_hs.head.vok<opts_.mins)){
      std::cerr << "READ not enough signatures after validaiton\n";
      free(peer_svsi);
      return(0);}
    //now decide if You want to sync to last stage first ; or load missing blocks and messages first, You can decide based on size of databases and time to next block
    //the decision should be in fact made at the beginning by the server
    if(opts_.fast){
      handle_read_servers();
      server_.last_srvs_.header(sync_hs.head);} // set new starting point for headers synchronisation
    handle_read_headers();
    do_sync=0; // set peer in sync, we are not in sync (server_.do_sync==1)
    return(1);
  }

  void handle_read_body(const boost::system::error_code& error)
  {
    if(error){
      std::cerr << "ERROR reading message\n";
      server_.leave(shared_from_this());
      return;}
    read_msg_->read_head();
    if(!read_msg_->svid || read_msg_->svid>=srvs_.nodes.size()){
      std::cerr << "ERROR reading head\n";
      server_.leave(shared_from_this());
      return;}
    if(read_msg_->check_signature(srvs_.nodes[read_msg_->svid].pk,opts_.svid)){
      std::cerr << "BAD signature from "<<read_msg_->svid<<"\n";
      ed25519_printkey(srvs_.nodes[read_msg_->svid].pk,32);
      server_.leave(shared_from_this());
      return;}
    if(!svid){ // FIXME, move this to 'start()'
      if(!authenticate()){
        std::cerr << "NOT authenticated\n";
        server_.leave(shared_from_this());
        return;}
      std::cerr << "CONTINUE\n";
      read_msg_ = boost::make_shared<message>();
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,message::header_length),
        boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
      return;}
    assert(read_msg_->hash.dat[1]!=MSGTYPE_BLK);
    //if this is a candidate vote
    if(read_msg_->hash.dat[1]==MSGTYPE_CND){
//FIXME, move this to handle_read_candidate
      if(!parse_vote()){
        std::cerr << "PARSE vote FAILED\n";
        server_.leave(shared_from_this());
        return;}}
    //TODO, check if correct server message number ! and update the server
    std::cerr << "INSERT message\n";
    if(server_.message_insert(read_msg_)==-1){ //NEW, insert in correct containers
      std::cerr << "INSERT message FAILED\n";
      server_.leave(shared_from_this());
      return;}
    // consider waiting here if we have too many messages to process
    read_msg_ = boost::make_shared<message>();
    boost::asio::async_read(socket_,
      boost::asio::buffer(read_msg_->data,message::header_length),
      boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
  }

  void handle_read_block(const boost::system::error_code& error)
  { std::cerr << "BLOCK, got BLOCK\n";
    if(error || !svid){
      std::cerr << "BLOCK READ error\n";
      server_.leave(shared_from_this());
      return;}
    if(read_msg_->len==4+64+10){ //adding missing data
      if(server_.last_srvs_.now!=read_msg_->msid){
        fprintf(stderr,"BLOCK READ problem, ignoring short BLK message due to msid mismatch, msid:%08X block:%08X\n",read_msg_->msid,server_.last_srvs_.now);
        read_msg_ = boost::make_shared<message>();
        boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_->data,message::header_length),
          boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
        return;}
      header_t* h=(header_t*)(read_msg_->data+4+64+10);
      read_msg_->len+=sizeof(header_t);
      read_msg_->data=(uint8_t*)realloc(read_msg_->data,read_msg_->len); // throw if no RAM ???
      server_.last_srvs_.header(*h);}
    read_msg_->read_head();
    if(!read_msg_->svid || read_msg_->svid>=srvs_.nodes.size()){
      std::cerr << "ERROR reading head\n";
      server_.leave(shared_from_this());
      return;}
    if(read_msg_->check_signature(srvs_.nodes[read_msg_->svid].pk,opts_.svid)){
      std::cerr << "BLOCK signature error\n";
      server_.leave(shared_from_this());
      return;}
    if((read_msg_->msid!=server_.last_srvs_.now && read_msg_->msid!=srvs_.now) || read_msg_->now<read_msg_->msid || read_msg_->now>=read_msg_->msid+2*BLOCKSEC){
      fprintf(stderr,"BLOCK TIME error now:%08X msid:%08X block[-1].now:%08X block[].now:%08X \n",read_msg_->now,read_msg_->msid,server_.last_srvs_.now,srvs_.now);
      server_.leave(shared_from_this());
      return;}
    if(read_msg_->svid==svid){
      std::cerr << "BLOCK from peer "<<svid<<"\n";
      memcpy(&peer_hs.head,read_msg_->data+4+64+10,sizeof(header_t));
      char hash[2*SHA256_DIGEST_LENGTH];
      //ed25519_key2text(hash,oldhash,SHA256_DIGEST_LENGTH);
      ed25519_key2text(hash,peer_hs.head.nowhash,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"BLOCK %08X %.*s from PEER\n",peer_hs.head.now,2*SHA256_DIGEST_LENGTH,hash);}
    std::cerr << "INSERT BLOCK message\n";
    if(server_.message_insert(read_msg_)==-1){ //NEW, insert in correct containers
      std::cerr << "INSERT BLOCK message FAILED\n";
      server_.leave(shared_from_this());
      return;}
    read_msg_ = boost::make_shared<message>();
    boost::asio::async_read(socket_,
      boost::asio::buffer(read_msg_->data,message::header_length),
      boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
  }

  void handle_read_stop(const boost::system::error_code& error)
  {
    std::cerr << "STOP, got STOP\n";
    if(error){
      std::cerr << "READ error\n";
      server_.leave(shared_from_this());
      return;}
    memcpy(last_message_hash,read_msg_->data+1,SHA256_DIGEST_LENGTH);
      char hash[2*SHA256_DIGEST_LENGTH]; hash[2*SHA256_DIGEST_LENGTH-1]='?';
      ed25519_key2text(hash,last_message_hash,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"LAST HASH got %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    boost::asio::async_read(socket_,
      boost::asio::buffer(read_msg_->data,2),
      boost::bind(&peer::read_peer_missing_header,shared_from_this(),boost::asio::placeholders::error));
    mtx_.lock();
    BLOCK_MODE_PEER=1;
    if(BLOCK_MODE_SERVER>1){
      mtx_.unlock();
      write_peer_missing_messages();
      return;}
    mtx_.unlock();
  }

  void svid_msid_rollback(message_ptr msg) // remove deleted message from known messages map
  { if(svid_msid_new[msg->svid]>msg->msid-1){
      svid_msid_new[msg->svid]=msg->msid-1;}
  }

  void write_peer_missing_messages()
  { std::string data;
    uint16_t server_missed=0;

    svid_msid_server_missing.clear();
    //FIXME, check handling of invalidated messages
    for(auto it=svid_msid_new.begin();it!=svid_msid_new.end();++it){ // TODO TODO svid_msid_new must not change after thread_clock entered block mode
      if(it->first<server_.last_srvs_.nodes.size() && it->second==server_.last_srvs_.nodes[it->first].msid){
        continue;}
      uint32_t msid=0;
      auto me=server_.last_svid_msgs.find(it->first);
      if(me!=server_.last_svid_msgs.end()){
        msid=me->second->msid;}
      if(msid<it->second){
        data.append((const char*)&it->first,2);
        data.append((const char*)&msid,4);
        svidmsid_t svms;
        svms.svid=it->first;
	svms.msid=msid;
        svid_msid_server_missing.push_back(svms);
        fprintf(stderr,"SERVER missed %d:%d>%d\n",it->first,it->second,svms.msid);
        server_missed++;}}
    fprintf(stderr,"SERVER missed %d in total\n",server_missed);
    message_ptr put_msg(new message(2+6*server_missed));
    memcpy(put_msg->data,&server_missed,2);
    if(server_missed){
      memcpy(put_msg->data+2,data.c_str(),6*server_missed);}
    mtx_.lock(); // needed ?
    write_msgs_.push_front(put_msg); // to prevent data loss
    mtx_.unlock(); // needed ?
    boost::asio::async_write(socket_,boost::asio::buffer(put_msg->data,put_msg->len),
      boost::bind(&peer::write_peer_missing_hashes,shared_from_this(),boost::asio::placeholders::error)); 
  }

  void read_peer_missing_header(const boost::system::error_code& error)
  { if(error){
      std::cerr << "READ read_peer_missing_header error\n";
      server_.leave(shared_from_this());
      return;}
    memcpy(&peer_missed,read_msg_->data,2);
    if(peer_missed){
      free(read_msg_->data);
      read_msg_->data=(uint8_t*)malloc(6*peer_missed);
      fprintf(stderr,"PEER missed in total %d\n",peer_missed);
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,6*peer_missed),
        boost::bind(&peer::read_peer_missing_messages,shared_from_this(),boost::asio::placeholders::error));}
    else{
      std::cerr << "PEER has all\n";
      svid_msid_peer_missing.clear();
      write_peer_missing_hashes(error);}
  }

  void read_peer_missing_messages(const boost::system::error_code& error)
  { if(error){
      std::cerr << "READ read_peer_missing_messages error\n";
      server_.leave(shared_from_this());
      return;}
    std::cerr << "PEER read peer missing messages\n";
    // create peer_missing list;
    svid_msid_peer_missing.clear();
    svid_msid_peer_missing.reserve(peer_missed);
    for(int i=0;i<peer_missed;i++){
      svidmsid_t svms;
      memcpy(&svms.svid,read_msg_->data+6*i,2);
      memcpy(&svms.msid,read_msg_->data+6*i+2,4);
      if(svms.svid>=srvs_.nodes.size()){
        std::cerr << "ERROR read_peer_missing_messages peersvid\n";
        server_.leave(shared_from_this());
        return;}
      fprintf(stderr,"PEER missed %d:%d<?\n",svms.svid,svms.msid);
      svid_msid_peer_missing.push_back(svms);}
    write_peer_missing_hashes(error);
  }

  void write_peer_missing_hashes(const boost::system::error_code& error)
  { if(error){
      std::cerr << "ERROR write_peer_missing_hashes error\n";
      server_.leave(shared_from_this());
      return;}
    mtx_.lock();
    std::cerr << "PEER write peer missing hashes\n";
    if(BLOCK_MODE_PEER<2){ // must be called 2 times, from write_peer_missing_messages and from read_peer_missing_messages; second call writes response
      BLOCK_MODE_PEER=2;
      mtx_.unlock();
      return;}
    write_msgs_.pop_front();
    mtx_.unlock();
    std::cerr << "PEER write peer missing hashes run\n";
    // we have svid_msid_peer_missing defined, lets send missing hashes
    // first let's check if the msids are correct (must be!) ... later we can consider not sending them, but we will need to make sure we know about all double spends
    if(peer_missed){
      int i=0;
      message_ptr put_msg(new message((4+SHA256_DIGEST_LENGTH)*peer_missed));
      for(auto it=svid_msid_peer_missing.begin();it!=svid_msid_peer_missing.end();++it,i++){
        auto mp=server_.last_svid_msgs.find(it->svid);
        if(mp==server_.last_svid_msgs.end()){

//FIXME, maybe both servers missed this, why ???

          std::cerr << "ERROR write_peer_missing_hashes error (bad svid:" << std::to_string(it->svid) << ")\n";
          server_.leave(shared_from_this());
          return;}
        //if(it->msid!=mp->second->msid)  //
        //  fprintf(stderr,"PEER write_peer_missing_hashes error (bad msid %d<>%d)\n",it->msid,mp->second->msid);
        //  server_.leave(shared_from_this());
        //  return; 
        char sigh[2*SHA256_DIGEST_LENGTH];
        ed25519_key2text(sigh,mp->second->sigh,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"HSEND: %d:%d>%d %.*s\n",(int)(it->svid),(int)(mp->second->msid),it->msid,2*SHA256_DIGEST_LENGTH,sigh);
      //std::cerr << "PHASH: " << it->first << ":" << it->second.msid << " " << it->second.sigh[0] << "..." << it->second.sigh[31] << "\n";
        memcpy(put_msg->data+i*(4+SHA256_DIGEST_LENGTH),&mp->second->msid,4);
        memcpy(put_msg->data+i*(4+SHA256_DIGEST_LENGTH)+4,mp->second->sigh,SHA256_DIGEST_LENGTH);}
      assert(i==peer_missed);
      assert(write_msgs_.empty());
      mtx_.lock(); // needed ?
      write_msgs_.push_front(put_msg);
      mtx_.unlock(); // needed ?
      fprintf(stderr,"SENDING %d hashes to PEER [%d]\n",peer_missed,write_msgs_.front()->len);
      boost::asio::async_write(socket_,boost::asio::buffer(write_msgs_.front()->data,write_msgs_.front()->len),
        boost::bind(&peer::peer_block_finish,shared_from_this(),boost::asio::placeholders::error));}
    else{
      BLOCK_MODE_SERVER=3;}
    if(svid_msid_server_missing.size()>0){
      //BLOCK_MODE_SERVER=3;
      // prepera buffer to read missing 
      free(read_msg_->data);
      read_msg_->len=(4+SHA256_DIGEST_LENGTH)*svid_msid_server_missing.size();
      read_msg_->data=(uint8_t*)malloc(read_msg_->len);
      fprintf(stderr,"WAITING for %lu hashes from PEER [%d]\n",svid_msid_server_missing.size(),read_msg_->len);
      boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_->data,read_msg_->len),
        boost::bind(&peer::read_peer_missing_hashes,shared_from_this(),boost::asio::placeholders::error));}
    else{ // go back to standard write mode
      read_peer_missing_hashes(error);}
  }

  void message_phash(uint8_t* mhash,std::map<uint16_t,msidhash_t>& map)
  { SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for(std::map<uint16_t,msidhash_t>::iterator it=map.begin();it!=map.end();++it){
      if(!it->second.msid){
        continue;}
      char sigh[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(sigh,it->second.sigh,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"PHASH: %d:%d %.*s\n",(int)(it->first),(int)(it->second.msid),2*SHA256_DIGEST_LENGTH,sigh);
      //std::cerr << "PHASH: " << it->first << ":" << it->second.msid << " " << it->second.sigh[0] << "..." << it->second.sigh[31] << "\n";
      SHA256_Update(&sha256,it->second.sigh,sizeof(hash_t));}
    SHA256_Final(mhash, &sha256);
  }

  void read_peer_missing_hashes(const boost::system::error_code& error)
  { 
    std::cerr << "PEER read peer missing hashes\n";

    svid_msha=server_.svid_msha;

    // newer hashes from peer
    int i=0;
    svid_miss.clear(); // from peer (missed by server)
    for(auto it=svid_msid_server_missing.begin();it!=svid_msid_server_missing.end();++it,i++){
      msidhash_t msha;
      memcpy(&msha.msid,read_msg_->data+i*(4+SHA256_DIGEST_LENGTH),4);
      memcpy(msha.sigh,read_msg_->data+i*(4+SHA256_DIGEST_LENGTH)+4,SHA256_DIGEST_LENGTH);
      svid_msha[it->svid]=msha;
      svid_miss[it->svid]=msha;
      char hash[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(hash,msha.sigh,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"PEER %d:%d->%d %.*s\n",it->svid,it->msid,msha.msid,2*SHA256_DIGEST_LENGTH,hash);}

    //mpeer.reserve(svid_msid_peer_missing.size());
    bool failed=false;
    svid_have.clear(); // missed by peer
    for(auto it=svid_msid_peer_missing.begin();it!=svid_msid_peer_missing.end();++it){
      if(!it->msid){
        msidhash_t msha;
        msha.msid=0;
        bzero(msha.sigh,sizeof(hash_t));
        svid_msha[it->svid]=msha;
        svid_have[it->svid]=msha;
        char hash[2*SHA256_DIGEST_LENGTH];
        ed25519_key2text(hash,msha.sigh,SHA256_DIGEST_LENGTH);
        fprintf(stderr,"PEER %d:?->%d %.*s\n",it->svid,msha.msid,2*SHA256_DIGEST_LENGTH,hash);
        continue;}
      // find message
      message_ptr pm=server_.message_svidmsid(it->svid,it->msid);
      if(pm==NULL){
        fprintf(stderr,"ERROR failed to find %d:%d\n",it->svid,it->msid);
        server_.leave(shared_from_this());
        return;}
      if(pm->got<srvs_.now+BLOCKSEC-MESSAGE_MAXAGE){ // we will not accept missing this message
        // do not accept candidates with missing: double spend, my messeges, old messages
        fprintf(stderr,"BLOCK failed because old message (%d:%d) lost (%d<%d-%d)\n",pm->svid,pm->msid,pm->got,srvs_.now+BLOCKSEC,MESSAGE_MAXAGE);
        failed=true;}
      auto sm=server_.last_svid_msgs.find(it->svid);
      if(sm==server_.last_svid_msgs.end()){
        fprintf(stderr,"ERROR internal error when checking last message for node %d(>%d)\n",it->svid,it->msid);
        server_.leave(shared_from_this());
        return;}
      //if(sm->second->svid==opts_.svid) // we will not accept missing this message
      //  // do not accept candidates with missing: my messeges
      //  failed=true;
      if(sm->second->msid==0xffffffff){ // we will not accept missing this message
        // do not accept candidates with missing: double spend
        fprintf(stderr,"BLOCK failed because dbl spend server accepted\n");
        failed=true;}
      msidhash_t msha;
      msha.msid=pm->msid;
      memcpy(msha.sigh,pm->sigh,sizeof(hash_t));
      svid_msha[it->svid]=msha;
      svid_have[it->svid]=msha;
      char hash[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(hash,msha.sigh,SHA256_DIGEST_LENGTH);
      fprintf(stderr,"PEER %d:%d->%d %.*s\n",it->svid,sm->second->msid,msha.msid,2*SHA256_DIGEST_LENGTH,hash);}

    hash_s peer_cand;
    message_phash(peer_cand.hash,svid_msha);
    // create new map and
    char hash1[2*SHA256_DIGEST_LENGTH];
    char hash2[2*SHA256_DIGEST_LENGTH];
    ed25519_key2text(hash1,last_message_hash,SHA256_DIGEST_LENGTH);
    ed25519_key2text(hash2,peer_cand.hash,SHA256_DIGEST_LENGTH);
    fprintf(stderr,"HASH check:\n  %.*s vs\n  %.*s\n",2*SHA256_DIGEST_LENGTH,hash1,2*SHA256_DIGEST_LENGTH,hash2);
    if(memcmp(last_message_hash,peer_cand.hash,SHA256_DIGEST_LENGTH)){
      std::cerr << "ERROR read_peer_missing_hashes\n";
      server_.leave(shared_from_this());
      return;}
    std::cerr << "OK peer " << std::to_string(svid) << " in sync\n";

    //TODO, store last_message_hash in list of message_hashes;
    candidate_ptr c_ptr(new candidate(svid_miss,svid_have,svid,failed));
    server_.save_candidate(peer_cand,c_ptr);
    peer_block_finish(error);
  }

  void peer_block_finish(const boost::system::error_code& error)
  { if(error){
      std::cerr << "ERROR peer_block_finish error\n";
      server_.leave(shared_from_this());
      return;}
    std::cerr << "peer_block_finish\n";
    mtx_.lock();
    if(BLOCK_MODE_SERVER<3){
      BLOCK_MODE_SERVER=3;
      mtx_.unlock();
      return;}
    std::cerr << "peer_block_finish run\n";
    //write_msgs_.pop_front();
    write_msgs_.clear();
    for(auto me=wait_msgs_.begin();me!=wait_msgs_.end();me++){
      write_msgs_.push_back(*me);}
    wait_msgs_.clear();
    BLOCK_MODE_SERVER=0;
    BLOCK_MODE_PEER=0;
    if(!write_msgs_.empty()){
      int len=message_len(write_msgs_.front());
      boost::asio::async_write(socket_,boost::asio::buffer(write_msgs_.front()->data,len),
        boost::bind(&peer::handle_write,shared_from_this(),boost::asio::placeholders::error)); }
    read_msg_ = boost::make_shared<message>(); // continue with a fresh message container
    boost::asio::async_read(socket_,
      boost::asio::buffer(read_msg_->data,message::header_length),
      boost::bind(&peer::handle_read_header,shared_from_this(),boost::asio::placeholders::error));
    mtx_.unlock();
  }

  int parse_vote() //TODO, make this function similar to handle_read_block (make this handle_read_candidate)
  { if((read_msg_->msid!=server_.last_srvs_.now && read_msg_->msid!=srvs_.now) || read_msg_->now<read_msg_->msid || read_msg_->now>=read_msg_->msid+2*BLOCKSEC){
      fprintf(stderr,"BLOCK TIME error now:%08X msid:%08X block[-1].now:%08X block[].now:%08X \n",read_msg_->now,read_msg_->msid,server_.last_srvs_.now,srvs_.now);
      return(0);}
    hash_s cand;
    memcpy(cand.hash,read_msg_->data+message::data_offset,sizeof(hash_t));
    candidate_ptr c_ptr=server_.known_candidate(cand,svid);
    char hash[2*SHA256_DIGEST_LENGTH];
    ed25519_key2text(hash,cand.hash,SHA256_DIGEST_LENGTH);
    fprintf(stderr,"CAND %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    if(c_ptr==NULL){
      std::cerr << "PARSE --NEW-- vote for --NEW-- candidate\n";
      // check message hash
      if(read_msg_->len<message::data_offset+sizeof(hash_t)+2){
        std::cerr << "PARSE vote short len " << read_msg_->len << " FATAL\n";
        return(0);}
      uint16_t changed;
      memcpy(&changed,read_msg_->data+message::data_offset+sizeof(hash_t),2);
      if(!changed){
        std::cerr << "PARSE vote empty change list FATAL\n";
        return(0);}
      if(read_msg_->len!=message::data_offset+sizeof(hash_t)+2+(changed*(2+4+sizeof(hash_t)))){
        std::cerr << "PARSE vote bad len " << read_msg_->len << " FATAL\n";
        return(0);}
      std::map<uint16_t,msidhash_t> new_svid_msha(svid_msha);
      std::map<uint16_t,msidhash_t> new_svid_miss(svid_miss);
      std::map<uint16_t,msidhash_t> new_svid_have(svid_have);
      uint8_t* d=read_msg_->data+message::data_offset+sizeof(hash_t)+2;
      for(int i=0;i<changed;i++,d+=2+4+sizeof(hash_t)){
        uint16_t psvid;
        msidhash_t msha;
        memcpy(&psvid,d,2);
        memcpy(&msha.msid,d+2,4);
        memcpy(&msha.sigh,d+6,sizeof(hash_t));
        if(psvid>=srvs_.nodes.size()){
          std::cerr << "PARSE bad psvid " << psvid << " FATAL\n";
          return(0);}
        new_svid_msha[psvid]=msha;
        new_svid_miss.erase(psvid);
        new_svid_have.erase(psvid);
        if(!msha.msid){
          new_svid_miss[psvid]=msha;
          continue;}
        // check status of message
        auto me=server_.last_svid_msgs.find(psvid);
        if(me!=server_.last_svid_msgs.end() && me->second->msid==msha.msid){ 
          if(memcmp(msha.sigh,me->second->sigh,sizeof(hash_t))){
            char hash1[2*SHA256_DIGEST_LENGTH];
            char hash2[2*SHA256_DIGEST_LENGTH];
            ed25519_key2text(hash1,msha.sigh,SHA256_DIGEST_LENGTH);
            ed25519_key2text(hash2,me->second->sigh,SHA256_DIGEST_LENGTH);
            fprintf(stderr,"HASH mismatch for %d:%d\n  %.*s vs\n  %.*s\n",psvid,msha.msid,2*SHA256_DIGEST_LENGTH,hash1,2*SHA256_DIGEST_LENGTH,hash2);
            return(0);}
          else{ // we have this hash in our final list
            continue;}}
        if(me!=server_.last_svid_msgs.end() && me->second->msid>msha.msid){ // have a newer hash
          new_svid_have[psvid]=msha;}
        else{
          new_svid_miss[psvid]=msha;}}
      bool failed=false;
      for(auto it=new_svid_have.begin();it!=new_svid_have.end();it++){
        message_ptr pm=server_.message_svidmsid(it->first,it->second.msid);
        if(pm==NULL){
          fprintf(stderr,"ERROR failed to find %d:%d\n",it->first,it->second.msid);
          return(0);}
        if(pm->got<srvs_.now+BLOCKSEC-MESSAGE_MAXAGE){ // we will not accept missing this message
          // do not accept candidates with missing: double spend, my messeges, old messages
std::cerr << "TEST THIS !!!!!!!!!!!!\n";
          failed=true;}
        auto sm=server_.last_svid_msgs.find(it->first);
        if(sm==server_.last_svid_msgs.end() || sm->second->msid==0xffffffff || sm->second->svid==opts_.svid){ // we will not accept missing this message
          // do not accept candidates with missing: double spend, my messeges, old messages
          failed=true;}}
      // calculate the hash, TODO TODO, use peer as reference !!!
      hash_t tmp_hash;
      message_phash(tmp_hash,new_svid_msha);
      // create new map and
      if(memcmp(cand.hash,tmp_hash,SHA256_DIGEST_LENGTH)){
        std::cerr << "ERROR parsing hash from peer "<< svid <<"\n";
        return(0);}
      std::cerr << "OK candidate from peer "<< svid <<"\n";
      candidate_ptr n_ptr(new candidate(new_svid_miss,new_svid_have,svid,failed));
      c_ptr=n_ptr;
      server_.save_candidate(cand,c_ptr);}
    else{
      std::cerr << "PARSE vote for known candidate\n";}
    //modify tail from message
    uint16_t changed=c_ptr->svid_miss.size()+c_ptr->svid_have.size();
    if(changed){
      read_msg_->len=message::data_offset+sizeof(hash_t)+2+(changed*(2+4+sizeof(hash_t)));
      read_msg_->data=(uint8_t*)realloc(read_msg_->data,read_msg_->len); // throw if no RAM ???
      memcpy(read_msg_->data+message::data_offset+sizeof(hash_t),&changed,2);
      uint8_t* d=read_msg_->data+message::data_offset+sizeof(hash_t)+2;
      for(auto it=c_ptr->svid_miss.begin();it!=c_ptr->svid_miss.end();it++,d+=2+4+sizeof(hash_t)){
        memcpy(d,&it->first,2);
        memcpy(d+2,&it->second.msid,4);
        memcpy(d+6,&it->second.sigh,sizeof(hash_t));}
      for(auto it=c_ptr->svid_have.begin();it!=c_ptr->svid_have.end();it++,d+=2+4+sizeof(hash_t)){
        memcpy(d,&it->first,2);
        memcpy(d+2,&it->second.msid,4);
        memcpy(d+6,&it->second.sigh,sizeof(hash_t));}
      assert(d-read_msg_->data==read_msg_->len);}
    else{
      read_msg_->len=message::data_offset+sizeof(hash_t);
      read_msg_->data=(uint8_t*)realloc(read_msg_->data,read_msg_->len);}
    return(1);
  }

  uint32_t svid;
private:
  boost::asio::ip::tcp::socket socket_;
  server& server_;
  bool incoming_;
  servers& srvs_; //FIXME ==server_.srvs_
  options& opts_; //FIXME ==server_.opts_

  // data from peer
  std::string addr;
  uint32_t port; // not needed
  uint32_t msid;

  uint32_t peer_path; //used to load data when syncing
  int do_sync;

  handshake_t sync_hs;
  handshake_t peer_hs;
  svsi_t* peer_svsi;
  node_t* peer_nods;
  //handshake_t handshake; // includes the header now
  //header_t sync_head;
  //int sync_nok;
  //int sync_nno;
  //int sync_vok;

  message_ptr read_msg_;
  message_queue write_msgs_;
  message_queue wait_msgs_;
  boost::mutex mtx_;
  // statistics
  uint32_t files_out;
  uint32_t files_in;
  uint64_t bytes_out;
  uint64_t bytes_in;
  //uint32_t ipv4; // not needed
  //uint32_t srvn;
  //uint8_t oldhash[SHA256_DIGEST_LENGTH]; //used in authentication
  uint8_t last_message_hash[SHA256_DIGEST_LENGTH]; //used in block building
  // block hash of messages
  uint16_t peer_missed;
  std::vector<svidmsid_t> svid_msid_server_missing;
  std::vector<svidmsid_t> svid_msid_peer_missing;
  std::map<uint16_t,uint32_t> svid_msid_new; // highest msid for each svid known to peer
  std::map<uint16_t,msidhash_t> svid_msha; // peer last hash status
  std::map<uint16_t,msidhash_t> svid_miss; // from peer (missed by server)
  std::map<uint16_t,msidhash_t> svid_have; // missed by peer
  uint8_t BLOCK_MODE_SERVER;
  uint8_t BLOCK_MODE_PEER;
};

#endif // PEER_HPP
