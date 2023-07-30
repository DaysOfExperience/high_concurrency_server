#include <iostream>
#include <functional>
class Test
{
public:
    std::function<void()> GetFunc() {
        return std::bind(&Test::Run, this);
    }
private:
    std::function<void()> func;
    void Run() {
        std::cout << "Run In Privte" << std::endl;
    }
};

int main()
{
    Test t;
    std::function<void()> func = t.GetFunc();
    func();
    return 0;
}