// timerfd
// timerfd 这个名字拆开来看，就是 timer fd，所谓定时器 fd 类型
// 那么它的可读可写事件一定是跟时间有关系。
// timerfd 被 new 出来之后 （ timerfd_create ），可以设置超时时间（ timerfd_setting ）
// 超时之后，该句柄可读，读出来的是超时的次数。(距离上一次读取到这一次读取，一共超时了几次)
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/timerfd.h>

int main()
{
    //int timerfd_create(int clockid, int flags);
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);   // 创建timerfd
    if (timerfd < 0) {
        perror("timerfd_create error");
        return -1;
    }

    //int timerfd_settime(int fd, int flags, struct itimerspec *new, struct itimerspec *old);
    struct itimerspec itime;
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0;//第一次超时时间为1s后
    itime.it_interval.tv_sec = 1; 
    itime.it_interval.tv_nsec = 0; //第一次超时后，每次超时事件间隔
    timerfd_settime(timerfd, 0, &itime, NULL);

    sleep(3);
    uint64_t times;
    int ret = read(timerfd, &times, 8);  // 文件内部存储的8字节无符号整数，必须读取8字节
    if (ret < 0) {
        perror("read error");
        return -1;
    }
    printf("超时了，距离上一次超时了%ld次\n", times);

    while(1) {
        sleep(2);
        int ret = read(timerfd, &times, 8);  // 文件内部存储的8字节无符号整数，必须读取8字节
        if (ret < 0) {
            perror("read error");
            return -1;
        }
        printf("超时了，距离上一次超时了%ld次\n", times);
    }
    close(timerfd);
    return 0;
}