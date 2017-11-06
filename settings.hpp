#ifndef SETTINGS_HPP
#define SETTINGS_HPP

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(std::cerr, " "));
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
		nice(true),
		olog(true),
		drun(false),
		sk{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		pk{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
		//sn{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		//pn{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
	{}
	uint8_t ha[SHA256_DIGEST_LENGTH];
	int port;		// connecting port
	std::string host;	// connecting host
	std::string addr;	// my address
	uint16_t bank;		// my bank
	uint32_t user;		// my account
	int msid;		// my last message id
	bool nice;
	bool olog;
	bool drun;
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

uint16_t crc16(const uint8_t* data_p, uint8_t length)
{ uint8_t x;
  uint16_t crc = 0x1D0F; //differet initial checksum !!!

  while(length--){
    x = crc >> 8 ^ *data_p++;
    x ^= x>>4;
    crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);}
  return crc;
}

uint16_t crc_acnt(uint16_t to_bank,uint32_t to_user)
{ uint8_t data[6];
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

bool parse_acnt(uint16_t& to_bank,uint32_t& to_user,std::string str_acnt)
{ uint16_t to_csum=0;
  uint16_t to_crc16;
  char *endptr;
  if(str_acnt.length()!=18){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad length (required 18)\n",str_acnt.c_str());
    return(false);}
  if(str_acnt[4]!='-' || str_acnt[13]!='-'){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad format (required NNNN-UUUUUUUU-XXXX)\n",str_acnt.c_str());
    return(false);}
  str_acnt[4]='\0';
  str_acnt[13]='\0';
  errno=0;
  to_bank=(uint16_t)strtol(str_acnt.c_str(),&endptr,16);
  if(errno || endptr==str_acnt.c_str()){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad bank\n",str_acnt.c_str());
    perror("ERROR: strtol");
    return(false);}
  errno=0;
  to_user=(uint32_t)strtoll(str_acnt.c_str()+5,&endptr,16);
  if(errno || endptr==str_acnt.c_str()+5){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad user\n",str_acnt.c_str());
    perror("ERROR: strtol");
    return(false);}
  if(!strncmp("XXXX",str_acnt.c_str()+14,4)){
    return(true);}
  errno=0;
  to_csum=(uint16_t)strtol(str_acnt.c_str()+14,&endptr,16);
  if(errno || endptr==str_acnt.c_str()+14){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad checksum\n",str_acnt.c_str());
    perror("ERROR: strtol");
    return(false);}
  to_crc16=crc_acnt(to_bank,to_user);
  if(to_csum!=to_crc16){
    fprintf(stderr,"ERROR: parse_acnt(%s) bad checksum (expected %04X)\n",str_acnt.c_str(),to_crc16);
    return(false);}
  return(true);
}

bool parse_txid(uint16_t& to_bank,uint32_t& node_msid,uint32_t& node_mpos,std::string str_txid)
{ char *endptr;
  if(str_txid.length()!=18){
    fprintf(stderr,"ERROR: parse_txid(%s) bad length (required 18)\n",str_txid.c_str());
    return(false);}
  if(str_txid[4]!=':' || str_txid[13]!=':'){
    fprintf(stderr,"ERROR: parse_txid(%s) bad format (required NNNN:MMMMMMMM:PPPP)\n",str_txid.c_str());
    return(false);}
  str_txid[4]='\0';
  str_txid[13]='\0';
  errno=0;
  to_bank=(uint16_t)strtol(str_txid.c_str(),&endptr,16);
  if(errno || endptr==str_txid.c_str()){
    fprintf(stderr,"ERROR: parse_txid(%s) bad bank\n",str_txid.c_str());
    perror("ERROR: strtol");
    return(false);}
  errno=0;
  node_msid=(uint32_t)strtoll(str_txid.c_str()+5,&endptr,16);
  if(errno || endptr==str_txid.c_str()+5){
    fprintf(stderr,"ERROR: parse_txid(%s) bad msid\n",str_txid.c_str());
    perror("ERROR: strtol");
    return(false);}
  errno=0;
  node_mpos=(uint16_t)strtol(str_txid.c_str()+14,&endptr,16);
  if(errno || endptr==str_txid.c_str()+14){
    fprintf(stderr,"ERROR: parse_txid(%s) bad mpos\n",str_txid.c_str());
    perror("ERROR: strtol");
    return(false);}
  return(true);
}



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
				("secret,s", boost::program_options::value<std::string>(&skey),					"passphrase or private key [64chars in hex format / 32bytes]")
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
				std::cerr << "Usage: " << av[0] << " [settings]\n";
				std::cerr << generic << "\n";
				std::cerr << config << "\n";
				exit(0);}
			if(vm.count("version")){
				std::cerr << "Version 1.0\n";
				exit(0);}

			if(vm.count("bank") || vm.count("address")){
				std::string line;
				if (vm.count("secret")){
					line = vm["secret"].as<std::string>();
				} else {
					std::cerr << "ENTER passphrase or private key\n";
					std::getline(std::cin,line);
					boost::trim_right_if(line,boost::is_any_of(" \r\n\t"));
					if(line.empty()){
						std::cerr << "ERROR, failed to read passphrase\n";
						exit(1);}
				}
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
			}

			if(vm.count("port")){
				std::cerr << "Bank port: " << vm["port"].as<int>() << std::endl;}
			else{
				std::cerr << "ERROR: bank port missing!" << std::endl;
				exit(1);}
			if (vm.count("host")){
				std::cerr << "Bank host: " << vm["host"].as<std::string>() << std::endl;}
			else{
				std::cerr << "ERROR: bank host missing!" << std::endl;
				exit(1);}
			if (vm.count("address")) {
				if(vm.count("bank") || vm.count("user")) {
					std::cerr << "ERROR: do not specify bank/user and address at the same time" << std::endl;
					exit(1);
				}
				std::cerr << "Address: " << vm["address"].as<std::string>() << std::endl;
				if(!parse_acnt(bank, user, addr)){
					std::cerr << "ERROR: invalid address" << std::endl;
					exit(1);
				}
				vm.insert(std::make_pair("bank", boost::program_options::variable_value(bank, false)));
				vm.insert(std::make_pair("user", boost::program_options::variable_value(user, false)));
			}
			if (vm.count("bank")){
				std::cerr << "Bank   id: " << vm["bank"].as<uint16_t>() << std::endl;}
			//else{
			//	std::cerr << "ERROR: bank id missing! Specify --bank or --address" << std::endl;
			//	exit(1);}
			if (vm.count("user")){
				std::cerr << "User   id: " << vm["user"].as<uint32_t>() << std::endl;}
			//else{
			//	std::cerr << "ERROR: user id missing! Specify --user or --address" << std::endl;
			//	exit(1);}
			if (vm.count("msid")){
				std::cerr << "LastMsgId: " << vm["msid"].as<int>() << std::endl;}
			else{
				if(vm.count("user")){
					std::cerr << "WARNING: last message id missing!" << std::endl;}}
			if (vm.count("nice")){
				std::cerr << "PrettyOut: " << vm["nice"].as<bool>() << std::endl;}
			if (vm.count("olog")){
				std::cerr << "LogOutput: " << vm["olog"].as<bool>() << std::endl;}
			if (vm.count("drun")){
				std::cerr << "Dry Run  : " << vm["drun"].as<bool>() << std::endl;}
			if (vm.count("hash")){
				if(hash.length()!=64){
					std::cerr << "ERROR: hash wrong length (should be 64): " << vm["hash"].as<std::string>() << std::endl;
					exit(1);}
				ed25519_text2key(ha,hash.c_str(),32);}
			else{
				if(vm.count("user")){
					std::cerr << "WARNING: last hash missing!" << std::endl;}}
			}

		catch(std::exception &e){
			std::cerr << "Exception: " << e.what() << std::endl;
			exit(1);
		}
	}
};

#endif // SETTINGS_HPP
