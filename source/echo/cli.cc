#include "../server.hpp"

int main()
{
    Socket sock;
    sock.CreateClientSocket("127.0.0.1", 8080);
    std::string s = "zzz";
    char buff[10240];
    while(1) {
        sleep(1);
        sock.Send(s.c_str(), s.size());
        ssize_t sz = sock.Recv(buff, sizeof buff - 1);
        if(sz > 0) {
            buff[sz] = 0;
        }
        DBG_LOG("server echo# %s", buff);
    }
    return 0;
}