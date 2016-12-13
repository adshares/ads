#include "main.hpp"

void iorun(boost::asio::io_service& io_service)
{ //while(1){
    //if(io_service.stopped()){
    //  std::cerr << "Service.Run exit\n"; //Now we know the server is down.
    //  return;}
    try{
      io_service.run();
      std::cerr << "Service.Run finished\n";} //Now we know the server is down.
    catch (std::exception& e){
      std::cerr << "Service.Run: " << e.what() << "\n";}
  //  sleep(1);}// just in case there is something wrong with 'run'
}

int main(int argc, char* argv[])
{
  options opt;
  opt.get(argc,argv);
  try{
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(),opt.port);
    boost::asio::io_service io_service;
    boost::asio::io_service::work work(io_service); //to start io_service before the server
    boost::thread t(iorun,boost::ref(io_service));
    server s(io_service,endpoint,opt);
    //for (std::string p : opt.peer) {
    //  s.connect(p); }
    std::string line;
    while (std::getline(std::cin,line)){
      if(line[0]=='.' && line[1]=='\0'){
        break;}
      s.write_message(line);}
    std::cerr << "out\n";
    io_service.stop();
    t.join();}
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";}
  return 0;
}
