#ifndef __MUDUO_SERVER_HPP__
#define __MUDUO_SERVER_HPP__

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG

#define LOG(level, format, ...) do{\
        if (level < LOG_LEVEL) break;\
        time_t t = time(NULL);\
        struct tm *ltm = localtime(&t);\
        char tmp[32] = {0};\
        strftime(tmp, 31, "%H:%M:%S", ltm);\
        fprintf(stdout, "[%p %s %s:%d] " format "\n", (void*)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
    }while(0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

// 缓冲区类，实例化之后为接收缓冲区/发送缓冲区
// 作为每一个服务器中的TCP连接的应用层缓冲区(发送&接收)
#define BUFFER_DEFAULT_SIZE 1024    // 1024char 1024字节~
class Buffer
{
private:
    std::vector<char> _buffer; // 使用vector进行缓冲区的内存空间管理，不使用string是因为string中的'\0'为终止符
    uint64_t _read_idx;        // 读偏移
    uint64_t _write_idx;       // 写偏移
public:
    Buffer() : _buffer(1024), _read_idx(0), _write_idx(0) {
    }
    char *Begin() {
        return &_buffer[0];
    }
    // 读位置(非偏移量)
    char *ReadPosition() {
        return Begin() + _read_idx;
    }
    // 写位置
    char *WritePosition() {
        return Begin() + _write_idx;
    }
    // 末尾空余空间大小(字节数)
    uint64_t TailIdleSize() {
        return _buffer.size() - _write_idx;
    }
    // 起始空余空间大小
    uint64_t HeadIdleSize() {
        return _read_idx;
    }
    // 可读数据大小
    uint64_t ReadableSize() {
        return _write_idx - _read_idx;
    }
    // 确保可写空间足够（整体空闲空间够了就移动数据，否则就扩容）
    void EnsureWriteSpace(uint64_t len) {
        if(len <= TailIdleSize()) return ;
        if(len <= HeadIdleSize() + TailIdleSize()) {
            // 移动数据
            uint64_t sz = ReadableSize();
            std::copy(ReadPosition(), WritePosition(), Begin());
            _read_idx = 0;
            _write_idx = sz;
            return ;
        }
        _buffer.resize(_write_idx + len);  // 扩容之后刚好可用于此次写入
    }
public:
    // 写入数据
    void Write(const void *data, uint64_t len) {
        if(len == 0) return;
        EnsureWriteSpace(len);
        const char *d = (const char *)data;
        std::copy(d, d + len, WritePosition());
        _write_idx += len;
    }
    void WriteString(const std::string &s) {
        Write(s.c_str(), s.size());
    }
    void WriteBuffer(Buffer &buffer) {
        Write(buffer.ReadPosition(), buffer.ReadableSize());
    }
    // 读取数据(读走数据)
    void Read(void *buf, uint64_t len) {
        if(len > ReadableSize()) return;
        std::copy(ReadPosition(), ReadPosition() + len, (char*)buf);
        _read_idx += len;
    }
    std::string ReadAsString(uint64_t len) {
        std::string s(len, '0');
        Read(&s[0], len);
        return s;
    }
    // 找到'\n'
    char *FindCRLF() {
        char *res = (char*)memchr(ReadPosition(), '\n', ReadableSize());
        return res;
    }
    // 获取一行数据，适用于http?
    std::string Getline() {
        char *pos = FindCRLF();
        if(pos == nullptr) return "";
        // +1是为了把'\n'也读出去
        return ReadAsString(pos - ReadPosition() + 1);
    }
    // 清空缓冲区
    void Clear() {
        _read_idx = 0;
        _write_idx = 0;
    }
};
// 对套接字的封装，封装网络编程中的套接字相关的常用操作
#define MAX_LISTEN 1024
class Socket
{
private:
    int _sockfd;
public:
    Socket() {}
    Socket(int sockfd) : _sockfd(sockfd) {}
    ~Socket() {
        Close();
    }
    int Fd() {
        return _sockfd;
    }
    bool Create() {
        // int socket(int domain, int type, int protocol)
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd < 0) {
            ERR_LOG("CREATE SOCKET FAILED!!");
            return false;
        }
        return true;
    }
    bool Bind(const std::string &ip, uint16_t port) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        // int bind(int sockfd, struct sockaddr*addr, socklen_t len);
        int ret = bind(_sockfd, (struct sockaddr*)&addr, len);
        if (ret < 0) {
            ERR_LOG("BIND FAILED!");
            return false;
        }
        return true;
    }
    bool Listen(int backlog = MAX_LISTEN) {
        // int listen(int backlog)
        int ret = listen(_sockfd, backlog);
        if (ret < 0) {
            ERR_LOG("LISTEN FAILED!");
            return false;
        }
        return true;
    }
    // 客户端向服务端发起TCP连接
    bool Connect(const std::string ip, uint16_t port) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        // int connect(int sockfd, struct sockaddr*addr, socklen_t len);
        int ret = connect(_sockfd, (struct sockaddr*)&addr, len);
        if (ret < 0) {
            ERR_LOG("CONNECT SERVER FAILED!");
            return false;
        }
        return true;
    }
    // 服务端的listen套接字获取建立好的TCP连接
    int Accept() {
        int newfd = accept(_sockfd, nullptr, nullptr);
        if(newfd < 0) {
            ERR_LOG("ACCEPT ERROR");
            return -1;
        }
        return newfd;
    }
    // 从TCP套接字的接收缓冲区中读取数据(常规连接，常规套接字)
    ssize_t Recv(void *buf, size_t len, int flag = 0) {
        // ssize_t recv(int sockfd, void *buf, size_t len, int flag);
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if (ret <= 0) {
            //EAGAIN 当前socket的接收缓冲区中没有数据了，在非阻塞的情况下才会有这个错误
            //EINTR  表示当前socket的阻塞等待，被信号打断了，
            if (errno == EAGAIN || errno == EINTR) {
                return 0;//表示这次接收没有接收到数据
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret; //实际接收的数据长度
    }
    ssize_t RecvNonBlock(void *buf, size_t len) {
        return Recv(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞。
    }
    // 发送数据
    ssize_t Send(const void *buf, size_t len, int flag = 0) {
        // ssize_t send(int sockfd, void *data, size_t len, int flag);
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                return 0;
            }
            ERR_LOG("SOCKET SEND FAILED!!");
            return -1;
        }
        return ret;//实际发送的数据长度
    }
    ssize_t SendNonBlock(const void *buf, size_t len) {
        return Send(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前发送为非阻塞。
    }
    // 创建一个tcp server的listen socket
    bool CreateListenSocket(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = true) {
        //1. 创建套接字，2. 绑定地址，3. 开始监听，4. 设置非阻塞(可选)， 5. 启动地址重用
        if (Create() == false) return false;
        if (!block_flag) NonBlock();   // 指的是这个listen套接字为非阻塞
        if (Bind(ip, port) == false) return false;
        if (Listen() == false) return false;
        ReuseAddress();
        return true;
    }
    // 创建一个客户端与服务端进行通信的TCP套接字，且已连接
    bool CreateClientSocket(const std::string ip, uint16_t port) {
        if(Create() == false) return false;
        if(Connect(ip, port) == false) return false;
        return true;
    }
    // 设置套接字选项--开启ip和port的地址重用(TIME_WAIT)
    void ReuseAddress() {
        // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void*)&val, sizeof(int));
    }
    // 设置套接字文件为非阻塞（若没有数据/发送缓冲区已满，则出错返回）
    void NonBlock() {
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
    void Close() {
        if(_sockfd >= 0)
            close(_sockfd);
    }
};

// 事件管理: 该文件描述符关心哪些事件、哪些事件就绪、每个事件就绪时对应的回调函数
class EventLoop;
class Channel
{
private:
    EventLoop *_event_loop;  // 封装了Poller(epoll)
private:
    int _fd;
    uint32_t _events;   // 当前关心的事件
    uint32_t _revents;  // 当前就绪的事件
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;   // 可读事件就绪时的回调函数
    EventCallback _write_callback;  // 可写事件就绪时的回调函数
    EventCallback _error_callback;  // 错误事件就绪时的回调函数
    EventCallback _close_callback;  // 连接断开事件就绪时的回调函数
    EventCallback _event_callback;  // 任意事件就绪时的回调函数（重置计时器，证明该连接活跃）
public:
    Channel(EventLoop *p, int fd)
    : _event_loop(p), _fd(fd), _events(0), _revents(0) {
    }
    int Fd() {
        return _fd;
    }
    uint32_t Events() {
        return _events;
    }
    // 设置当前就绪的事件（epoll模型在epool_wait之后进行设置）
    void SetRevents(uint32_t revents) {
        _revents = revents;
    }
    // 事件就绪的回调方法的设定，在channel定义后即进行。
    void SetReadCallback(const EventCallback &cb) {
        _read_callback = cb;
    }
    void SetWriteCallback(const EventCallback &cb) {
        _write_callback = cb;
    }
    void SetErrorCallback(const EventCallback &cb) {
        _error_callback = cb;
    }
    void SetCloseCallback(const EventCallback &cb) {
        _close_callback = cb;
    }
    void SetEventCallback(const EventCallback &cb) {
        _event_callback = cb;
    }
    // 关心可读/取消关心可读事件(一般来说，每个文件描述符（listen套接字，普通TCP连接，eventfd）开始时都只关心读事件)
    // 下面几个方法都需要这个channel关联的epoll(EventLoop)来进行具体设定
    // 且具有立即性，调用之后立刻生效
    void EnableRead() {
        _events |= EPOLLIN;
        UpdateFromEpoll();
    }
    void DisableRead() {
        _events &= ~EPOLLIN;
        UpdateFromEpoll();
    }
    // 关心可写/取消关心可写
    void EnableWrite() {
        _events |= EPOLLOUT;
        UpdateFromEpoll();
    }
    void DisableWrite() {
        _events &= ~EPOLLOUT;
        UpdateFromEpoll();
    }
    // // 取消所有事件监控(此时在epoll中还有什么意义吗?)     有用？
    // void DisableAllEvent() {
    //     _events = 0;
    // }
    // 从epoll中移除该文件描述符
    // 下方两个方法调用了此channel绑定的epoll(EventLoop)的方法
    // 调用的方法在其他类，且在下方定义，所以需要类体外定义(EventLoop定义后)
    void RemoveFromEpoll();
    void UpdateFromEpoll();
    // 事件处理: 调用此时所有就绪事件的回调函数，即处理当前文件描述符的就绪事件~
    void HandleEvent() {
        if(_revents & EPOLLIN || _revents & EPOLLPRI || _revents & EPOLLHUP) {
            if(_read_callback) _read_callback();
        }
        if(_revents & EPOLLOUT) {
            if(_write_callback) _write_callback();
        } else if(_revents & EPOLLERR) {
            if(_error_callback) _error_callback();
        } else if(_revents & EPOLLHUP) {
            if(_close_callback) _close_callback();
        }
        if(_event_callback) _event_callback();
    }
};
#define MAX_EPOLLEVENTS 1024
class Poller
{
private:
    int _epfd;   // 仅仅只是epoll模型的一个标识
    struct epoll_event _revs[MAX_EPOLLEVENTS];  // epoll_wait之后存储所有就绪的文件描述符及其就绪事件
    std::unordered_map<int, Channel *> _channels; // 当前epoll监管了哪些文件描述符
public:
    Poller() {
        _epfd = epoll_create(666);
        if(_epfd < 0) {
            ERR_LOG("EPOLL CREATE ERROR!!");
            abort();
        }
    }
    // 比如: 新增文件描述符监管，修改监管，移除监管。
    // 单次epoll_wait
    // 添加或修改文件描述符的事件监控
    void UpdateFromEpoll(Channel *ch) {
        auto it = _channels.find(ch->Fd());
        if(it == _channels.end()) {
            // 新增文件描述符监控
            // 注意还要新增到_channels中
            _channels.insert(std::make_pair(ch->Fd(), ch));
            EpollCtl(ch, EPOLL_CTL_ADD);
            return ;
        }
        // 更改文件描述符监控事件
        EpollCtl(ch, EPOLL_CTL_MOD);
    }
    void RemoveFromEpoll(Channel *ch) {
        if(_channels.find(ch->Fd()) == _channels.end())
            return;
        _channels.erase(ch->Fd());
        EpollCtl(ch, EPOLL_CTL_DEL);
    }
    // 单次的epoll_wait，获取当前有事件就绪的文件描述符(返回活跃连接)
    void Poll(std::vector<Channel *> *actives) {
        int nfds = epoll_wait(_epfd, _revs, MAX_EPOLLEVENTS, -1); // -1阻塞、0非阻塞，>0定时阻塞
        if(nfds < 0) {
            if(errno == EINTR) {
                return;
            }
            ERR_LOG("EPOLL WAIT ERROR : %s", strerror(errno));
            abort();
        }
        // 此处nfds不可能等于0，因为是阻塞
        for(int i = 0; i < nfds; ++i) {
            int fd = _revs[i].data.fd;
            uint32_t revents = _revs[i].events;
            if(_channels.find(fd) == _channels.end()) {
                ERR_LOG("POLL ERROR!!");
                abort();
            }
            Channel *ch = _channels[fd];
            ch->SetRevents(revents);
            // ch->HandleEvent();   // 这里不handle
            actives->push_back(ch);
        }
    }
private:
    void EpollCtl(Channel *ch, int op) {
        // int epoll_ctl(int __epfd, int __op, int __fd, epoll_event *)
        int fd = ch->Fd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = ch->Events();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if(ret < 0) {
            ERR_LOG("EPOLL CTL ERROR!!");
            abort();
        }
        return ;
    }
};
class EventLoop
{
private:
    Poller poller;
public:
    EventLoop() {}
    void Start() {
        while(1) {
            std::vector<Channel *> channels;
            poller.Poll(&channels);
            for(auto &i:channels) {
                i->HandleEvent();
            }
        }
    }
    // epoll相关
    // 添加描述符监控/修改描述符的事件监控
    void UpdateFromEpoll(Channel *ch) {
        poller.UpdateFromEpoll(ch);
    }
    void RemoveFromEpoll(Channel *ch) {
        poller.RemoveFromEpoll(ch);
    }
};
// 从epoll中移除该文件描述符
void Channel::RemoveFromEpoll() {
    _event_loop->RemoveFromEpoll(this);
}
// 新增文件描述符监管 / 修改文件描述符的监管事件
void Channel::UpdateFromEpoll() {
    _event_loop->UpdateFromEpoll(this);
}
#endif