all:client2

client2:client2.cc
	g++ -o $@ $^ -std=c++11 -lpthread

client1:client1.cc
	g++ -o $@ $^ -std=c++11 -lpthread

svr:tcp_server.cc
	g++ -o $@ $^ -std=c++17 -lpthread
cli:tcp_client.cc
	g++ -o $@ $^ -std=c++17 -lpthread