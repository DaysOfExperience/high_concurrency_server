#include "../server.hpp"

class EchoServer
{
private:
    TcpServer _server;
public:
    EchoServer(int port) :
    _server(port, 4) {
        _server.SetMessageCallback(std::bind(&EchoServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void Start() {
        _server.Start();
    }
private:
    void OnMessage(const PtrConnection &conn, Buffer *in_buffer) {
        // 调用发送接口进行echo回应
        conn->Send(in_buffer->ReadPosition(), in_buffer->ReadableSize());
        in_buffer->MoveReadOffset(in_buffer->ReadableSize());   // 啊这..
    }
};