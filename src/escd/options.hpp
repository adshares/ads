#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <boost/program_options.hpp>
#include <arpa/inet.h>

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(std::cout, " "));
    return os;
}

class options
{
public:
        options() :
                init(false),
                fast(false),
                ipv4(0)
        {}
        bool init;
        bool fast;
        int mins;
        int offi;
        int port;
        int svid;
        std::string addr;
        std::string dnsa;
        uint32_t ipv4;
        int back;
        std::vector<std::string> peer;

        void get(int ac, char *av[])
        {	 try{
                        boost::program_options::options_description generic("Generic options");
                        generic.add_options()
                                ("version,v", "print version string")
                                ("help,h", "produce help message")
                                ;
                        boost::program_options::options_description config("Configuration [command_line + config_file]");
                        config.add_options()
                                ("init,n", boost::program_options::value<bool>(&init)->default_value(0),			"start new chain")
                                ("fast,f", boost::program_options::value<bool>(&fast)->default_value(0),			"fast sync without history")
                                ("mins,m", boost::program_options::value<int>(&mins)->default_value(VIP_MAX/2),			"minimum number of offered block signatures to start sync (0:max/2)")
                                ("offi,o", boost::program_options::value<int>(&offi)->default_value(std::atoi(OFFICE_PORT)),	"office port (for clients)")
                                ("port,p", boost::program_options::value<int>(&port)->default_value(std::atoi(SERVER_PORT)),	"service port (for peers)")
                                ("addr,a", boost::program_options::value<std::string>(&addr)->default_value("127.0.0.1"),	"service address or hostname")
                                ("svid,i", boost::program_options::value<int>(&svid)->default_value(0),				"service id (assigned by the network)")
                                ("dnsa,d", boost::program_options::value<std::string>(&dnsa)->default_value(SERVER_DNSA),	"host name of adshares nodes")
                                ("peer,r", boost::program_options::value<std::vector<std::string>>(&peer)->composing(),		"peer address:port/id, multiple peers allowed, id as int")
                                ("back,b", boost::program_options::value<int>(&back)->default_value(0),				"roll back database given number of blocks (irreversable!)")
                                ;
                        boost::program_options::options_description cmdline_options;
                        cmdline_options.add(generic).add(config);
                        boost::program_options::options_description config_file_options;
                        config_file_options.add(config);
                        boost::program_options::variables_map vm;
                        store(boost::program_options::command_line_parser(ac, av).options(cmdline_options).run(), vm);
                        std::ifstream ifs("options.cfg");
                        store(parse_config_file(ifs, config_file_options), vm);
                        notify(vm);
                        if(vm.count("help")){
                                std::cout << "Usage: " << av[0] << " [options]\n";
                                std::cout << generic << "\n";
                                std::cout << config << "\n";
                                exit(0);}
                        if(vm.count("version")){
                                std::cout << "Version 1.0\n";
                                exit(0);}
                        if(vm.count("init")){
                                std::cout << "Service init: " << vm["init"].as<bool>() << std::endl;}
                        if(vm.count("fast")){
                                std::cout << "Service fast: " << vm["fast"].as<bool>() << std::endl;}
                        if(vm.count("mins")){
                                std::cout << "Service mins: " << vm["mins"].as<int>() << std::endl;}
                        if(vm.count("offi")){
                                std::cout << "Service offi: " << vm["offi"].as<int>() << std::endl;}
                        if(vm.count("port")){
                                std::cout << "Service port: " << vm["port"].as<int>() << std::endl;}
                        if (vm.count("addr")){
                                std::cout << "Service addr: " << vm["addr"].as<std::string>() << std::endl;
                struct in_addr adds;
                                if(inet_aton(addr.c_str(),&adds)){ //FIXME, check if this accepts "localhost"
                                        ipv4=adds.s_addr;}
                                else{
                                        std::cout << "Service addr: ERROR parsing my ip" << std::endl;}}
                        if (vm.count("svid")){
                                std::cout << "Service svid: " << vm["svid"].as<int>() << std::endl;}
                        else{
                                if(!init){
                                        std::cout << "Service svid: 0 (read only)" << std::endl;
                                        svid=0;}
                                else{
                                        std::cout << "Service svid: 1 (init node)" << std::endl;
                                        svid=1;}}
                        if (vm.count("dnsa")){
                                std::cout << "Adshares nodes: " << vm["dnsa"].as<std::string>() << std::endl;}
                        /*if (vm.count("skey")){
                                char pktext[2*32+1]; pktext[2*32]='\0';
                                if(skey.length()!=64){
                                        std::cerr << "ERROR: Service secret key wrong length (should be 64): " << vm["skey"].as<std::string>() << std::endl;
                                        exit(1);}
                                ed25519_text2key(sk,skey.c_str(),32);
                                ed25519_publickey(sk,pk);
                                ed25519_key2text(pktext,pk,32);
                                std::cout << "Service secret key: " << vm["skey"].as<std::string>() << std::endl;
                                std::cout << "Service public key: " << std::string(pktext) << std::endl;}
                        else{
                                std::cout << "ERROR: Service secret key missing!" << std::endl;
                                exit(1);}*/
                        if (vm.count("peer")){
                                std::cout << "Peers are: " << vm["peer"].as<std::vector<std::string>>() << std::endl;}
                        if(vm.count("back")){
                                std::cout << "Roll back DB: " << vm["back"].as<int>() << std::endl;}}
                catch(std::exception &e){
                        std::cout << "Exception: " << e.what() << std::endl;
                        exit(1);
                }
        }

        uint16_t get_svid(std::string peer_address) // get svid from peer address
        {	char* p=strchr((char*)peer_address.c_str(),'/'); //detect port
                if(p==NULL){
                        return(0);}
                return((uint16_t)atoi(p+1));
        }

};

#endif // OPTIONS_HPP
