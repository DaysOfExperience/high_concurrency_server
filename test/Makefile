all:client1

client1:client1.cpp
	g++ -o $@ $^ -std=c++11 -lpthread

svr:tcp_server.cc
	g++ -o $@ $^ -std=c++17 -lpthread
cli:tcp_client.cc
	g++ -o $@ $^ -std=c++17 -lpthread

.PHONY:clean
clean:
	rm svr cli client1