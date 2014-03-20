#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int main (void)
{
    //  Socket to talk to clients
    int io_threads = 10;
    void *context = zmq_ctx_new ();
    zmq_ctx_set (context, ZMQ_IO_THREADS, io_threads);
    assert (zmq_ctx_get (context, ZMQ_IO_THREADS) == io_threads);
    
    void *responder = zmq_socket (context, ZMQ_REP);
    int rc = zmq_bind (responder, "tcp://*:12345");
    assert (rc == 0);

    while (1) {
        char buffer [6];
        zmq_recv (responder, buffer, 5, 0);
        zmq_send (responder, buffer, 5, 0);
    }
    return 0;
}