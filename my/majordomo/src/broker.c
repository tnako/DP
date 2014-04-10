#include "broker.h"

#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <signal.h>

#include <unistd.h>


pobj_loop *looper;
pnet_socket server;
bool can_send_server = false;

pnet_socket server2;
bool can_send_server2 = false;

pnet_socket client;
bool can_send_client = false;

/*
static void func(pobj_loop* loop, const puint32 epoll_events)
{
    plog_warn("....... %x | %u", loop, epoll_events);
    static int test = 0;
    if (test++ > 0) {
        pobj_internal_timer_stop(loop);
        test = 0;
    }

    if (can_send_client) {
        char *buf = "test";
        pnet_send(&client, buf);
    }

}
*/
static void func_SIGINT()
{
    plog_info("Got SIGINT");
    looper->broken = 1;
}

static void func_SIGQUIT()
{
    plog_info("Got SIGQUIT");
}

static void server_net_send()
{
    plog_info("Server | Can send!");
    can_send_server = true;
    pobj_unregister_event(looper, server.send_fd);
}

static void server2_net_send()
{
    plog_info("Server2 | Can send!");
    can_send_server2 = true;
    pobj_unregister_event(looper, server2.send_fd);
}

static void client_net_send()
{
    plog_info("Client | Can send!");
    can_send_client = true;
    pobj_unregister_event(looper, client.send_fd);
}

static void server_net_recv()
{
    plog_info("Server | Got new message");
    char *buf = NULL;
    int size = pnet_recv(&server, &buf);
    plog_dbg("Server | message: %s (%d)", buf, size);
    pnet_send(&client, buf);

    //char bufg[15] = { 0 };
    //sprintf(bufg, "%d test", 1234);
    //pnet_send(&client, bufg);

    can_send_client = false;
    pobj_register_event(looper, client_net_send, client.send_fd, POBJOUT);
}

static void server2_net_recv()
{
    plog_info("Server2 | Got new message");
    char *buf = NULL;
    int size = pnet_recv(&server2, &buf);
    plog_dbg("Server2 | message: %s (%d)", buf, size);
    pnet_send(&server2, buf);

//    char *bufg = "test";
//    pnet_send(&client, bufg);

    can_send_server2 = false;
    pobj_register_event(looper, server2_net_send, server2.send_fd, POBJOUT);
}

static void client_net_recv()
{
    plog_info("Client | Got new message");
    char *buf = NULL;
    int size = pnet_recv(&client, &buf);
    plog_dbg("Client | message: %s (%d)", buf, size);
    pnet_send(&server, buf);
    can_send_server = false;
    pobj_register_event(looper, server_net_send, server.send_fd, POBJOUT);
}

void broker_main_loop()
{
    //struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };

    looper = pobj_create(128, false);
    //pobj_internal_timer_start(looper, 1, ts, func);

    if (!pobj_signals_add(looper, SIGINT, func_SIGINT) || (!pobj_signals_add(looper, SIGQUIT, func_SIGQUIT))) {
        plog_error("signal error");
    }

    if (!pobj_signals_start(looper)) {
        plog_error("signal error");
    }

    pnet_server_start(&server, "tcp://*:12345");
    pobj_register_event(looper, server_net_recv, server.recv_fd, POBJIN);
    pobj_register_event(looper, server_net_send, server.send_fd, POBJOUT);

    pnet_server_start(&server2, "tcp://*:55155");
    pobj_register_event(looper, server2_net_recv, server2.recv_fd, POBJIN);
    pobj_register_event(looper, server2_net_send, server2.send_fd, POBJOUT);


    pnet_client_start(&client, "tcp://127.0.0.1:55155");
    pobj_register_event(looper, client_net_recv, client.recv_fd, POBJIN);
    pobj_register_event(looper, client_net_send, client.send_fd, POBJOUT);


    pobj_run(looper);



    if (!can_send_server) {
        pobj_unregister_event(looper, server.send_fd);
    }
    if (!can_send_server2) {
        pobj_unregister_event(looper, server2.send_fd);
    }
    if (!can_send_client) {
        pobj_unregister_event(looper, client.send_fd);
    }

    pobj_unregister_event(looper, server.recv_fd);
    pobj_unregister_event(looper, server2.recv_fd);
    pobj_unregister_event(looper, client.recv_fd);

    pnet_socket_destroy(&server);
    pnet_socket_destroy(&server2);
    pnet_socket_destroy(&client);

    pobj_signals_stop(looper);

    pobj_destroy(&looper);
}
