.PHONY: restart
.PHONY: all
.PHONY: clean
#CC=gcc -Wall -fmax-errors=5 -fPIC -DED25519_SSE2 -O3 -m64 -static
#CC=gcc -Wall -fmax-errors=5 -fPIC -O3 -m64 -static -DED25519_TEST
CC=gcc -Wall -fmax-errors=5 -fPIC -O3 -m64
#mysql: mysql.so
all: test mytest key sign check
restart: test mytest key sign check
ed25519-test.o: ed25519.c ed25519.h
	$(CC) -DED25519_TEST -c $< -o $@
ed25519.o: ed25519.c ed25519.h
	$(CC) -c $< -o $@
test.o: test.c ed25519.h
	$(CC) -c $< -o $@
mytest.o: mytest.c ed25519.h
	$(CC) -c $< -o $@
key.o: key.c ed25519.h
	$(CC) -c $< -o $@
sign.o: sign.c ed25519.h
	$(CC) -c $< -o $@
check.o: check.c ed25519.h
	$(CC) -c $< -o $@
mysql.o: mysql.c ed25519.h
	$(CC) -I /usr/include/mysql/ -c $< -o $@
test: test.o ed25519-test.o
	$(CC) $^ -lssl -lcrypto -lpthread -ldl -o $@
mytest: mytest.o ed25519.o
	$(CC) $^ -lssl -lcrypto -lpthread -ldl -o $@
key: key.o ed25519.o
	$(CC) $^ -lssl -lcrypto -lpthread -ldl -o $@
sign: sign.o ed25519.o
	$(CC) $^ -lssl -lcrypto -lpthread -ldl -o $@
check: check.o ed25519.o
	$(CC) $^ -lssl -lcrypto -lpthread -ldl -o $@
mysql.so: mysql.o ed25519.o
	$(CC) $^ -lssl -lcrypto -lpthread -ldl -shared -o $@
clean:
	rm test mytest key sign check test.o mytest.o key.o sign.o check.o ed25519.o ed25519-test.o
