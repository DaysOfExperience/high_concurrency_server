#ifndef __MUDUO_SERVER_HPP__
#define __MUDUO_SERVER_HPP__

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <cstring>
#include <cassert>
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
#include <sys/eventfd.h>
#include <sys/timerfd.h>

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
    void MoveReadOffset(ssize_t len) {
        // 比如发送缓冲区的数据发出去了，从外部需要移动一下
        _read_idx += len;
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
        if(len == 0) return 0;
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

// 事件管理类: 该文件描述符关心哪些事件、哪些事件就绪、每个事件就绪时对应的回调函数
class EventLoop;
/*1. 提供EventLoop*(该文件描述符的监管放在哪个Epoll中了)和fd 2. 回调函数设定 3. 该文件描述符想要关心哪些事件(Enable)*/
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
    // 常规操作：初始化时,提供EventLoop*和fd，回调函数设定，以及该文件描述符想要关心哪些事件
    // 就好了，在对应事件发生时，会自动在EventLoop对应的Poller中进行事件回调~~
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
        if(_event_callback) _event_callback();
        if((_revents & EPOLLIN) || (_revents & EPOLLPRI) || (_revents & EPOLLHUP)) {
            if(_read_callback) _read_callback();
        }else if(_revents & EPOLLOUT) {
            if(_write_callback) _write_callback();
        } else if(_revents & EPOLLERR) {
            if(_error_callback) _error_callback();
        } else if(_revents & EPOLLHUP) {
            if(_close_callback) _close_callback();
        }
    }
};
#define MAX_EPOLLEVENTS 1024
/*该类直接被EventLoop封装。构造时，直接构造一个Epoll模型。
提供两个接口1. void UpdateFromEpoll(Channel *ch) 2. void RemoveFromEpoll(Channel *ch) 3. Poll(一次epoll_wait)*/
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
        // 此处nfds不可能等于0，因为是阻塞式epoll_wait
        for(int i = 0; i < nfds; ++i) {
            int fd = _revs[i].data.fd;
            uint32_t revents = _revs[i].events;
            assert(_channels.find(fd) != _channels.end());
            Channel *ch = _channels[fd];
            ch->SetRevents(revents);
            // ch->HandleEvent();   // 这里不handle，外层获取了actives再handle，其实差不多
            actives->push_back(ch);
        }
        return ;
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
class TimerTask
{
private:
    uint64_t _id;       // 定时任务唯一id
    uint32_t _timeout;  // 定时时长
    using TaskFunc = std::function<void()>;
    using ReleaseTimer = std::function<void()>;
    TaskFunc _task_cb;  // 定时器对象要执行的定时任务
    ReleaseTimer _release; // 用于删除TimerWheel中保存的TimerTask
    bool _canceled;
public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb)
    : _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
    ~TimerTask() { 
        if (_canceled == false) _task_cb(); 
        _release();    // 自动执行TimerWheel中的RemoveTimer，从_timers中删除对应的定时任务(weak_ptr)
    }
    void Cancel() {
        _canceled = true;
    }
    void SetReleaseTimer(const ReleaseTimer &cb) {
        _release = cb;
    }
    uint64_t GetDelayTime() {
        return _timeout;
    }
};
#define TIMER_WHEEL_SIZE 60
class TimerWheel
{
private:
    using SharedTask = std::shared_ptr<TimerTask>;
    using WeakTask = std::weak_ptr<TimerTask>;
    using TaskFunc = std::function<void()>;
    int _size;  // 时间轮大小，与最大定时时间相关
    int _tick;  // 时间轮的秒针，每向前一步，就会执行时间轮的一个轮子里面的所有任务(具体多久走一步，取决于timerfd)
    std::vector<std::vector<SharedTask>> _timerwheel;
    std::unordered_map<uint64_t, WeakTask> _timers;

