out:
	cc -pthread -ggdb general.c log.c server.c tcp_delegate.c main.c -o tcpd


