out:
	cc -pthread -ggdb general.c crypt.c log.c server.c main.c -o tcpd
	cc -pthread -ggdb client/main.c -o client/client

