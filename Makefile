.PHONY: clean
.PHONY: restart
.PHONY: all
CCP=g++ -Wall -fmax-errors=5
restart: main user
all: main user
ed25519/ed25519.o: ed25519/ed25519.c
	$(CCP) -c $< -o $@
user.o: user.cpp user.hpp settings.hpp
	$(CCP) -c $< -o $@
main.o: main.cpp candidate.hpp main.hpp office.hpp peer.hpp servers.hpp client.hpp message.hpp options.hpp server.hpp
	$(CCP) -c $< -o $@
main: main.o ed25519/ed25519.o
	$(CCP) $^ -m64 -lssl -lcrypto -lboost_thread -lpthread -lboost_system -lboost_program_options -lboost_serialization -lrt -o $@
user: user.o ed25519/ed25519.o
	$(CCP) $^ -m64 -lssl -lcrypto -lboost_thread -lpthread -lboost_system -lboost_program_options -lrt -o $@
clean:
	rm -f *.o main user
