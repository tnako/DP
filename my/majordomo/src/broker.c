#include "broker.h"


pobj_loop *looper;
pnet_broker *broker;


static void func_signals()
{
    plog_info("Got signal");
    looper->broken = 1;
}

static void func_net_event()
{
    while (pnet_broker_readmsg(broker)) {
        //plog_info("Data IN!");
    }
}

static void timer_check_workers()
{
    //pnet_broker_purge_workers(broker);
    plog_dbg("timer");
}

void broker_main_loop()
{
    looper = pobj_create(128, false);

    if (!pobj_signals_add(looper, SIGINT, func_signals) || (!pobj_signals_add(looper, SIGQUIT, func_signals))) {
        plog_error("signal error");
    }

    if (!pobj_signals_start(looper)) {
        plog_error("signal error");
    }




    pnet_broker_start(&broker, "tcp://127.0.0.1:12345");
    pnet_broker_register(broker, looper, func_net_event);


    struct timespec time = { .tv_sec = 60, .tv_nsec = 0 };
    pobj_internal_timer_start(looper, 1, time, timer_check_workers);



    pobj_run(looper);





    pnet_broker_stop(&broker);




    pobj_signals_stop(looper);

    pobj_destroy(&looper);
}
