#include "broker.h"

#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <signal.h>

#include <unistd.h>


pobj_loop *looper;

pnet_broker *broker;
bool broker_can_send = false;

static void func_SIGINT()
{
    plog_info("Got SIGINT");
    looper->broken = 1;
}

static void func_SIGQUIT()
{
    plog_info("Got SIGQUIT");
}

static void parse_client_msg()
{
    plog_info("Got client msg");
}

static void parse_worker_msg()
{
    plog_info("Got worker msg");
}

static void func_net_event(pobj_loop* UNUSED(loop), const puint32 epoll_events)
{
    if (epoll_events & (POBJIN | POBJOUT)) {
        pint32 net_event = pnet_broker_check_event(broker);
        if (net_event & POBJIN) {
            //aaa += 10;
        }
        if (net_event & POBJOUT) {
            broker_can_send = true;
            // ToDo: check query
        }
    } else {
        plog_error("Net error!");
    }
}

void broker_main_loop()
{
    looper = pobj_create(128, false);

    if (!pobj_signals_add(looper, SIGINT, func_SIGINT) || (!pobj_signals_add(looper, SIGQUIT, func_SIGQUIT))) {
        plog_error("signal error");
    }

    if (!pobj_signals_start(looper)) {
        plog_error("signal error");
    }




    pnet_broker_start(&broker, "tcp://127.0.0.1:12345");
    pnet_broker_register(broker, looper, func_net_event);




    pobj_run(looper);





    pnet_broker_stop(&broker);




    pobj_signals_stop(looper);

    pobj_destroy(&looper);
}
