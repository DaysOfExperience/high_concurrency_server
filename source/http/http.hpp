#include "../server.hpp"
#include <regex>
std::unordered_map<int, std::string> _status_msg = {
    {100,  "Continue"},
    {101,  "Switching Protocol"},
    {102,  "Processing"},
    {103,  "Early Hints"},
    {200,  "OK"},
    {201,  "Created"},
    {202,  "Accepted"},
    {203,  "Non-Authoritative Information"},
    {204,  "No Content"},
    {205,  "Reset Content"},
    {206,  "Partial Content"},
    {207,  "Multi-Status"},
    {208,  "Already Reported"},
    {226,  "IM Used"},
    {300,  "Multiple Choice"},
    {301,  "Moved Permanently"},
    {302,  "Found"},
    {303,  "See Other"},
    {304,  "Not Modified"},
    {305,  "Use Proxy"},
    {306,  "unused"},
    {307,  "Temporary Redirect"},
    {308,  "Permanent Redirect"},
    {400,  "Bad Request"},
    {401,  "Unauthorized"},
    {402,  "Payment Required"},
    {403,  "Forbidden"},
    {404,  "Not Found"},
    {405,  "Method Not Allowed"},
    {406,  "Not Acceptable"},
    {407,  "Proxy Authentication Required"},
    {408,  "Request Timeout"},
    {409,  "Conflict"},
    {410,  "Gone"},
    {411,  "Length Required"},
    {412,  "Precondition Failed"},
    {413,  "Payload Too Large"},
    {414,  "URI Too Long"},
    {415,  "Unsupported Media Type"},
    {416,  "Range Not Satisfiable"},
    {417,  "Expectation Failed"},
    {418,  "I'm a teapot"},
    {421,  "Misdirected Request"},
    {422,  "Unprocessable Entity"},
    {423,  "Locked"},
    {424,  "Failed Dependency"},
    {425,  "Too Early"},
    {426,  "Upgrade Required"},
    {428,  "Precondition Required"},
    {429,  "Too Many Requests"},
    {431,  "Request Header Fields Too Large"},
    {451,  "Unavailable For Legal Reasons"},
    {501,  "Not Implemented"},
    {502,  "Bad Gateway"},
    {503,  "Service Unavailable"},
    {504,  "Gateway Timeout"},
    {505,  "HTTP Version Not Supported"},
    {506,  "Variant Also Negotiates"},
    {507,  "Insufficient Storage"},
    {508,  "Loop Detected"},
    {510,  "Not Extended"},
    {511,  "Network Authentication Required"}
};

