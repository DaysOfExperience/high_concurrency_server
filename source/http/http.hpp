#include "../server.hpp"
#include <regex>
std::unordered_map<int, std::string> _statu_msg = {
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

    std::unordered_map<std::string, std::string> _headers;        // 头部字段
    std::unordered_map<std::string, std::string> _query_string;   // 查询字符串
public:
    HttpRequest() {}
    void Reset() {
    }
    void SetHeader(const std::string &key, const std::string &value) {
    }
    bool HasHeader(const std::string &key) {
    }
    std::string GetHeader(const std::string &key) {
    }
    void SetQueryString(const std::string &key, const std::string &value) {
    }
    bool HasQueryString(const std::string &key) {
    }
    std::string GetQueryString(const std::string &key) {
    }
    // 获取HTTP请求的正文长度~
    size_t ContentLength() {
        if(_headers.find("content-length") == _headers.end()) {   // 其实有接口了，也就是HasHeader = =
            return 0;
        }
        return std::stoul(_headers["content-length"]);   // yzl~~
    }
    // 判断是否是短连接
    bool Close() {
    }
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
    void Reset() {
    }
    void SetHeader(const std::string &key, const std::string &value) {
    }
    bool HasHeader(const std::string &key) {
    }
    std::string GetHeader(const std::string &key) {
    }
    // 设置正文，以及正文类型（设置在HTTP响应header中）
    void SetContent(const std::string &body, const std::string &type = "text/html") {

    }
    void SetRedirect(const std::string &url, int status = 302) {

    }
    // 判断是否是短连接
    bool Close() {
    }
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

#define MAX_LINE 8192
class HttpContext
{
private:
    int _resp_status;             // 响应状态码
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
    //           进行业务处理，需要从中解析出HTTP request，因此，参数是Buffer
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
        // line 为一个HTTP request的请求头，需要将其进行解析，放入_request中
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