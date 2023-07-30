#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unistd.h>

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
        _release();    // 自动执行TimerWheel中的RemoveTimer，从timers中删除对应的定时器(weak_ptr)
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

class TimerWheel
{
private:
    using SharedTask = std::shared_ptr<TimerTask>;
    using WeakTask = std::weak_ptr<TimerTask>;
    int _size;
    int _tick;
    std::vector<std::vector<SharedTask>> _timerwheel;
    std::unordered_map<uint64_t, WeakTask> _timers;
    void RemoveTimer(uint64_t id) {
        auto it = _timers.find(id);
        if (it != _timers.end()) {
            _timers.erase(it);
        }
    }
public:
    TimerWheel(int size = 60)
    : _size(size), _timerwheel(size), _tick(0) {
    }
    ~TimerWheel() {}
    // 一个新的TCP连接，一个Timer定时任务，时间到了，代表超时，关闭非活跃连接
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
        SharedTask pTask(new TimerTask(id, delay, cb));
        pTask->SetReleaseTimer(std::bind(&TimerWheel::RemoveTimer, this, id));
        int pos = (_tick + delay) % _size;   // 这个任务放入的时间轮的位置
        _timerwheel[pos].push_back(pTask);
        _timers.insert({id, WeakTask(pTask)});   // 利用shared_ptr构造weak_ptr?
    }
    // 当一个TCP连接事件就绪，代表活跃，刷新定时任务，重新计时
    void TimerRefresh(uint64_t id) {
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
    void TimerCancel(uint64_t id) {
        auto it = _timers.find(id);
        if(it == _timers.end()) {
            return ;   // 任务不存在
        }
        SharedTask pTask = it->second.lock();
        if(pTask) pTask->Cancel();
    }
    void TickOnce() {
        _tick += 1;
        _tick %= _size;
        // 销毁这个轮子上的所有智能指针，若引用计数变为0，则执行TimerTask的析构函数，执行定时任务（若没有cancel）
        _timerwheel[_tick].clear();
    }
};

void Task() {
    std::cout << "Task!!!!" << std::endl;
}

int main()
{
    TimerWheel tw;
    tw.TimerAdd(1, 5, Task);
    sleep(3);
    int n = 0;
    while(1) {
        sleep(1);
        ++n;
        if(n == 3) tw.TimerRefresh(1);
        std::cout << "tick..." << std::endl;
        tw.TickOnce();
    }
    return 0;
}