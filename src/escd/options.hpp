#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <boost/program_options.hpp>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include "default.hpp"
#include "settings.hpp"

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    copy(v.begin(), v.end(), std::ostream_iterator<T>(std::cout, " "));
    return os;
}

class options {
  public:
    options() :
        init(false),
        fast(false),
        comm(false),
        ipv4(0) {
    }
    bool init;
    bool fast;
    bool comm;
    int mins;
    int offi;
    int port;
    int svid;
    std::string addr;
    std::string dnsa;
    std::string genesis;
    std::string viphash;
    uint32_t ipv4;
    int back;
    std::vector<std::string> peer;
    std::string workdir;
    int log_level = (int)logging::LoggingLevel::LOG_INFO;
    int log_source = (int)logging::LoggingSource::LOG_ALL;


    std::vector<std::string> allow_from;

    std::vector<std::string> redirect_read;
    std::vector<std::string> redirect_read_exclude;
    std::vector<std::string> redirect_write;
    std::vector<std::string> redirect_write_exclude;


    void print_version() {
        std::string version = PROJECT_VERSION;
        std::cerr << "Version ";
        if(version.empty()) {
          std::cerr << GIT_BRANCH << "@" << GIT_COMMIT_HASH << "\n";
        } else {
          std::cerr << PROJECT_VERSION << "\n";
        }
    }

    void get(int ac, char *av[]) {
        try {
            boost::program_options::options_description generic("Generic options");
            generic.add_options()
            ("work-dir,w", boost::program_options::value<std::string>(&workdir)->default_value(std::string("$HOME/.") + std::string(PROJECT_NAME) + std::string("d")),    "working directory")
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
            ("dnsa,d", boost::program_options::value<std::string>(&dnsa)->default_value(SERVER_DNSA),	"host name of ESC nodes")
            ("peer,r", boost::program_options::value<std::vector<std::string>>(&peer)->composing(),		"peer address:port/id, multiple peers allowed, id as int")
            ("back,b", boost::program_options::value<int>(&back)->default_value(0),				"roll back database given number of blocks (reversible if no commit)")
            ("comm,c", boost::program_options::value<bool>(&comm)->default_value(0),			"commit database roll back database (irreversible!) and proceed")
            ("viphash,V", boost::program_options::value<std::string>(&viphash)->default_value(""),      "current viphash of desired network (required with --fast switch)")
            ("genesis,g", boost::program_options::value<std::string>(&genesis)->default_value(""),      "json file with network state at genesis block (works with --init)")
            ("log_level", boost::program_options::value<int>(&log_level)->default_value(logging::LoggingLevel::LOG_INFO), "collecting logs level:\n0 - trace (all)\n1 - debug\n2 - info\n3 - warning\n4 - error\n5 - fatal (only)")
            ("log_source", boost::program_options::value<int>(&log_source)->default_value(logging::LoggingSource::LOG_ALL), "logs output sources:\n0 - none\n1 - only console\n2 - only file\n3 - file and console")
            ("allow-from", boost::program_options::value<std::vector<std::string>>(&allow_from)->composing(), "ip/host (allow only clients from specified IPs)")
            ("redirect-read", boost::program_options::value<std::vector<std::string>>(&redirect_read)->composing(), "host:port (redirect reading clients to another endpoint)")
            ("redirect-read-exclude", boost::program_options::value<std::vector<std::string>>(&redirect_read_exclude)->composing(), "ip/host (do not redirect clients from specified IPs)")
            ("redirect-write", boost::program_options::value<std::vector<std::string>>(&redirect_write)->composing(), "host:port (redirect writing clients to another endpoint)")
            ("redirect-write-exclude", boost::program_options::value<std::vector<std::string>>(&redirect_write_exclude)->composing(), "ip/host (do not redirect clients from specified IPs)")
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
            if(vm.count("help")) {
                std::cout << "Usage: " << PROJECT_NAME << "d [options]\n";
                std::cout << generic << "\n";
                std::cout << config << "\n";
                settings::print_version();
                exit(0);
            }
            if(vm.count("version")) {
                settings::print_version();
                exit(0);
            }
            if(vm.count("init")) {
                std::cout << "Service init: " << vm["init"].as<bool>() << std::endl;
            }
            if(vm.count("fast")) {
                std::cout << "Service fast: " << vm["fast"].as<bool>() << std::endl;
                /*if(vm["fast"].as<bool>()) {
                    if(!vm.count("viphash") || vm["viphash"].as<std::string>().length() == 0) {
                        std::cerr << "Must provide --viphash for fast sync" << std::endl;
                        throw std::runtime_error("Must provide --viphash for fast sync");
                    } else if(vm["viphash"].as<std::string>().length() != SHA256_DIGEST_LENGTH*2) {
                        std::cerr << "Invalid --viphash length" << std::endl;
                        throw std::runtime_error("Invalid --viphash length");
                    }
                }*/
            }
            if(vm.count("mins")) {
                std::cout << "Service mins: " << vm["mins"].as<int>() << std::endl;
            }
            if(vm.count("offi")) {
                std::cout << "Service offi: " << vm["offi"].as<int>() << std::endl;
            }
            if(vm.count("port")) {
                std::cout << "Service port: " << vm["port"].as<int>() << std::endl;
            }
            if (vm.count("addr")) {
                std::cout << "Service addr: " << vm["addr"].as<std::string>() << std::endl;
                struct in_addr adds;
                if(inet_aton(addr.c_str(),&adds)) { //FIXME, check if this accepts "localhost"
                    ipv4=adds.s_addr;
                } else {
                    std::cout << "Service addr: ERROR parsing my ip" << std::endl;
                }
            }
            if (vm.count("svid")) {
                std::cout << "Service svid: " << vm["svid"].as<int>() << std::endl;
            } else {
                if(!init) {
                    std::cout << "Service svid: 0 (read only)" << std::endl;
                    svid=0;
                } else {
                    std::cout << "Service svid: 1 (init node)" << std::endl;
                    svid=1;
                }
            }
            if (vm.count("dnsa")) {
                std::cout << "ESC nodes: " << vm["dnsa"].as<std::string>() << std::endl;
            }
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
            if (vm.count("peer")) {
                std::cout << "Peers are: " << vm["peer"].as<std::vector<std::string>>() << std::endl;
            }
            if(vm.count("back")) {
                std::cout << "Roll back DB: " << vm["back"].as<int>() << std::endl;
            }
            if(vm.count("back") && vm.count("comm")) {
                std::cout << "Commit DB   : " << vm["comm"].as<bool>() << std::endl;
            }
            logging::set_level((logging::LoggingLevel)log_level);
            logging::set_log_source((logging::LoggingSource)log_source);
        } catch(std::exception &e) {
            std::cout << "Exception: " << e.what() << std::endl;
            exit(1);
        }
    }

    uint16_t get_svid(std::string peer_address) { // get svid from peer address
        char* p=strchr((char*)peer_address.c_str(),'/'); //detect port
        if(p==NULL) {
            return(0);
        }
        return((uint16_t)atoi(p+1));
    }

    void get_address(const std::string& address, std::string& ip, std::string& port, std::string& svid)
    { // get port from peer address
        const size_t posPort    = address.find(':');
        const size_t posId      = address.find('/');

        ip      = address.substr(0, posPort);
        port    = address.substr(posPort+1, posId - posPort -1);
        svid    = address.substr(posId+1);
    }

};

#endif // OPTIONS_HPP
