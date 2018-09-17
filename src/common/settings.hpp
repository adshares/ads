#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <fstream>
#include <iostream>
#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include "openssl/sha.h"
#include "ed25519/ed25519.h"
#include "default.hpp"
#include "helper/ascii.h"
#include "helper/json.h"
#include "utils/os_utils.h"

using namespace Helper;


class settings {
  public:
    settings() :
        ha{{
            0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
            0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}},
         port(0),
         bank(0),
         user(0),
         msid(0),
         nice(true),
         olog(true),
         drun(false),
         sk{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
         pk{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
         //sn{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
         //pn{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    {}
    //uint8_t ha[SHA256_DIGEST_LENGTH];
    std::array<uint8_t, SHA256_DIGEST_LENGTH> ha;
    int port;		// connecting port
    std::string host;	// connecting host
    std::string addr;	// my address
    uint16_t bank;		// my bank
    uint32_t user;		// my account
    int msid;		// my last message id
    bool nice;
    bool olog;
    bool drun;
    bool signature_provided = false;
    bool without_secret = true;
    ed25519_secret_key sk;
    ed25519_public_key pk;	// calculated
//	ed25519_secret_key sn;
//	ed25519_public_key pn;	// calculated
//	ed25519_secret_key so;
//	ed25519_public_key po;	// calculated
    std::string skey;
    std::string pkey;
//	std::string snew;
//	std::string sold;
    std::string hash;	// my last message hash
    uint32_t lastlog;	// placeholder for last requested log entry
    std::string workdir; // my last message hash

    uint16_t crc16(const uint8_t* data_p, uint8_t length) {
        uint8_t x;
        uint16_t crc = 0x1D0F; //differet initial checksum !!!

        while(length--) {
            x = crc >> 8 ^ *data_p++;
            x ^= x>>4;
            crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
        }
        return crc;
    }

    uint16_t crc_acnt(uint16_t to_bank,uint32_t to_user) {
        uint8_t data[6];
        uint8_t* bankp=(uint8_t*)&to_bank;
        uint8_t* userp=(uint8_t*)&to_user;
        //change endian
        data[0]=bankp[1];
        data[1]=bankp[0];
        data[2]=userp[3];
        data[3]=userp[2];
        data[4]=userp[1];
        data[5]=userp[0];
        return(crc16(data,6));
    }

    bool parse_acnt(uint16_t& to_bank,uint32_t& to_user,std::string str_acnt) {
        uint16_t to_csum=0;
        uint16_t to_crc16;
        char *endptr;
        if(str_acnt.length()!=18) {
            fprintf(stderr,"ERROR: parse_acnt(%s) bad length (required 18)\n",str_acnt.c_str());
            return(false);
        }
        if(str_acnt[4]!='-' || str_acnt[13]!='-') {
            fprintf(stderr,"ERROR: parse_acnt(%s) bad format (required NNNN-UUUUUUUU-XXXX)\n",str_acnt.c_str());
            return(false);
        }
        str_acnt[4]='\0';
        str_acnt[13]='\0';
        errno=0;

        char acnt[19];
        memcpy(acnt, str_acnt.c_str(), sizeof(acnt));
        to_bank=(uint16_t)strtol(acnt,&endptr,16);
        if(errno || endptr!=acnt+4) {
            fprintf(stderr,"ERROR: parse_acnt(%s) bad bank\n",acnt);
            perror("ERROR: strtol");
            return(false);
        }
        errno=0;
        to_user=(uint32_t)strtoll(acnt+5,&endptr,16);
        if(errno || endptr!=acnt+13) {
            fprintf(stderr,"ERROR: parse_acnt(%s) bad user\n",acnt+5);
            perror("ERROR: strtol");
            return(false);
        }
        if(!strncmp("XXXX",acnt+14,4)) {
            return(true);
        }
        errno=0;
        to_csum=(uint16_t)strtol(acnt+14,&endptr,16);
        if(errno || endptr!=acnt+18) {
            fprintf(stderr,"ERROR: parse_acnt(%s) bad checksum\n",acnt+14);
            perror("ERROR: strtol");
            return(false);
        }
        to_crc16=crc_acnt(to_bank,to_user);
        if(to_csum!=to_crc16) {
            fprintf(stderr,"ERROR: parse_acnt(%s) bad checksum (expected %04X)\n",acnt,to_crc16);
            return(false);
        }
        return(true);
    }

    bool parse_txid(uint16_t& to_bank,uint32_t& node_msid,uint32_t& node_mpos,std::string str_txid) {
        char *endptr;
        if(str_txid.length()!=18) {
            fprintf(stderr,"ERROR: parse_txid(%s) bad length (required 18)\n",str_txid.c_str());
            return(false);
        }
        if(str_txid[4]!=':' || str_txid[13]!=':') {
            fprintf(stderr,"ERROR: parse_txid(%s) bad format (required NNNN:MMMMMMMM:PPPP)\n",str_txid.c_str());
            return(false);
        }
        str_txid[4]='\0';
        str_txid[13]='\0';

        char acnt[19];
        memcpy(acnt, str_txid.c_str(), sizeof(acnt));

        errno=0;
        to_bank=(uint16_t)strtol(acnt,&endptr,16);
        if(errno || endptr!=acnt+4) {
            fprintf(stderr,"ERROR: parse_txid(%s) bad bank\n",acnt);
            perror("ERROR: strtol");
            return(false);
        }
        errno=0;
        node_msid=(uint32_t)strtoll(acnt+5,&endptr,16);
        if(errno || endptr!=acnt+5+8) {
            fprintf(stderr,"ERROR: parse_txid(%s) bad msid\n",acnt+5);
            perror("ERROR: strtol");
            return(false);
        }
        errno=0;
        node_mpos=(uint16_t)strtol(acnt+14,&endptr,16);
        if(errno || endptr!=acnt+14+4) {
            fprintf(stderr,"ERROR: parse_txid(%s) bad mpos\n",acnt+14);
            perror("ERROR: strtol");
            return(false);
        }
        return(true);
    }

    bool parse_msgid(uint16_t& to_bank,uint32_t& node_msid,std::string str_txid) {
        char *endptr;
        if(str_txid.length()!=13) {
            fprintf(stderr,"ERROR: parse_msgid(%s) bad length (required 12)\n",str_txid.c_str());
            return(false);
        }
        if(str_txid[4]!=':') {
            fprintf(stderr,"ERROR: parse_msgid(%s) bad format (required NNNN:MMMMMMMM)\n",str_txid.c_str());
            return(false);
        }
        str_txid[4]='\0';
        char acnt[14];
        memcpy(acnt, str_txid.c_str(), sizeof(acnt));
        errno=0;
        to_bank=(uint16_t)strtol(acnt,&endptr,16);
        if(errno || endptr!=acnt+4) {
            fprintf(stderr,"ERROR: parse_msgid(%s) bad bank\n",acnt);
            perror("ERROR: strtol");
            return(false);
        }
        errno=0;
        node_msid=(uint32_t)strtoll(acnt+5,&endptr,16);
        if(errno || endptr!=acnt+5+8) {
            fprintf(stderr,"ERROR: parse_msgid(%s) bad msid\n",acnt+5);
            perror("ERROR: strtol");
            return(false);
        }

        return(true);
    }

    static void print_version() {
        std::cerr << "Version " << get_version() << std::endl;
    }

    static std::string get_version(uint max_length=0) {
        std::stringstream ss;
        if(strlen(PROJECT_VERSION) == 0) {
          ss << GIT_BRANCH << "@" << GIT_COMMIT_HASH;
        } else {
          ss << PROJECT_VERSION;
        }
#ifdef DEBUG
        ss << "x";
#endif
        std::string version = ss.str();
        if(max_length && version.length() > max_length) {
            return version.substr(version.length() - max_length);
        } else {
            return version;
        }
    }

    std::string get_password(char print_char = 0) {
        std::string pass;
        char ch;
        while (1) {
            ch = utils::getch();
            if (ch == '\n') break;
            std::cout<<print_char;
            pass.push_back(ch);
        }
        std::cout<<std::endl;
        return pass;
    }

    static void change_working_dir(std::string workdir) {
        auto i = workdir.find("$HOME");
        if(i != std::string::npos) {
            char * home = std::getenv("HOME");
            if(home) {
                workdir.replace(i, strlen("$HOME"), home);
            } else {
                std::cerr << "Cannot find $HOME\n";
                exit(-1);
            }
        }
        boost::filesystem::create_directories(workdir);
        if(chdir(workdir.c_str())) {
            std::cerr << "Could not chdir to working directory\n";
            exit(-1);
        }
        std::cerr << "Working dir: " << workdir << "\n";
    }

    static std::string get_workdir(int ac, char *av[], bool server) {
      boost::program_options::options_description config;

      config.add_options()
          ("work-dir,w", boost::program_options::value<std::string>()->default_value(std::string("$HOME/.") + std::string(PROJECT_NAME) + (server ? "d" : "")),    "working directory");

      boost::program_options::options_description cmdline_options;
      cmdline_options.add(config);
      boost::program_options::variables_map vm;
      store(boost::program_options::command_line_parser(ac, av).options(cmdline_options).allow_unregistered().run(), vm);
      if(vm.count("work-dir")) {
        return vm["work-dir"].as<std::string>();
      }
      return nullptr;
    }

    void get(int ac, char *av[]) {
        try {
            boost::program_options::options_description generic("Generic options");
            generic.add_options()
            ("work-dir,w", boost::program_options::value<std::string>(&workdir)->default_value(std::string("$HOME/.") + std::string(PROJECT_NAME)),    "working directory")
            ("version,v", "print version string")
            ("help,h", "produce help message")
            ;
            boost::program_options::options_description config("Configuration [command_line + config_file]");
            config.add_options()
            ("port,P", boost::program_options::value<int>(&port)->default_value(std::atoi(OFFICE_PORT)),	"node port (for clients)")
            ("host,H", boost::program_options::value<std::string>(&host)->default_value("127.0.0.1"),	"node hostname or ip")
            ("address,A", boost::program_options::value<std::string>(&addr),				"address (don't use with --bank, --user)")
            ("bank,b", boost::program_options::value<uint16_t>(&bank),					"node id (don't use with --address)")
            ("user,u", boost::program_options::value<uint32_t>(&user),					"user id (don't use with --address)")
            ("msid,i", boost::program_options::value<int>(&msid)->default_value(0),				"last message id")
            ("nice,n", boost::program_options::value<bool>(&nice)->default_value(true),			"request pretty json")
            ("olog,o", boost::program_options::value<bool>(&olog)->default_value(true),			"record submitted transactions in log file")
            ("dry-run,d", boost::program_options::value<bool>(&drun)->default_value(false),			"dry run (do not submit to network)")
            ("hash,x", boost::program_options::value<std::string>(&hash),					"last hash [64chars in hex format / 32bytes]")
            ("secret,s", boost::program_options::value<std::string>(&skey)->implicit_value("-"),         "passphrase or private key [64chars in hex format / 32bytes]")
            ;
            std::string positional_help;
            boost::program_options::options_description positionals("Positional options");
            positionals.add_options()("pos_help", boost::program_options::value<std::string>(&positional_help), "Print all possible commands");

            boost::program_options::positional_options_description positional_options;
            positional_options.add("pos_help", -1);

            boost::program_options::options_description cmdline_options;
            cmdline_options.add(generic).add(config).add(positionals);
            boost::program_options::options_description config_file_options;
            config_file_options.add(config);
            boost::program_options::variables_map vm;
            store(boost::program_options::command_line_parser(ac, av).options(cmdline_options).positional(positional_options).run(), vm);
            std::ifstream ifs("settings.cfg");
            store(parse_config_file(ifs, config_file_options), vm);
            notify(vm);
            if (!positional_help.empty()) {
                if (positional_help == "help") {
                    Helper::print_all_commands_help();
                    exit(0);
                }
                // else go to help
                vm.insert(std::make_pair("help", boost::program_options::variable_value()));
            }
            if(vm.count("help")) {
                std::cerr << "Usage: \t" << PROJECT_NAME << " [settings]\n";
                std::cerr <<  "\t"<< PROJECT_NAME <<" [help]\t\t print all possible commands "<< "\n";
                std::cerr << generic;

                std::cerr << config << "\n";

                print_version();
                exit(0);
            }
            if(vm.count("version")) {
                print_version();
                exit(0);
            }

            if(vm.count("secret")) {
                std::string line;
                line = vm["secret"].as<std::string>();
                if (line == "-") {
                    std::cerr << "ENTER passphrase or secret key\n";
                    line = get_password('*');
                    boost::trim_right_if(line,boost::is_any_of(" \r\n\t"));
                    if(line.empty()) {
                        std::cerr << "ERROR, failed to read passphrase\n";
                        exit(1);
                    }
                }
                char pktext[2*32+1];
                pktext[2*32]='\0';
                if(line.length() == 64 && line.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                    skey = line;
                    ed25519_text2key(sk,skey.c_str(),32);
                    ed25519_publickey(sk,pk);
                    ed25519_key2text(pktext,pk,32);
                } else {
                    SHA256_CTX sha256;
                    SHA256_Init(&sha256);
                    SHA256_Update(&sha256,line.c_str(),line.length());
                    SHA256_Final(sk,&sha256);
                    ed25519_publickey(sk,pk);
                    ed25519_key2text(pktext,pk,32);
                }
                std::cerr << "Public key: " << std::string(pktext) << std::endl;
                without_secret = false;
            }

            if(vm.count("port")) {
                std::cerr << "Node port: " << vm["port"].as<int>() << std::endl;
            } else {
                std::cerr << "ERROR: node port missing!" << std::endl;
                exit(1);
            }
            if (vm.count("host")) {
                std::cerr << "Node host: " << vm["host"].as<std::string>() << std::endl;
            } else {
                std::cerr << "ERROR: node host missing!" << std::endl;
                exit(1);
            }
            if (vm.count("address")) {
                if(vm.count("bank") || vm.count("user")) {
                    std::cerr << "ERROR: do not specify bank/user and address at the same time" << std::endl;
                    exit(1);
                }
                std::cerr << "Address: " << vm["address"].as<std::string>() << std::endl;
                if(!parse_acnt(bank, user, addr)) {
                    std::cerr << "ERROR: invalid address" << std::endl;
                    exit(1);
                }
                vm.insert(std::make_pair("bank", boost::program_options::variable_value(bank, false)));
                vm.insert(std::make_pair("user", boost::program_options::variable_value(user, false)));
            }
            if (vm.count("bank")) {
                std::cerr << "Node   id: " << vm["bank"].as<uint16_t>() << std::endl;
            }
            //else{
            //	std::cerr << "ERROR: bank id missing! Specify --bank or --address" << std::endl;
            //	exit(1);}
            if (vm.count("user")) {
                std::cerr << "User   id: " << vm["user"].as<uint32_t>() << std::endl;
            }
            //else{
            //	std::cerr << "ERROR: user id missing! Specify --user or --address" << std::endl;
            //	exit(1);}
            if (vm.count("msid")) {
                std::cerr << "LastMsgId: " << vm["msid"].as<int>() << std::endl;
            } else {
                if(vm.count("user")) {
                    std::cerr << "WARNING: last message id missing!" << std::endl;
                }
            }
            if (vm.count("nice")) {
                std::cerr << "PrettyOut: " << vm["nice"].as<bool>() << std::endl;
            }
            if (vm.count("olog")) {
                std::cerr << "LogOutput: " << vm["olog"].as<bool>() << std::endl;
            }
            if (vm.count("dry-run")) {
                std::cerr << "Dry Run  : " << vm["dry-run"].as<bool>() << std::endl;
            }
            if (vm.count("hash")) {
                if(hash.length()!=64) {
                    std::cerr << "ERROR: hash wrong length (should be 64): " << vm["hash"].as<std::string>() << std::endl;
                    exit(1);
                }
                ed25519_text2key(ha.data(), hash.c_str(), 32);
            } else {
                if(vm.count("user")) {
                    std::cerr << "WARNING: last hash missing!" << std::endl;
                }
            }
        }

        catch(std::exception &e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            exit(1);
        }
    }
};

#endif // SETTINGS_HPP