    EventLoop *_loop;
    int _timerfd; // 定时器描述符--可读事件回调就是读取计数器，tick指针前移，执行定时任务
    std::unique_ptr<Channel> _timerfd_channel;
public:
    TimerWheel(EventLoop *loop): 
    _size(TIMER_WHEEL_SIZE), _timerwheel(TIMER_WHEEL_SIZE), _tick(0),
    _loop(loop), _timerfd(CreateTimerfd()), _timerfd_channel(new Channel(loop, _timerfd)) {
        // 此处timerfd已经开始计时(我们定的是每1s超时一次)，到时间会向timerfd中写入
        // timerfd的读事件由EventLoop 也就是epoll监督
        // 每到时，就调用Ontime，读取timerfd文件中的超时次数
        // 进一步调用TickOnce，_tick前移一步（超时多次前移多步）
        // 执行超时任务
        _timerfd_channel->SetReadCallback(std::bind(&TimerWheel::Ontime, this));
        _timerfd_channel->EnableRead();
    }
    ~TimerWheel() {}
private:
    void TickOnce() {
        _tick += 1;
        _tick %= _size;
        // 销毁这个轮子上的所有智能指针，若引用计数变为0，则执行TimerTask的析构函数，执行定时任务（若没有cancel）
        _timerwheel[_tick].clear();
    }
    // 可以说，下面这个操作，每到一次时间都会执行，只是时间轮里面可能没有任务而已。
    void Ontime() {
        // 根据实际超时的次数，执行对应的超时任务
        int times = ReadTimefd();
        for (int i = 0; i < times; i++) {
            TickOnce();
        }
    }
    // 我们实现一个时间轮
    // timerfd用epoll监管，到时间，timerfd读就绪，根据读事件回调函数就执行Ontime，从而执行时间轮中任务而已。

public:
    /*定时器中有个_timers成员，定时器数据的操作有可能在多线程中进行，因此需要考虑线程安全问题*/
    /*如果不想加锁，那就把对定时器的所有操作，都放到一个线程中进行*/

    // 下面的三个方法，不能直接执行，因为存在线程安全问题
    // 确保这些方法在EventLoop对应的线程中执行就没有线程安全问题了。
    // 所以调用RunInLoop

    // 确保执行下方函数时，要在EventLoop线程中
    // 如果在，则直接执行，如果不在则push到EventLoop的任务队列中
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    // 刷新/延迟定时任务
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);
    /*这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行*/
    bool HasTimer(uint64_t id) {
        auto it = _timers.find(id);
        if (it == _timers.end()) {
            return false;
        }
        return true;
    }
    // 其实外部使用的就是上面这三个方法:1、添加任务2、刷新任务3、取消任务
public:
    // 一个新的TCP连接，一个Timer定时任务，时间到了，代表超时，关闭非活跃连接
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb) {
        SharedTask pTask(new TimerTask(id, delay, cb));
        pTask->SetReleaseTimer(std::bind(&TimerWheel::RemoveTimer, this, id));
        int pos = (_tick + delay) % _size;   // 这个任务放入的时间轮的位置
        _timerwheel[pos].push_back(pTask);
        _timers.insert({id, WeakTask(pTask)});   // 利用shared_ptr构造weak_ptr?
    }
    // 当一个TCP连接事件就绪，代表活跃，刷新定时任务，重新计时
    void TimerRefreshInLoop(uint64_t id) {
        // 通过id在unordered_map中找到对应的保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中。
        auto it = _timers.find(id);
        if(it == _timers.end()) {
            return ;   // 任务不存在
        }
        SharedTask pTask = it->second.lock();
        uint64_t delay = pTask->GetDelayTime();
        int pos = (_tick + delay) % _size;
        _timerwheel[pos].push_back(pTask);
    }
    // 当一个连接突然关闭，要取消对应的定时任务~(只是到时间之后不会执行对应任务，实则对应任务还是在_timers内的，这里不回删除)
    void TimerCancelInLoop(uint64_t id) {
        auto it = _timers.find(id);
        if(it == _timers.end()) {
            return ;   // 任务不存在
        }
        SharedTask pTask = it->second.lock();
        if(pTask) pTask->Cancel();
    }
