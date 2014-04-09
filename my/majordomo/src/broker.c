#include "broker.h"

#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <signal.h>

#include <unistd.h>


pobj_loop *looper;

static void func(pobj_loop* loop, const puint32 epoll_events)
{
    plog_warn("....... %x | %u", loop, epoll_events);
    static int test = 0;
    if (test++ > 5) {
        pobj_internal_timer_stop(loop);
        test = 0;
    }
}

static void func_SIGINT()
{
    plog_info("Got SIGINT");
    looper->broken = 1;

}

static void func_SIGQUIT()
{
    plog_info("Got SIGQUIT");

}

void broker_main_loop()
{
    struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };

    looper = pobj_create(128, false);
    pobj_internal_timer_start(looper, 1, ts, func);

    if (!pobj_signals_add(looper, SIGINT, func_SIGINT) || (!pobj_signals_add(looper, SIGQUIT, func_SIGQUIT))) {
        plog_error("signal error");
    }
    if (!pobj_signals_start(looper)) {
        plog_error("signal error");
    }

    pobj_run(looper);
    pobj_signals_stop(looper);

    pobj_destroy(&looper);
}
