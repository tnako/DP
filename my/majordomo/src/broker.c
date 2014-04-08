#include "broker.h"

#include <sys/timerfd.h>


int timerfd;

static void func()
{
    plog_warn(".......");
}

void broker_main_loop()
{
    struct itimerspec new_value;
    struct timespec now;

    if (clock_gettime(CLOCK_REALTIME, &now) == -1) {
                 perror("clock_gettime");
    }

    new_value.it_value.tv_sec = now.tv_sec + 1;
    new_value.it_value.tv_nsec = now.tv_nsec;
    new_value.it_interval.tv_sec = 0;

    int timerfd = timerfd_create(CLOCK_REALTIME, 0);
    if (timerfd) {
        perror("timerfd_create:");
    }
    int res = timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &new_value, 0);
    if(res < 0) {
        perror("timerfd_settime:");
    }

    pobj_loop *looper = pobj_create(128, false);
    pobj_register_event(looper, func, timerfd, EPOLLIN);
    plog_warn("!!!!!");

    pobj_run(looper);
}
