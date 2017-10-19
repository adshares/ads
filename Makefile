.PHONY: restart
.PHONY: all
.PHONY: clean
CCP=g++ -Wall -m64 -fmax-errors=5 -fPIC -std=c++11 -g -DDEBUG
restart: escd esc
all: escd esc
ed25519/ed25519.o: ed25519/ed25519.c ed25519/ed25519.h
	$(CCP) -c $< -o $@
esc.o: user.cpp user.hpp settings.hpp ed25519/ed25519.h default.hpp message.hpp servers.hpp hlog.hpp
	$(CCP) -c $< -o $@
escd.o: main.cpp candidate.hpp office.hpp peer.hpp servers.hpp client.hpp message.hpp options.hpp server.hpp ed25519/ed25519.h user.hpp hash.hpp default.hpp hlog.hpp
	$(CCP) -c $< -o $@
escd: escd.o ed25519/ed25519.o
	$(CCP) $^ -m64 -lssl -lcrypto -lboost_thread -lpthread -lboost_system -lboost_program_options -lrt -o $@
esc: esc.o ed25519/ed25519.o
	$(CCP) $^ -m64 -lssl -lcrypto -lboost_thread -lpthread -lboost_system -lboost_program_options -lrt -o $@
clean:
	rm ed25519/ed25519.o esc.o escd.o escd esc
