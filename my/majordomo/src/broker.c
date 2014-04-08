#include "broker.h"

#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <signal.h>

#include <unistd.h>

static void func(pobj_loop* loop, const puint32 epoll_events)
{
    plog_warn("....... %x | %u", loop, epoll_events);
    pobj_internal_timer_stop(loop);
}

void broker_main_loop()
{

    struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };

    pobj_loop *looper = pobj_create(128, false);
    pobj_internal_timer_start(looper, 1, ts, func);

    pobj_run(looper);

    pobj_destroy(&looper);
}
