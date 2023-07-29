#ifndef __MUDUO_SERVER_HPP__
#define __MUDUO_SERVER_HPP__

#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <unistd.h>

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

    }
    bool Bind() {

    }
    bool Listen() {

    }
    // 客户端向服务端发起TCP连接
    bool Connect() {

    }
    // 服务端获取建立好的TCP连接
    int Accept() {

    }
    ssize_t Recv() {

    }
    ssize_t RecvNonBlock() {

    }
    ssize_t Send() {

    }
    ssize_t SendNonBlock() {

    }
    bool CreateServerSocket() {

    }
    bool CreateClientSocket() {

    }
    // 服务端开启ip和port的地址重用(TIME_WAIT)
    void ReuseAddress() {

    }
    // 设置套接字文件为非阻塞（若没有数据则出错返回）
    void NonBlock() {

    }
    void Close() {
        if(_sockfd >= 0)
            close(_sockfd);
    }
};

#endif