

  void store_final_msgl( // remember, validators are running !!!

      for(auto jt=winner->msg_mis.begin();jt!=winner->msg_mis.end();jt++){
        it=jt++;
        txs_.lock();
        auto me=txs_msgs_.lower_bound(*it);
        while(me!=txs_msgs_.end()){
          if((me->first & 0xFFFFFFFFFFFF0000L)!=(*it)){
            break;}
          if(!(me->second.status & MSGSTAT_DAT)){
            me++;
            continue;}
          if(memcmp(me->second.sigh,winner->msg_add[*it].hash,32))
            LOG("WARNING moving message %04X:%08X to failed messages\n",me->second.svid,me->second.msid);
            auto oe=me++;
            bad_insert(oe->second);
            txs_msgs_.erase(oe);
            continue;}
          //this message is ok we should have found it before, maybe it is not yet valid
          if(me->second.status & MSGSTAT_VAL){
            LOG("WARNING found missing message %04X:%08X in map :-(\n",me->second.svid,me->second.msid);
            winner->msg_mis.erase(it);
            break;}
          break;}

...

        if(it->second.msid==0x00000000){ // should never happen
          continue;}
        if(it->second.msid==0xFFFFFFFF){ // should never happen
          winner->waiting_server.erase(it->first);
          continue;}
        if(winner->waiting_server.find(it->first)!=winner->waiting_server.end()){
          message_ptr pm=message_svidmsid(it->first,it->second.msid);
          if(pm!=NULL && !memcmp(pm->sigh,it->second.sigh,sizeof(hash_t))){
            LOG("WARNING, found missing message after election ended :-(\n");
            winner->waiting_server.erase(it->first);}}}


update_list ???