private:
    // 用于当某TimerTask析构执行任务时，需要从_timers里面移除
    void RemoveTimer(uint64_t id) {
        auto it = _timers.find(id);
        if (it != _timers.end()) {
            _timers.erase(it);
        }
    }
    int CreateTimerfd() {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);   // 创建timerfd
        if (timerfd < 0) {
            ERR_LOG("TIMERFD CREATE FAILED!!");
            abort();
        }
        //int timerfd_settime(int fd, int flags, struct itimerspec *new, struct itimerspec *old);
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0;// 第一次超时时间为1s后
        itime.it_interval.tv_sec = 1; 
        itime.it_interval.tv_nsec = 0; // 第一次超时后，每次超时事件间隔1秒。总的来说，一秒钟
        timerfd_settime(timerfd, 0, &itime, NULL);   // 自此，timerfd开始计时工作，每秒钟往timerfd对应文件中写入一个1(累积)
        return timerfd;
    }
    int ReadTimefd() {
        uint64_t times;
        // 有可能因为其他描述符的事件处理花费事件比较长，然后在处理定时器描述符事件的时候，有可能就已经超时了多次
        // read读取到的数据times就是从上一次read之后超时的次数
        // 因为实际的超时任务也就是关闭非活跃连接，所以，超一点时间无妨
        int ret = read(_timerfd, &times, sizeof times);
        if (ret < 0) {
            ERR_LOG("READ TIMEFD FAILED!");
            abort();
        }
        return times;
    }
};
class EventLoop
{
private:
    std::thread::id _thread_id;// 线程ID
    // eventfd用于唤醒poller监控IO事件有可能导致的阻塞，当eventfd读事件就绪时，不需要关心里面的数据(8字节整数)
    int _event_fd;
    std::unique_ptr<Channel> _event_channel;
    
    Poller poller;

    // 我他妈之前一直不明白_tasks为什么涉及线程安全
    // 比如: EventLoop线程在Start内RunAllTask访问_tasks
    // 而主线程在用RunInLoop往_tasks内push_back任务。就涉及线程安全
    using Task = std::function<void()>;
    std::vector<Task> _tasks;// 任务池
    std::mutex _mutex;// 实现任务池操作的线程安全

