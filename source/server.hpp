#ifndef __MUDUO_SERVER_HPP__
#define __MUDUO_SERVER_HPP__

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
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
using TaskFunc = std::function<void()>;
class TimerTask
{
private:
    uint64_t _id;       // 任务id
    uint32_t _timeout;  // 定时时长
    using ReleaseTimer = std::function<void()>;
    TaskFunc _task_cb;  // 定时器对象要执行的定时任务
    ReleaseTimer _release; // 用于删除TimerWheel中保存的定时器对象信息
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
    void Ontime() {
        // 根据实际超时的次数，执行对应的超时任务
        int times = ReadTimefd();
        for (int i = 0; i < times; i++) {
            TickOnce();
        }
    }
    // 其实外部使用的就是下面这三个方法:1、添加任务2、刷新任务3、取消任务
    // 我们实现一个时间轮
    // timerfd用epoll监管，到时间，timerfd读就绪，根据读事件回调函数就执行Ontime，从而执行时间轮中任务而已。
private:
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
    // 当一个连接突然关闭，要取消对应的定时任务~
    void TimerCancelInLoop(uint64_t id) {
        auto it = _timers.find(id);
        if(it == _timers.end()) {
            return ;   // 任务不存在
        }
        SharedTask pTask = it->second.lock();
        if(pTask) pTask->Cancel();
    }
public:
    /*定时器中有个_timers成员，定时器数据的操作有可能在多线程中进行，因此需要考虑线程安全问题*/
    /*如果不想加锁，那就把对定时器的所有操作，都放到一个线程中进行*/

    // 下面的三个方法，不能直接执行，因为存在线程安全问题
    // 确保这些方法在EventLoop这个线程中执行就没有线程安全问题了。
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

    using Task = std::function<void()>;
    std::vector<Task> _tasks;// 任务池
    std::mutex _mutex;// 实现任务池操作的线程安全

    // 时间轮定时器模块：用timerfd，每秒读事件就绪一次，epoll检测到，调用读事件处理函数，tick前移，执行某轮子中的任务
    TimerWheel _timer_wheel;  // 定时器模块
public:
    EventLoop():
    _thread_id(std::this_thread::get_id()), 
    _event_fd(CreateEventFd()), 
    _event_channel(new Channel(this, _event_fd)),
    _timer_wheel(this) {
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this));
        _event_channel->EnableRead();
        // 将eventfd的读事件监督交给epoll(poller)，这样可以通过WriteEventfd来唤醒阻塞在epoll_wait中的epoll线程。
    }
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
    // 判断将要执行的任务是否处于当前线程中，如果是则执行，不是则压入队列。
    void RunInLoop(const Task &cb) {
        if (IsInLoop()) {
            return cb();
        }
        return PushInTasks(cb);
    }
    // 用于判断当前线程是否是EventLoop对应的线程；
    bool IsInLoop() {
        return (_thread_id == std::this_thread::get_id());
    }
    // 将操作压入任务池
    void PushInTasks(const Task &cb) {
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
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
        return _timer_wheel.TimerAdd(id, delay, cb);
    }
    void TimerRefresh(uint64_t id) {
        return _timer_wheel.TimerRefresh(id);
    }
    void TimerCancel(uint64_t id) { 
        return _timer_wheel.TimerCancel(id); 
    }
    bool HasTimer(uint64_t id) {
        return _timer_wheel.HasTimer(id);
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
#endif