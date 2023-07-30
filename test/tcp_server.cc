#include "../source/server.hpp"

void HandleClose(Channel *ch) {
    DBG_LOG("close : %d", ch->Fd());
    // 将该文件描述符的所有就绪事件清楚(~)
    ch->SetRevents(0);
    // 从Epoll中解除监控
    ch->RemoveFromEpoll();
    // close文件描述符，四次挥手，可能被动，可能主动
    close(ch->Fd());
    delete ch;
}
void HandleRead(Channel *ch) {
    int fd = ch->Fd();
    char buff[1024];
    int ret = recv(fd, buff, sizeof buff - 1, 0);
    if(ret <= 0) {
        // 读取失败，或者对端关闭连接
        return HandleClose(ch);
    }
    buff[ret] = 0;
    std::cout << "client# " << buff << std::endl;
    ch->EnableWrite();
}
void HandleWrite(Channel *ch) {
    std::string s("__zz11");
    int ret = send(ch->Fd(), s.c_str(), s.size(), 0);
    if(ret < 0) {
        return HandleClose(ch);
    }
    ch->DisableWrite();
}
void HandleError(Channel *ch) {
    return HandleClose(ch);
}
void HandleEvent(Channel *ch) {
    std::cout << "some event ready" << std::endl;
}
void Accept(Socket *sock, EventLoop *ev) {
    int newfd = sock->Accept();
    if(newfd < 0) return;
    Channel *ch = new Channel(ev, newfd);
    ch->SetReadCallback(std::bind(HandleRead, ch));
    ch->SetWriteCallback(std::bind(HandleWrite, ch));
    ch->SetErrorCallback(std::bind(HandleError, ch));
    ch->SetCloseCallback(std::bind(HandleClose, ch));
    ch->SetEventCallback(std::bind(HandleEvent, ch));
    ch->EnableRead();
}

int main()
{
    Socket lst_sock;
    lst_sock.CreateListenSocket(8080);
    EventLoop event_loop;
    Channel ch(&event_loop, lst_sock.Fd());
    ch.SetReadCallback(std::bind(Accept, &lst_sock, &event_loop));   // listen套接字只关心可读即可
    ch.EnableRead();
    event_loop.Start();
    lst_sock.Close();
    return 0;
}