    // 时间轮定时器模块：用timerfd，每秒读事件就绪一次，epoll检测到，调用读事件处理函数，tick前移，执行某轮子中的任务
    using TaskFunc = std::function<void()>;
    TimerWheel _timer_wheel;  // 定时器模块
public:
    EventLoop():
    _thread_id(std::this_thread::get_id()), 
    _event_fd(CreateEventFd()), 
    _event_channel(new Channel(this, _event_fd)),
    _timer_wheel(this) {
        DBG_LOG("test 222");
        // 有关timerwheel：在构造TimerWheel的对象时，传过去EventLoop指针，就已经完成了所有操作了，包括timerfd的回调设定
        // 包括timerfd在EventLoop中的读事件关心。外部可以调用TimerWheel的三个接口进行任务添加，刷新，删除~(RunInLoop)
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this));
        _event_channel->EnableRead();
        // 将eventfd的读事件监督交给epoll(poller)，这样可以通过WriteEventfd来唤醒阻塞在epoll_wait中的epoll线程。
    }
    // 真-Event Loop
    void Start() {
        // 事件监控 -> 事件处理 -> 执行任务(????)
        while(1) {
            std::vector<Channel *> channels;
            poller.Poll(&channels);
            for(auto &i:channels) {
                i->HandleEvent();
            }
            // 调用listen套接字的读事件处理函数时，会接收新的通信套接字，然后调用EnableRead
            // EnableRead会进一步调用EventLoop的UpdateFromEpoll

            // 执行任务
            RunAllTask();
        }
    }
    // 判断执行RunInLoop的线程是否是该EventLoop绑定线程，若是，则直接执行任务即可
    // 若不是，则压入任务池中
    void RunInLoop(const Task &cb) {
        if (IsInLoop()) {
            return cb();
        }
        return QueueInLoop(cb);
    }
    // 线程安全的任务池相关的   ????
    void RunAllTask() {
        // 执行任务池中的所有任务
        std::vector<Task> tasks;
        {
            // 保证线程安全????
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.swap(tasks);
        }
        for (auto &f : tasks) {
            f();
        }
        return ;
    }
    void AssertInLoop() {
        assert(_thread_id == std::this_thread::get_id());
    }
    // 用于判断当前线程是否是EventLoop对应的线程；
    bool IsInLoop() {
        return (_thread_id == std::this_thread::get_id());
    }
    // 将操作压入任务池
    void QueueInLoop(const Task &cb) {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        // 唤醒有可能因为没有事件就绪，而导致的阻塞的epoll；
        // 其实就是给eventfd写入一个数据，eventfd就会触发可读事件
        WriteEventFd();
    }
    // epoll相关(poller)
    // 添加描述符监控/修改描述符的事件监控
    void UpdateFromEpoll(Channel *ch) {
        poller.UpdateFromEpoll(ch);
    }
    void RemoveFromEpoll(Channel *ch) {
        poller.RemoveFromEpoll(ch);
    }
    // eventfd相关，防止epoll在epollwait时一直阻塞。用于唤醒poller（通过向eventfd中写数据WriteEventfd）
    // static int CreateEventFd() {
    int CreateEventFd() {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            ERR_LOG("CREATE EVENTFD FAILED!!");
            abort(); // 让程序异常退出
        }
        return efd;
    }
    void ReadEventfd() {
        uint64_t res = 0;
        int ret = read(_event_fd, &res, sizeof(res));
        if (ret < 0) {
            // EINTR -- 被信号打断；   EAGAIN -- 表示无数据可读
            if (errno == EINTR || errno == EAGAIN) {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return ;
    }
    void WriteEventFd() {
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if (ret < 0) {
            if (errno == EINTR) {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return ;
    }
    // TimerWheel相关: 实现非活跃连接销毁功能
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
        return _timer_wheel.TimerAdd(id, delay, cb);
    }
    // 当可以确定执行的线程就是当前EventLoop绑定线程时，可以直接调用下方方法
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb) {
        return _timer_wheel.TimerAddInLoop(id, delay, cb);
    }
    void TimerRefresh(uint64_t id) {
        return _timer_wheel.TimerRefresh(id);
    }
    void TimerRefreshInLoop(uint64_t id) {
        return _timer_wheel.TimerRefreshInLoop(id);
    }
    void TimerCancel(uint64_t id) { 
        return _timer_wheel.TimerCancel(id); 
    }
    bool HasTimer(uint64_t id) {
        return _timer_wheel.HasTimer(id);
    }
};
// 新增文件描述符监管 / 修改文件描述符的监管事件
void Channel::UpdateFromEpoll() {
    _event_loop->UpdateFromEpoll(this);
}
// 从epoll中移除该文件描述符
void Channel::RemoveFromEpoll() {
    _event_loop->RemoveFromEpoll(this);
}
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
// 刷新/延迟定时任务
void TimerWheel::TimerRefresh(uint64_t id) {
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id) {
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}
// 因为EventLoop必须与一个线程绑定，而EventLoop在构造函数中就会以构造它的线程作为绑定线程。
// 因此我们不能在主线程（主Reactor）中创建EventLoop
/*创建一个LoopThread相当于直接创建了一个从Reactor线程，开始EventLoop*/
class LoopThread
{
private:
    EventLoop *_loop;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _cond;
public:
    LoopThread(): _loop(nullptr), _thread(&LoopThread::ThreadIntry, this) 
    { }
    EventLoop *GetLoop() {
        std::unique_lock<std::mutex> lock(_mutex); // 加锁
        _cond.wait(lock, [&](){ return _loop != NULL; });  // 若条件不达成，则在条件变量下等待，且释放_mutex;
        return _loop;
    }
private:
    // 实际上，每一个从Reactor线程都执行的是这个函数~
    void ThreadIntry() {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex); // 加锁
            _loop = &loop;
            _cond.notify_all();
        }
        loop.Start();
    }
};
class LoopThreadPool
{
private:
    int _thread_num;   // 线程池内线程个数
    int _loop_index;
    EventLoop *_base_loop;
    std::vector<LoopThread *> _threads;
    std::vector<EventLoop *> _loops;
public:
    /*if thread_num == 0, then you should set base_loop
      if not, you could not set base_loop */
    LoopThreadPool(int thread_num, EventLoop *base_loop = nullptr) :
        _thread_num(thread_num), _loop_index(0), _base_loop(base_loop) {
        DBG_LOG("test 444");
        for(int i = 0; i < _thread_num; ++i) {
            _threads.push_back(new LoopThread());
            _loops.push_back(_threads[i]->GetLoop());  // safely
            // _loops.push_back((*_threads.end())->GetLoop());  // safely
        }
    }
    EventLoop *GetLoopBalanced() {
        if(_thread_num == 0) return _base_loop;
        _loop_index = (_loop_index + 1) % _thread_num;
        return _loops[_loop_index];
    }
};
typedef enum {
    CONNECTING,    // 建立连接初步完成，待处理状态（实际上待处理的任务就是:读事件关心一下）
    CONNECTED,     // 连接建立完成，各种设置已完成，可以通信的状态
    DISCONNECTING, // 待关闭状态
    DISCONNECTED   // 连接关闭状态
} ConnStatus;
class Connection : public std::enable_shared_from_this<Connection> 
{
private:
    uint64_t _conn_id;  // 连接的唯一ID
    uint64_t _timer_id; // 连接对应的定时任务的ID(超时删除)，具有唯一性即可，故为了方便默认设为_conn_id
    int _sockfd;        // 连接关联的文件描述符
    ConnStatus _status; // 连接状态
    bool _enable_inactive_release; // 连接是否开启非活跃销毁，默认false