std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac",        "audio/aac"},
    {".abw",        "application/x-abiword"},
    {".arc",        "application/x-freearc"},
    {".avi",        "video/x-msvideo"},
    {".azw",        "application/vnd.amazon.ebook"},
    {".bin",        "application/octet-stream"},
    {".bmp",        "image/bmp"},
    {".bz",         "application/x-bzip"},
    {".bz2",        "application/x-bzip2"},
    {".csh",        "application/x-csh"},
    {".css",        "text/css"},
    {".csv",        "text/csv"},
    {".doc",        "application/msword"},
    {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot",        "application/vnd.ms-fontobject"},
    {".epub",       "application/epub+zip"},
    {".gif",        "image/gif"},
    {".htm",        "text/html"},
    {".html",       "text/html"},
    {".ico",        "image/vnd.microsoft.icon"},
    {".ics",        "text/calendar"},
    {".jar",        "application/java-archive"},
    {".jpeg",       "image/jpeg"},
    {".jpg",        "image/jpeg"},
    {".js",         "text/javascript"},
    {".json",       "application/json"},
    {".jsonld",     "application/ld+json"},
    {".mid",        "audio/midi"},
    {".midi",       "audio/x-midi"},
    {".mjs",        "text/javascript"},
    {".mp3",        "audio/mpeg"},
    {".mpeg",       "video/mpeg"},
    {".mpkg",       "application/vnd.apple.installer+xml"},
    {".odp",        "application/vnd.oasis.opendocument.presentation"},
    {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt",        "application/vnd.oasis.opendocument.text"},
    {".oga",        "audio/ogg"},
    {".ogv",        "video/ogg"},
    {".ogx",        "application/ogg"},
    {".otf",        "font/otf"},
    {".png",        "image/png"},
    {".pdf",        "application/pdf"},
    {".ppt",        "application/vnd.ms-powerpoint"},
    {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar",        "application/x-rar-compressed"},
    {".rtf",        "application/rtf"},
    {".sh",         "application/x-sh"},
    {".svg",        "image/svg+xml"},
    {".swf",        "application/x-shockwave-flash"},
    {".tar",        "application/x-tar"},
    {".tif",        "image/tiff"},
    {".tiff",       "image/tiff"},
    {".ttf",        "font/ttf"},
    {".txt",        "text/plain"},
    {".vsd",        "application/vnd.visio"},
    {".wav",        "audio/wav"},
    {".weba",       "audio/webm"},
    {".webm",       "video/webm"},
    {".webp",       "image/webp"},
    {".woff",       "font/woff"},
    {".woff2",      "font/woff2"},
    {".xhtml",      "application/xhtml+xml"},
    {".xls",        "application/vnd.ms-excel"},
    {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml",        "application/xml"},
    {".xul",        "application/vnd.mozilla.xul+xml"},
    {".zip",        "application/zip"},
    {".3gp",        "video/3gpp"},
    {".3g2",        "video/3gpp2"},
    {".7z",         "application/x-7z-compressed"}
};
class Util
{

};

class HttpRequest
{
public:
    std::string _method;   // 请求方法
    std::string _path;     // 资源路径
    std::string _version;  // 协议版本
    std::string _body;     // 请求正文
    std::smatch _matches;     // 资源路径的正则提取数据
    std::unordered_map<std::string, std::string> _headers;        // 头部字段
    std::unordered_map<std::string, std::string> _query_string;   // 查询字符串
public:
    HttpRequest() {}
    // void Reset() {
    // }
    void SetHeader(const std::string &key, const std::string &value) {
        _headers.insert({key, value});
    }
    bool HasHeader(const std::string &key) {
        if(_headers.find(key) == _headers.end()) {
            return false;
        }
        return true;
    }
    std::string GetHeader(const std::string &key) {
        auto it = _headers.find(key);
        if(it == _headers.end()) {
            return "";
        }
        return it->second;
    }
    void SetQueryString(const std::string &key, const std::string &value) {
        _query_string.insert(std::make_pair(key, value));
    }
    bool HasQueryString(const std::string &key) {
        if(_query_string.find(key) == _query_string.end()) {
            return false;
        }
        return true;
    }
    std::string GetQueryString(const std::string &key) {
        auto it = _query_string.find(key);
        if(it == _query_string.end()) {
            return "";
        }
        return it->second;
    }
    // 获取HTTP请求的正文长度~
    size_t ContentLength() {
        if(_headers.find("content-length") == _headers.end()) {   // 其实有接口了，也就是HasHeader = =
            return 0;
        }
        return std::stoul(_headers["content-length"]);   // yzl~~
    }
    // 判断是否是短连接
    // bool Close() {
    // }
};
// 媒体类型（通常称为 Multipurpose Internet Mail Extensions 或 MIME 类型）是一种标准，用来表示文档、文件或字节流的性
// 质和格式。它在IETF RFC 6838中进行了定义和标准化。

// 互联网号码分配机构（IANA）是负责跟踪所有官方 MIME 类型的官方机构，您可以在媒体类型页面中找到最新的完整列表。

// 警告： 浏览器通常使用 MIME 类型（而不是文件扩展名）来确定如何处理 URL，因此 Web 服务器在响应头中添加正确的
// MIME 类型非常重要。如果配置不正确，浏览器可能会曲解文件内容，网站将无法正常工作，并且下载的文件也会被错误处理。
class HttpResponse
{
public:
    std::string _version;       // 协议版本
    int _status;                // 状态码
    std::string _status_desc;   // 状态码描述
    std::string _body;          // 请求正文
    bool _redirect_flag;        // 是否重定向
    std::string _redirect_url;  // 重定向URL(统一资源定位器)
    std::unordered_map<std::string, std::string> _headers;        // 头部字段
public:
    HttpResponse() {}
    HttpResponse(int status) : _status(status) {
    }
    void Reset() {
    }
    void SetHeader(const std::string &key, const std::string &value) {
        _headers.insert({key, value});
    }
    bool HasHeader(const std::string &key) {
        if(_headers.find(key) == _headers.end()) {
            return false;
        }
        return true;
    }
    std::string GetHeader(const std::string &key) {
        auto it = _headers.find(key);
        if(it == _headers.end()) {
            return "";
        }
        return it->second;
    }
    // // 设置正文，以及正文类型（设置在HTTP响应header中）
    // void SetContent(const std::string &body, const std::string &type = "text/html") {

    // }
    // void SetRedirect(const std::string &url, int status = 302) {

    // }
    // // 判断是否是短连接
    // bool Close() {
    // }
};
// 因为TCP面向字节流，因此在接收缓冲区中的HTTP请求可能不足一个完成的报文
// 因此我们需要按照HTTP的格式解析已经收到的HTTP请求报文（可能是一部分），若只解析了一部分，比如请求头
// 则返回，等待下次接收缓冲区有数据，再进一步解析
// 此类就是一个HTTP Request的解析类
typedef enum {
    RECV_HTTP_ERROR,  // 接收解析Request错误
    RECV_HTTP_LINE,   // 接收并解析请求首行的阶段
    RECV_HTTP_HEAD,   // 接收并解析请求Header的阶段
    RECV_HTTP_BODY,   // 接收请求正文的阶段
    RECV_HTTP_OVER    // 接收并解析完毕
}HttpRecvStatus;

// 在Connection类中有一个上下文字段，类型为Any，用于解析客户端发来的应用层报文
// 若应用层协议为HTTP协议，则该Any类型的上下文字段就应该存储HttpContext类型对象
#define MAX_LINE 8192
class HttpContext
{
private:
    // 在解析HttpRequest的过程中，可能出现各种错误情况，比如414; // URI TOO LONG  400;  //BAD REQUEST
    // 所以，
    int _resp_status;             // Response响应状态码
    HttpRecvStatus _recv_status;  // 当前接收并解析的阶段状态
    HttpRequest _request;         // 已经解析得到的Http请求信息
public:
    HttpContext(): _resp_status(200), _recv_status(RECV_HTTP_LINE) {
    }
    void Reset() {

    }
    int RespStatus() {

    }
    HttpRecvStatus RecvStatus() {

    }
    HttpRequest &Request() {
    }
    // Buffer? : void OnMessage(const PtrConnection &conn, Buffer *buffer)
    //           业务处理函数，是当将内核TCP接收缓冲区读取到应用层_in_buffer中后
    //           进行业务处理时，首先需要从_in_buffer中解析出HTTP request，因此，参数是Buffer *
    // 对接收缓冲区中的http字符串进行解析
    void RecvHttpRequest(Buffer *buffer) {
        switch(_recv_status) {
            case RECV_HTTP_LINE:
                RecvHttpLine(buffer);
            case RECV_HTTP_HEAD:
                RecvHttpHeader(buffer);
            case RECV_HTTP_BODY:
                RecvHttpBody(buffer);
        }
        return ;
    }
private:
    bool RecvHttpLine(Buffer *buffer) {
        if(_recv_status != RECV_HTTP_LINE) {
            return false;
        }
        // 1. 从接收缓冲区中获取一行数据，带有末尾的换行
        std::string line = buffer->Getline();
        // 2. 可能的情况
        if(line.size() == 0) {
            // 缓冲区中的数据不足一行，则需要判断缓冲区的可读数据长度，如果很长了都不足一行，这是有问题的
            if(buffer->ReadableSize() > MAX_LINE) {  // 8KB
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 414; // URI TOO LONG
                return false;
            }
            // 不足一行，但是比较短，也就是正常情况，只是请求行没有接收完毕
            return true;
        }
        if(line.size() > MAX_LINE) {   // 8KB
            // 确实够了一行，但是还是过长
            _recv_status = RECV_HTTP_ERROR;
            _resp_status = 414; // URI TOO LONG
            return false;
        }
        // 接收到了一行长度正常的请求行
        bool ret = ParseHttpLine(line);
        if(ret == false) {
            return false;
        }
        // 首行处理完毕，进入头部获取解析阶段
        _recv_status = RECV_HTTP_HEAD;
        return true;
    }
    bool ParseHttpLine(const std::string line) {
        // line 为一个HTTP request的请求行，需要将其进行解析，放入_request中
        std::smatch matches;
        std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        bool ret = std::regex_match(line, matches, e);
        if (ret == false) {
            _recv_status = RECV_HTTP_ERROR;
            _resp_status = 400;  //BAD REQUEST
            return false;
        }
        // 下面是通过正则表达式将请求头字符串进行解析之后得到的结果，在matches中，前方为下标，后方为内容
        //0 : GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1
        //1 : GET
        //2 : /bitejiuyeke/login
        //3 : user=xiaoming&pass=123123
        //4 : HTTP/1.1
        // 请求方法
        _request._method = matches[1];
        // 请求方法可能为小写，需要进行转换
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper);
        // 资源路径，需要进行URL解码!!!
        _request._path = Util::UrlDecode(matches[2], false);
        // 协议版本
        _request._version = matches[4];
        // 查询字符串的获取与处理  matches[3]
        std::string query_string = matches[3];
        std::vector<std::string> query_string_array;
        Util::Split(query_string, "&", &query_string_array);
        for(auto & s : query_string_array) {
            // xxx=yyy
            auto pos = s.find("=");
            if(pos == std::string::npos) {
                // 此查询字符串有问题
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 400;//BAD REQUEST
                return false;
            }
            auto left = Util::UrlDecode(s.substr(0, pos), true);
            auto right = Util::UrlDecode(s.substr(pos + 1), true);
            _request.SetQueryString(left, right);   // 一个查询字符串提取出来了
        }
        return true;
    }
    bool RecvHttpHeader(Buffer *buffer) {
        if(_recv_status != RECV_HTTP_HEAD) {
            return false;
        }
        // 一行一行取出数据，直到遇到空行为止， 头部的格式 key: val\r\nkey: val\r\n....
        while(1) {
            // 1. 从接收缓冲区中获取一行数据，带有末尾的换行
            std::string line = buffer->Getline();
            // 2. 可能的情况
            if(line.size() == 0) {
                // 缓冲区中的数据不足一行，则需要判断缓冲区的可读数据长度，如果很长了都不足一行，这是有问题的
                if(buffer->ReadableSize() > MAX_LINE) {  // 8KB
                    _recv_status = RECV_HTTP_ERROR;
                    _resp_status = 414; // URI TOO LONG
                    return false;
                }
                // 不足一行，但是比较短，也就是正常情况，只是请求行没有接收完毕
                return true;
            }
            if(line.size() > MAX_LINE) {   // 8KB
                // 确实够了一行，但是还是过长
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 414; // URI TOO LONG
                return false;
            }
            // 接收到了一行长度正常的请求行
            if(line == "\r\n" || line == "\n") {
                break;
            }
            bool ret = ParseHttpHeader(line);   // 对这一行进行解析，放入_request中
            if(ret == false) {
                return false;
            }
        }
        _recv_status = RECV_HTTP_BODY;   // http的头部已经空行都处理完毕，接下来接收body即可
        return true;
    }
    bool ParseHttpHeader(std::string line) {
        //key: val\r\n
        if(line.back() == '\n') line.pop_back();
        if(line.back() == '\r') line.pop_back();
        // xxx: val
        auto pos = line.find(": ");
        if(pos == std::string::npos) {
            // 此header的某一行有问题
            _recv_status = RECV_HTTP_ERROR;
            _resp_status = 400;//BAD REQUEST
            return false;
        }
        auto left = line.substr(0, pos);
        auto right = line.substr(pos + 1);
        _request.SetHeader(left, right);   // 一个查询字符串提取出来了
        return true;
    }
    bool RecvHttpBody(Buffer *buffer) {
        if(_recv_status != RECV_HTTP_BODY) {
            return false;
        }
        // 注意，此时不一定是第一次执行~
        size_t length = _request.ContentLength();
        // 若HTTP请求没有正文，直接处理结束
        if(length == 0) {
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        // 有正文，第一次处理正文，也可能不是第一次，也就是_request._body中已经有了之前接收的一部分正文
        size_t real_length = length - _request._body.size();   // 可能已经接收一部分了
        if(buffer->ReadableSize() >= real_length) {
            _request._body.append(buffer->ReadAsString(real_length));
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        else {
            // 接收缓冲区中剩余长度不足
            // 则全部读出
            _request._body.append(buffer->ReadAsString(buffer->ReadableSize()));
            return true;
        }
    }
};
#define DEFALT_TIMEOUT 10
class HttpServer
{
private:
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;  // /numbers/(\d+) : 回调方法
    Handlers _get_route;
    Handlers _post_route;
    Handlers _put_route;
    Handlers _delete_route;
    TcpServer _server;
    std::string _basedir;    // 静态资源根目录
public:
    HttpServer(int port, int thread_num = 2, int timeout = DEFALT_TIMEOUT) :
        _server(port, thread_num, timeout) {
        // std::function<void (const PtrConnection &, Buffer *in_buffer)>
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
private:
    // 实际上，当server的某通信TCP套接字读事件就绪时，会读对端发送的数据到Connection的_in_buffer中
    // （对于Http server来说，就是一个http request字符串），我们需要进行业务处理，下方OnMessage方法就是业务处理方法
    void OnMessage(const PtrConnection &conn, Buffer *in_buffer) {
        // 1. 获取上下文，指针指向Connection的_context字段
        HttpContext *context = conn->GetContext()->get<HttpContext>();
        // 2. 通过上下文对接收缓冲区中的数据进行解析，得到HttpRequest对象
        context->RecvHttpRequest(in_buffer);
        HttpRequest &req = context->Request();   // 当前解析出的Request
        HttpResponse rsp(context->RespStatus()); // 进行响应的Response，目前只有_status状态码字段被设置了
        //  2.1 如果缓冲区的数据解析出错，就直接回复出错响应
        if (context->RespStatus() >= 400) {
            //进行错误响应，关闭连接
            ErrorHandler(req, &rsp);     //填充一个错误显示页面数据到rsp中
            WriteResponse(conn, req, rsp);//组织响应发送给客户端
            // 注意，此上下文实际上是存储在Connection的内部的，所以我们需要进行清理。但是，后面要关闭连接，也就无所谓了其实
            context->Reset();
            in_buffer->MoveReadOffset(in_buffer->ReadableSize());//出错了就把缓冲区数据清空
            conn->Shutdown();//关闭连接
            return ;
        }
        if (context->RecvStatus() != RECV_HTTP_OVER) {
            // 当前还没有解析出一个完成的http请求报文，等新数据到来再重新继续处理
            return ;
        }
        //  2.2 如果解析正常，且请求已经获取完毕，才开始去进行处理
        // 此时有了一个完整的Http请求了
        // 3. 请求路由 + 业务处理
        Route(req, &rsp);
        // 4. 获取到一个Response，进行发送
        WriteReponse(conn, req, rsp);
        // 5. 重置上下文，以免对下次该连接的Http处理产生影响
        context->Reset();
        // 6. 根据长短连接判断是否关闭连接/继续处理
        if (rsp.Close() == true) {
            conn->Shutdown();  // 短链接则直接关闭
        }
    }
    void WriteResponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse &rsp) {
        //1. 先完善头部字段
        if (req.Close() == true) {
            rsp.SetHeader("Connection", "close");
        }else {
            rsp.SetHeader("Connection", "keep-alive");
        }
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Length") == false) {
            rsp.SetHeader("Content-Length", std::to_string(rsp._body.size()));
        }
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Type") == false) {
            rsp.SetHeader("Content-Type", "application/octet-stream");
        }
        if (rsp._redirect_flag == true) {
            rsp.SetHeader("Location", rsp._redirect_url);
        }
        //2. 将rsp中的要素，按照http协议格式进行组织
        std::stringstream rsp_str;
        rsp_str << req._version << " " << std::to_string(rsp._statu) << " " << Util::StatuDesc(rsp._statu) << "\r\n";
        for (auto &head : rsp._headers) {
            rsp_str << head.first << ": " << head.second << "\r\n";
        }
        rsp_str << "\r\n";
        rsp_str << rsp._body;
        // 进行发送
        conn->Send(rsp_str.str().c_str(), rsp_str.str().size());
    }
    // DisPatcher中会对Request进行进行修改：_matches
    void Route(HttpRequest &req, HttpResponse *rsp) {
        // 对请求进行分辨，是一个静态资源请求，还是一个功能性请求
        // 静态资源请求，则进行静态资源的处理
        // 功能性请求，则需要通过几个请求路由表来确定是否有处理函数
        // 既不是静态资源请求，也没有设置对应的功能性请求处理函数，就返回405

        // 判断是否是在请求一个合理的存在的静态资源，若是，则进行读取静态资源
        if(IsFileHandler(req) == true) {
            // 此时，是在请求一个静态资源，且静态资源存在，且合法，且有静态资源根目录....
            // 进行读取静态资源到HttpResponse的正文中？
            FileHandler(req, rsp);
        }
        // 请求的不是静态资源
        if (req._method == "GET" || req._method == "HEAD") {
            return Dispatcher(req, rsp, _get_route);
        }else if (req._method == "POST") {
            return Dispatcher(req, rsp, _post_route);
        }else if (req._method == "PUT") {
            return Dispatcher(req, rsp, _put_route);
        }else if (req._method == "DELETE") {
            return Dispatcher(req, rsp, _delete_route);
        }
    }
    bool IsFileHandler(const HttpRequest &req) {
        // 1. HttpServer设置了静态资源根目录，才有可能访问到静态资源
        if (_basedir.empty()) {
            return false;
        }
        // 2. 请求方法，必须是GET / HEAD请求方法
        if (req._method != "GET" && req._method != "HEAD") {
            return false;
        }
        // 3. 请求的资源路径必须是一个合法路径
        if (Util::ValidPath(req._path) == false) {
            return false;
        }
        // 4. 请求的资源必须存在,且是一个普通文件
        // (有一种请求比较特殊 -- 目录：/, /image/， 这种情况给后边默认追加一个 index.html)
        // index.html    /image/a.png
        // 不要忘了前缀的相对根目录,也就是将请求路径转换为实际存在的路径  /image/a.png  ->   ./wwwroot/image/a.png
        std::string req_path = _basedir + req._path; // 为了避免直接修改请求的资源路径，因此定义一个临时对象
        if (req._path.back() == '/')  {
            req_path += "index.html";
        }
        if (Util::IsRegular(req_path) == false) {
            return false;
        }
        return true;
    }
    // 静态资源的请求处理 --- 将静态资源文件的数据读取出来，放到rsp的_body中, 并设置mime
    void FileHandler(const HttpRequest &req, HttpResponse *rsp) {
        // 此时的req._path依旧可能为一个"/"，且没有添加前缀的相对根目录路径，需要我们处理一下
        std::string req_path = _basedir + req._path;
        if(req._path.back() == '/') {
            req_path += "index.html";
        }
        // req_path即需要访问的静态资源
        Util::ReadFile(req_path, &rsp->_body);   // Response的正文好了
        rsp->SetHeader("Content-Type", Util::ExtMime(req_path));  // Response的mime好了
    }
    void Dispatcher(HttpRequest &req, HttpResponse *rsp, Handlers &handlers) {
        // 在对应请求方法的路由表中，查找是否含有对应资源请求的处理函数，有则调用，没有则返回404
        // 思想：路由表存储的是键值对 -- 正则表达式 : 处理函数
        // 使用正则表达式，对请求的资源路径进行正则匹配，匹配成功就使用对应函数进行处理
        // 路由表中存储的是：/numbers/(\d+)      而Request的_path存储的是：/numbers/12345
        for (auto &handler : handlers) {
            const std::regex &re = handler.first;
            const Handler &functor = handler.second;
            bool ret = std::regex_match(req._path, req._matches, re);
            if (ret == false) {
                continue;  // 匹配失败，请求的不是这个资源
            }
            return functor(req, rsp); // 传入请求信息，和空的rsp，执行处理函数
        }
        rsp->_status = 404;   // Not Found
    }
};