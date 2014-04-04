#include <zmq.h>

#define MDPC_CLIENT         "MDPC01"


typedef struct {
    zctx_t *ctx;                //  Our context
    char *broker;
    void *client;               //  Socket to broker
    int timeout;                //  Request timeout
} mdcli_t;



void s_mdcli_connect_to_broker(mdcli_t *self)
{
    if (self->client)
        zsocket_destroy(self->ctx, self->client);
    self->client = zsocket_new(self->ctx, ZMQ_DEALER);
    zmq_connect(self->client, self->broker);
    zclock_log("I: connecting to broker at %s...", self->broker);
}

mdcli_t *mdcli_new(char *broker)
{
    assert (broker);

    mdcli_t *self = (mdcli_t *) zmalloc (sizeof (mdcli_t));
    self->ctx = zctx_new();
    self->broker = strdup(broker);
    self->timeout = 5000;           //  msecs

    s_mdcli_connect_to_broker(self);
    return self;
}

int mdcli_send(mdcli_t *self, char *service, zmsg_t **request_p)
{
    assert(self);
    assert(request_p);
    zmsg_t *request = *request_p;

    //  Prefix request with protocol frames
    //  Frame 0: empty (REQ emulation)
    //  Frame 1: "MDPCxy" (six bytes, MDP/Client x.y)
    //  Frame 2: Service name (printable string)
    zmsg_pushstr(request, service);
    zmsg_pushstr(request, MDPC_CLIENT);
    zmsg_pushstr(request, "");
        zclock_log ("I: send request to '%s' service:", service);
        zmsg_dump (request);
    zmsg_send (&request, self->client);
    return 0;
}

static zmsg_t *s_service_call(mdcli_t *session, char *service, zmsg_t **request_p)
{
    zmsg_t *reply = mdcli_send(session, service, request_p);
    if (reply) {
        zframe_t *status = zmsg_pop(reply);
        if (zframe_streq (status, "200")) {
            zframe_destroy (&status);
            return reply;
        } else  if (zframe_streq(status, "400")) {
            printf ("E: client fatal error, aborting\n");
            exit (EXIT_FAILURE);
        } else if (zframe_streq(status, "500")) {
            printf("E: server fatal error, aborting\n");
            exit (EXIT_FAILURE);
        }
    } else
        exit (EXIT_SUCCESS);    //  Interrupted or failed

    zmsg_destroy (&reply);
    return NULL;        //  Didn't succeed; don't care why not
}

zmsg_t *mdcli_recv(mdcli_t *self)
{
    assert (self);

    //  Poll socket for a reply, with timeout
    zmq_pollitem_t items[] = { { self->client, 0, ZMQ_POLLIN, 0 } };
    int rc = zmq_poll(items, 1, self->timeout * ZMQ_POLL_MSEC);
    if (rc == -1)
        return NULL;            //  Interrupted

    //  If we got a reply, process it
    if (items[0].revents & ZMQ_POLLIN) {
        zmsg_t *msg = zmsg_recv(self->client);
            zclock_log ("I: received reply:");
            zmsg_dump (msg);
        //  Don't try to handle errors, just assert noisily
        assert (zmsg_size(msg) >= 4);

        zframe_t *empty = zmsg_pop(msg);
        assert (zframe_streq(empty, ""));
        zframe_destroy(&empty);

        zframe_t *header = zmsg_pop(msg);
        assert (zframe_streq(header, MDPC_CLIENT));
        zframe_destroy(&header);

        zframe_t *service = zmsg_pop(msg);
        zframe_destroy(&service);

        return msg;     //  Success
    }
    
    if (zctx_interrupted) {
        printf ("W: interrupt received, killing client...\n");
    } else {
        zclock_log ("W: permanent error, abandoning request");
    }

    return NULL;
}

int main (int argc, char *argv [])
{
    mdcli_t *session = mdcli_new ("tcp://localhost:5555");

    //  1. Send 'hello'
    zmsg_t *request = zmsg_new();
    zmsg_addstr(request, "echo");
    zmsg_addstr(request, "Hello world");
    zmsg_t *reply = s_service_call(session, "titanic.request", &request);

    zframe_t *uuid = NULL;
    if (reply) {
        uuid = zmsg_pop(reply);
        zmsg_destroy(&reply);
        zframe_print(uuid, "I: request UUID ");
    }
    
    //  2. Wait until we get a reply
    /*
    while (!zctx_interrupted) {
        request = zmsg_new();
        zmsg_add(request, zframe_dup(uuid));
        zmsg_t *reply = s_service_call(session, "titanic.reply", &request);

        if (reply) {
            char *reply_string = zframe_strdup (zmsg_last (reply));
            printf ("Reply: %s\n", reply_string);
            free (reply_string);
            zmsg_destroy (&reply);

            //  3. Close request
            request = zmsg_new();
            zmsg_add(request, zframe_dup (uuid));
            reply = s_service_call(session, "titanic.close", &request);
            zmsg_destroy(&reply);
            break;
        } else {
            printf ("I: no reply yet, trying again...\n");
            zclock_sleep (5000);     //  Try again in 5 seconds
        }
    }
    */
    
    for (count = 0; count < 100000; count++) {
        zmsg_t *reply = mdcli_recv(session);
        if (reply)
            zmsg_destroy (&reply);
        else
            break;              //  Interrupted by Ctrl-C
    }
    printf ("%d replies received\n", count);
    
    zframe_destroy (&uuid);
    mdcli_destroy (&session);
    return 0;
} 