    EventLoop *_loop;   // 连接所在的EventLoop(epoll)

    Socket _socket;
    Channel _channel;
    Buffer _in_buffer;
    Buffer _out_buffer;
private:
    using PtrConnection = std::shared_ptr<Connection>;
    using MessageCallback = std::function<void (const PtrConnection &, Buffer *in_buffer)>;
    using ConnectedCallback = std::function<void (const PtrConnection &)>;
    using CloseCallback = std::function<void (const PtrConnection &)>;
    using AnyEventCallback = std::function<void (const PtrConnection &)>;
    // HandleRead读事件监控回调函数，从TCP内核接收缓冲区中读取数据，放到_in_buffer之后
    // 进行业务处理，业务处理函数即_message_callback，为用户设定的（HandleRead方法内）
    MessageCallback _message_callback;
    // 连接建立完成时的回调函数，用户想让连接建立完成时执行个什么东西，就通过这个设定~（Establish方法内）
    ConnectedCallback _connected_callback;
    CloseCallback _close_callback;
    // 在任意事件触发时，就会调用这个~~~（如果User设定了的话）（HandleEvent方法内）
    AnyEventCallback _any_event_callback;

    CloseCallback _server_close_callback;
public:
    // 和Channel的构造的唯一区别就是多了一个_conn_id而已
    // 在Connection构造时，会设定状态为CONNECTING
    Connection(EventLoop *loop, uint64_t id, int sockfd):
        _conn_id(id), _timer_id(id), _sockfd(sockfd), _status(CONNECTING), _enable_inactive_release(false),
        _loop(loop), _socket(_sockfd), _channel(_loop, _sockfd), _in_buffer(), _out_buffer() {
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
    }
    ~Connection() {}
    // int Fd() {    // useless
    // }
    uint64_t Id() {
        return _conn_id;
    }
    // bool Connected() { return (_statu == CONNECTED); }    // useless
    // 完了再说~
    // void SetContext(const Any &context) { _context = context; }
    // Any *GetContext() { return &_context; }
    void SetMessageCallback(const MessageCallback &cb) {
        _message_callback = cb;
    }
    void SetConnectedCallback(const ConnectedCallback &cb) {
        _connected_callback = cb;
    }
    void SetCloseCallback(const CloseCallback &cb) {
        _close_callback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback &cb) {
        _any_event_callback = cb;
    }
    void SetSvrCloseCallback(const CloseCallback &cb) {
        _server_close_callback = cb;
    }
    void Establish() {
        // 调用这个连接所绑定的EventLoop的方法
        _loop->RunInLoop(std::bind(&Connection::EstablishInLoop, this));
    }
    // User在编写业务处理函数时，对_in_buffer内的数据进行业务处理，然后调用此方法进行发送业务处理之后的给客户端的数据
    void Send(const char *data, size_t len) {
        // 按理来说，这个业务处理函数，一定是在HandleRead内调用的，所以应该就是InLoop状态????....
        // User传入的data，可能是个临时的空间，我们现在只是把发送操作压入了任务池，有可能并没有被立即执行
        // 因此有可能执行的时候，data指向的空间有可能已经被释放了。
        assert(_loop->IsInLoop());   // test
        Buffer buffer;
        buffer.Write(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buffer)));
    }
    void Shutdown() {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
    }
    void Release() {
        _loop->RunInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }
    void EnableInactiveRelease(int sec) {
        // 主线程创建新Connection时执行这里~
        // 让连接对应绑定的线程执行EnableInactiveReleaseInLoop~~~~   hhhhhh
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }
    // void Upgrade(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg, 
    // 实际上没有被调用过，只是提供一个接口
    // void CancelInactiveRelease() {
    //     _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    // }
