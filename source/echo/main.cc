#include "echo.hpp"

int main()
{
    EchoServer es(8080);
    es.Start();
    return 0;
}