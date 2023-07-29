all:svr cli

svr:tcp_server.cc
	g++ -o $@ $^ -std=c++17
cli:tcp_client.cc
	g++ -o $@ $^ -std=c++17

.PHONY:clean
clean:
	rm svr cli