private:
    void HandleRead() {
        // 1. 将TCP的内核接收缓冲区中的数据读取到用户层接收缓冲区
        char buf[65536];
        ssize_t sz = _socket.RecvNonBlock(buf, sizeof buf - 1);
        if(sz < 0) {
            // 读取出错，不能直接关闭连接
            return ShutdownInLoop();
        }
        _in_buffer.Write(buf, sz);
        // 2. 进行业务处理，调用用户指定的业务处理函数
        if(_in_buffer.ReadableSize() > 0) {
            // shared_from_this--从当前对象自身获取自身的shared_ptr管理对象
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }
    // 连接的写事件就绪后的回调函数：将发送缓冲区中的数据进行发送即可
    void HandleWrite() {
        ssize_t sz = _socket.SendNonBlock(_out_buffer.ReadPosition(), _out_buffer.ReadableSize());
        if(sz < 0) {
            // 发送失败，看看接收缓冲区中是否有数据，有就处理一下，没有就直接关闭喽
            if(_in_buffer.ReadableSize() > 0) {
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return ReleaseInLoop();  // 实际的关闭释放操作，此时不用关心发送缓冲区了，因为发送失败了~~
        }
        // 发送成功
        _out_buffer.MoveReadOffset(sz);  // 将发送缓冲区中已经发送出去的数据进行清理
        if(_out_buffer.ReadableSize() == 0) {
            // 有可能我此时发数据，是为了关闭连接前的最后处理一下
            if(_status == DISCONNECTING) {
                return ReleaseInLoop();
            }// 正常发送，且发完了
            _channel.DisableWrite(); // 关闭可写关心
        }
        return ;
    }
    // 描述符触发挂断事件之后的回调函数
    void HandleClose() {
        // 此时连接已经挂断，接收缓冲区有数据就处理一下
        if(_in_buffer.ReadableSize() > 0) {
            if(_message_callback) {
                _message_callback(shared_from_this(), &_in_buffer);
            }   
        }
        return ReleaseInLoop();
    }
    // 描述符触发出错事件??
    void HandleError() {
        return HandleClose();
    }
    void HandleEvent() {
        if(_enable_inactive_release == true) {
            // 需要刷新定时销毁任务
            _loop->TimerRefreshInLoop(_timer_id);  // _conn_id
        }
        if(_any_event_callback) {
            _any_event_callback(shared_from_this());
        }
    }
    void EstablishInLoop() {
        // 从属Reactor线程执行这个，理解为连接建立的收尾工作~关键步骤~
        assert(_loop->IsInLoop());
        assert(_status == CONNECTING);
        // 这个是关键~
        _channel.EnableRead();  // 使连接绑定的EventLoop里面的epoll关心读事件
        _status = CONNECTED;
        if(_connected_callback) {
            _connected_callback(shared_from_this());
        }
    }
    // 这个关闭操作并非实际的连接释放操作，需要判断还有没有数据待处理，待发送
    // 比如数据读取出错调用这个
    void ShutdownInLoop() {
        // 真正的关闭连接之前，接收缓冲区有数据就处理一下，发送缓冲区有数据就发送一下
        _status = DISCONNECTING;  // 设置连接为半关闭状态
        if(_in_buffer.ReadableSize() > 0) {
            if (_message_callback) {
                _message_callback(shared_from_this(), &_in_buffer);
            }
        }
        if(_out_buffer.ReadableSize() > 0) {
            if(!(_channel.Events() & EPOLLOUT)) {
                _channel.EnableWrite();
            }
        }
        else {
            // 没有数据未发送了
            ReleaseInLoop();
        }
    }
    void ReleaseInLoop() {
        _status = DISCONNECTED;
        _channel.RemoveFromEpoll();   // 从epoll模型中取出来
        _socket.Close();
        if(_loop->HasTimer(_timer_id)) {
            // 此时还有定时销毁任务
            _enable_inactive_release = false;
            if(_loop->HasTimer(_timer_id)) {
                _loop->TimerCancel(_timer_id);
            }
        }
        // User设定的连接关闭的回调函数
        if(_close_callback) {
            _close_callback(shared_from_this());
        }
        if(_server_close_callback) {
            // 需要从TcpServer中删除保存的Connection
            _server_close_callback(shared_from_this());
        }
    }
    // 并非实际的发送函数，而是把数据写入发送缓冲区中，启动写事件关心，由HandleWrite进行实际写入
    void SendInLoop(Buffer &buf) {
        if(_status == DISCONNECTED) return ;
        _out_buffer.WriteBuffer(buf);
        if(!(_channel.Events() & EPOLLOUT)) {
            _channel.EnableWrite();
        }
    }
    // void UpgradeInLoop(){} 
    void EnableInactiveReleaseInLoop(int sec) {
        // 比如连接建立时，_loop线程就要执行这个喽，去创建定时任务，如果功能开启的话~~
        _enable_inactive_release = true;
        // 按理来说这里不可能是已存在的
        if(_loop->HasTimer(_conn_id)) {
            // 定时任务已存在，刷新即可
            return _loop->TimerRefresh(_conn_id);
        }
        _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::Release, this));
    }
    // 取消非活跃销毁???? 这有用?
    // void CancelInactiveReleaseInLoop() {}
};
// Connection是对TCP通信套接字的同一管理，Acceptor是对listen套接字的统一管理
class Acceptor
{
private:
    Socket _listen_socket;
    EventLoop *_base_loop;   // 没有存在的必要~
    Channel _channel;

