#include "../source/server.hpp"

int main()
{
    Socket sock;
    sock.CreateClientSocket("127.0.0.1", 8080);
    std::string s = "I am client";
    char buff[1024];
    while(1) {
        sleep(1);
        sock.Send(s.c_str(), s.size());
        // std::cout << "111" << std::endl;
        ssize_t sz = sock.Recv(buff, sizeof buff - 1, 0);
        // std::cout << "2222" << std::endl;
        buff[sz] = 0;
        std::cout << "server echo#" << buff << std::endl;
    }
    return 0;
}