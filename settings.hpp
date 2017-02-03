#ifndef SETTINGS_HPP
#define SETTINGS_HPP

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(std::cout, " "));
    return os;
}

class settings
{
public:
	settings() :
		ha{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
		port(0),
		bank(0),
		user(0),
		msid(0),
		sk{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		pk{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		sn{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		pn{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
	{}
	uint8_t ha[SHA256_DIGEST_LENGTH];
	int port;		// connecting port
	std::string addr;	// connecting address
	int bank;		// my bank
	int user;		// my account
	int msid;		// my last message id
	ed25519_secret_key sk;
	ed25519_public_key pk;	// calculated
	ed25519_secret_key sn;
	ed25519_public_key pn;	// calculated
	ed25519_secret_key so;
	ed25519_public_key po;	// calculated
	std::string skey;
	std::string pkey;
	std::string snew;
	std::string sold;
	std::string exec;
	std::string hash;	// my last message hash

	void get(int ac, char *av[])
	{	 try{
			char pktext[2*32+1]; pktext[2*32]='\0';
			boost::program_options::options_description generic("Generic options");
			generic.add_options()
				("version,v", "print version string")
				("help,h", "produce help message")
				;
			boost::program_options::options_description config("Configuration [command_line + config_file]");
			config.add_options()
				("port,p", boost::program_options::value<int>(&port)->default_value(9080),			"bank port (for clients)")
				("addr,a", boost::program_options::value<std::string>(&addr)->default_value("127.0.0.1"),	"bank address or hostname")
				("bank,b", boost::program_options::value<int>(&bank),						"bank id")
				("user,u", boost::program_options::value<int>(&user),						"user id")
				("msid,i", boost::program_options::value<int>(&msid),						"last message id")
				("hash,j", boost::program_options::value<std::string>(&hash),					"last hash [64chars in hext format / 32bytes]")
				("skey,s", boost::program_options::value<std::string>(&skey),					"secret key [64chars in hext format / 32bytes]")
				("pkey,k", boost::program_options::value<std::string>(&pkey),					"public key [64chars in hext format / 32bytes]")
				("snew,n", boost::program_options::value<std::string>(&snew),					"new secret key [32bytes]")
				("sold,o", boost::program_options::value<std::string>(&sold),					"old secret key [32bytes]")
				("exec,e", boost::program_options::value<std::string>(&exec),					"command to execute")
				;
			boost::program_options::options_description cmdline_options;
			cmdline_options.add(generic).add(config);
			boost::program_options::options_description config_file_options;
			config_file_options.add(config);
			boost::program_options::variables_map vm;
			store(boost::program_options::command_line_parser(ac, av).options(cmdline_options).run(), vm);
			std::ifstream ifs("settings.cfg");
			store(parse_config_file(ifs, config_file_options), vm);
			notify(vm);
			if(vm.count("help")){
				std::cout << "Usage: " << av[0] << " [settings]\n";
				std::cout << generic << "\n";
				std::cout << config << "\n";
				exit(0);}
			if(vm.count("version")){
				std::cout << "Version 1.0\n";
				exit(0);}
			if (vm.count("skey")){
				if(skey.length()!=64){
					std::cerr << "ERROR: secret key wrong length (should be 64): " << vm["skey"].as<std::string>() << std::endl;
					exit(1);}
				ed25519_text2key(sk,skey.c_str(),32);
				ed25519_publickey(sk,pk);
				ed25519_key2text(pktext,pk,32);
				//std::cout << "secret key: " << vm["skey"].as<std::string>() << std::endl;
				std::cout << "Public key: " << std::string(pktext) << std::endl;}
			else{
				std::cout << "ENTER passphrase\n";
				std::string line;
				std::getline(std::cin,line);
				boost::trim_right_if(line,boost::is_any_of(" \r\n\t"));
				if(line.empty()){
					std::cout << "ERROR, failed to read passphrase\n";
					exit(1);}
				SHA256_CTX sha256;
				SHA256_Init(&sha256);
				SHA256_Update(&sha256,line.c_str(),line.length());
				SHA256_Final(sk,&sha256);
				ed25519_publickey(sk,pk);
				ed25519_key2text(pktext,pk,32);
				//std::cout << "secret key: " << vm["skey"].as<std::string>() << std::endl;
				std::cout << "Public key: " << std::string(pktext) << std::endl;}
			if(vm.count("pkey")){
				if(memcmp(pkey.c_str(),pktext,2*32)){
					std::cout << "Public key: "<<vm["pkey"].as<std::string>()<<" MISMATCH!\n";
					exit(1);}}
			else{
				std::cout << "INFO: suply public key to proceed\n";
				exit(0);}
			if (vm.count("snew")){
				if(snew.length()!=64){
					std::cout << "ENTER passphrase\n";
					std::string line;
					std::getline(std::cin,line);
					boost::trim_right_if(line,boost::is_any_of(" \r\n\t"));
					if(line.empty()){
						std::cout << "ERROR, failed to read passphrase\n";
						exit(1);}
					SHA256_CTX sha256;
					SHA256_Init(&sha256);
					SHA256_Update(&sha256,line.c_str(),line.length());
					SHA256_Final(sn,&sha256);}
				else{
					ed25519_text2key(sn,snew.c_str(),32);}
				ed25519_publickey(sn,pn);
				ed25519_key2text(pktext,pn,32);
				//std::cout << "secret key: " << vm["skey"].as<std::string>() << std::endl;
				std::cout << "New public key: " << std::string(pktext) << std::endl;}
			if (vm.count("sold")){
				if(sold.length()!=64){
					std::cout << "ENTER passphrase\n";
					std::string line;
					std::getline(std::cin,line);
					boost::trim_right_if(line,boost::is_any_of(" \r\n\t"));
					if(line.empty()){
						std::cout << "ERROR, failed to read passphrase\n";
						exit(1);}
					SHA256_CTX sha256;
					SHA256_Init(&sha256);
					SHA256_Update(&sha256,line.c_str(),line.length());
					SHA256_Final(so,&sha256);}
				else{
					ed25519_text2key(so,sold.c_str(),32);}
				ed25519_publickey(so,po);
				ed25519_key2text(pktext,po,32);
				//std::cout << "secret key: " << vm["skey"].as<std::string>() << std::endl;
				std::cout << "old public key: " << std::string(pktext) << std::endl;}
			if(vm.count("port")){
				std::cout << "Bank port: " << vm["port"].as<int>() << std::endl;}
			else{
				std::cout << "ERROR: bank port missing!" << std::endl;
				exit(1);}
			if (vm.count("addr")){
				std::cout << "Bank addr: " << vm["addr"].as<std::string>() << std::endl;}
			else{
				std::cout << "ERROR: bank addr missing!" << std::endl;
				exit(1);}
			if (vm.count("bank")){
				std::cout << "Bank   id: " << vm["bank"].as<int>() << std::endl;}
			else{
				std::cout << "ERROR: bank id missing!" << std::endl;
				exit(1);}
			if (vm.count("user")){
				std::cout << "User   id: " << vm["user"].as<int>() << std::endl;}
			else{
				std::cout << "ERROR: user id missing!" << std::endl;
				exit(1);}
			if (vm.count("msid")){
				std::cout << "LastMsgId: " << vm["msid"].as<int>() << std::endl;}
			else{
				std::cout << "WARNING: last message id missing!" << std::endl;}
			if (vm.count("hash")){
				if(hash.length()!=64){
					std::cerr << "ERROR: hash wrong length (should be 64): " << vm["hash"].as<std::string>() << std::endl;
					exit(1);}
				ed25519_text2key(ha,hash.c_str(),32);}
			else{
				std::cout << "WARNING: last hash missing!" << std::endl;}
			}
		catch(std::exception &e){
			std::cout << "Exception: " << e.what() << std::endl;
			exit(1);
		}
	}
};

#endif // SETTINGS_HPP