    using AcceptCallback = std::function<void(int)>;
    AcceptCallback _accept_callback;
public:
    Acceptor(EventLoop *base_loop, int port) :
        _listen_socket(CreateListenSocket(port)),
        _channel(base_loop, _listen_socket.Fd()) {
        DBG_LOG("test 333");
        _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
        // 这里只是设定了listen套接字的读事件回调函数，也就是HandleRead
        // 但是还没有启动读事件关心在_base_loop上
    }
    void SetAcceptCallback(const AcceptCallback& cb) {
        _accept_callback = cb;
    }
    // 启动listen套接字的读事件关心在_base_loop上
    // 自此，有client连接之后，listen套接字的读事件就绪，就会开始回调（HandleRead)
    void Listen() {
        _channel.EnableRead();
    }
private:
    // 主Reactor线程会执行这个~
    void HandleRead() {
        int newfd = _listen_socket.Accept();
        if(newfd == -1) return ;
        // 具体主Reactor线程获取一个newfd之后，怎么分配到从Reactor线程池，就是_accept_callback的工作了（TcpServer设定）
        if(_accept_callback) _accept_callback(newfd);
    }
    int CreateListenSocket(int port) {
        bool ret = _listen_socket.CreateListenSocket(port);
        assert(ret == true);
        return _listen_socket.Fd();
    }
};
using PtrConnection = std::shared_ptr<Connection>;
class TcpServer
{
private:
    uint64_t _conn_id;   // 自动增长的Connection ID
    int _port;
    int _timeout;
    bool _enable_inactive_release;

    EventLoop _base_loop;    // 主Reactor线程执行_base_loop的Start
    Acceptor _acceptor;
    LoopThreadPool _pool;    // 从属Reactor线程池
    std::unordered_map<uint64_t, PtrConnection> _conns;

    // HandleRead读事件监控回调函数，从TCP内核接收缓冲区中读取数据，放到_in_buffer之后
    // 进行业务处理，业务处理函数即_message_callback，为用户设定的。
    using MessageCallback = std::function<void (const PtrConnection &, Buffer *in_buffer)>;
    using ConnectedCallback = std::function<void (const PtrConnection &)>;
    using CloseCallback = std::function<void (const PtrConnection &)>;
    using AnyEventCallback = std::function<void (const PtrConnection &)>;
    using Functor = std::function<void()>;    // 主线程的定时任务类型
    MessageCallback _message_callback = nullptr;
    ConnectedCallback _connected_callback = nullptr;
    CloseCallback _close_callback = nullptr;
    AnyEventCallback _any_event_callback = nullptr;
public:
    /*if you want to create a Single threaded Reactor mode TCP server
    , please set 0 to the thread_num */
    // 若开启非活跃连接销毁功能，则第三个参数应设定为超时时间(>0)/
    TcpServer(int port, int thread_num = 2, int timeout = -1):
    _conn_id(0),
    _port(port),
    _timeout(0),
    _enable_inactive_release(false),
    _base_loop(),
    _acceptor(&_base_loop, _port),
    _pool(thread_num, &_base_loop) {
        DBG_LOG("test 111");
        // 先进行第一步，获取新连接之后才能分配到从属Reactor线程池中，再启动_base_loop对于listen套接字的读关心
        // 否则在获取新客户端连接之后，无法将新连接分配到从属Reactor线程池中，因为Acceptor的_accept_callback为空~
        _acceptor.SetAcceptCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
        _acceptor.Listen();    // 在base_loop中开启listen套接字的读事件关心
        if(timeout > 0) {
            _timeout = timeout;
            _enable_inactive_release = true;
        }
    }
    // 业务处理函数的设定
    void SetMessageCallback(const MessageCallback &cb) {
        _message_callback = cb;
    }
    // 可有可无，看User的需求~
    void SetConnectedCallback(const ConnectedCallback &cb) {
        _connected_callback = cb;
    }
    void SetCloseCallback(const CloseCallback &cb) {
        _close_callback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback &cb) {
        _any_event_callback = cb;
    }
    // 添加一个定时任务，User可以不用~
    void RunAfter(Functor func, int delay) {
        _base_loop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, func, delay));
    }
    void RunAfterInLoop(Functor func, int delay) {
        // 这里一定是base_loop对应线程在执行这里
        _conn_id++;
        _base_loop.TimerAddInLoop(_conn_id, delay, func);     // yzl~~
    }
    void RemoveConnection(const PtrConnection &conn) {
        // 从属线程在关闭连接时会执行这里~，然后把实际关闭的任务交给主线程~
        _base_loop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
    }
    void RemoveConnectionInLoop(const PtrConnection &conn) {
        // 将Connection从_conns中移除掉
        uint64_t id = conn->Id();
        if(_conns.find(id) == _conns.end()) {
            _conns.erase(id);
        }
    }
    void Start() {
        _base_loop.Start();
        // 主Reactor线程开始eventloop获取新连接
        // 从属Reactor线程在构造时就已经开始eventloop了(但是关心的文件描述符数量为空)
        // 因为主Reactor线程还没开始，所以从属Reactor线程池是安全的~
    }
private:
    // 主Reactor线程获取新连接之后会执行这个~
    void NewConnection(int fd) {
        EventLoop *loop = _pool.GetLoopBalanced();
        PtrConnection conn(new Connection(loop, _conn_id, fd));
        // User设定的业务处理函数，一定需要的
        conn->SetMessageCallback(_message_callback);
        // 可有可无，但是如果User设定了，则需要设定
        if(_connected_callback) {
            conn->SetConnectedCallback(_connected_callback);
        }
        if(_close_callback) {
            conn->SetCloseCallback(_close_callback);
        }
        if(_any_event_callback) {
            conn->SetAnyEventCallback(_any_event_callback);
        }
        conn->SetSvrCloseCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
        if(_enable_inactive_release) conn->EnableInactiveRelease(_timeout);  // 非活跃连接超时自动销毁
        // 到此为止，新连接的事件监控还没有开启
        conn->Establish();
        _conns.insert({_conn_id, conn});
        ++_conn_id;
    }
};
#endif