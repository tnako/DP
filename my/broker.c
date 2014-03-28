#include <zmq.h>


#define MDPC_CLIENT         "MDPC01"
#define MDPW_WORKER         "MDPW01"

#define MDPW_READY          "\001"
#define MDPW_REQUEST        "\002"
#define MDPW_REPLY          "\003"
#define MDPW_HEARTBEAT      "\004"
#define MDPW_DISCONNECT     "\005"

#define HEARTBEAT_LIVENESS  3       //  3-5 is reasonable
#define HEARTBEAT_INTERVAL  2500    //  msecs
#define HEARTBEAT_EXPIRY    HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS


typedef struct {
    zctx_t *ctx;                //  Our context
    void *socket;               //  Socket for clients & workers
    int verbose;                //  Print activity to stdout
    char *endpoint;             //  Broker binds to this endpoint
    zhash_t *services;          //  Hash of known services
    zhash_t *workers;           //  Hash of known workers
    zlist_t *waiting;           //  List of waiting workers
    uint64_t heartbeat_at;      //  When to send HEARTBEAT
} broker_t;

typedef struct {
    broker_t *broker;           //  Broker instance
    char *name;                 //  Service name
    zlist_t *requests;          //  List of client requests
    zlist_t *waiting;           //  List of waiting workers
    size_t workers;             //  How many workers we have
} service_t;

typedef struct {
    broker_t *broker;           //  Broker instance
    char *id_string;            //  Identity of worker as string
    zframe_t *identity;         //  Identity frame for routing
    service_t *service;         //  Owning service, if known
    int64_t expiry;             //  When worker expires, if no heartbeat
} worker_t;

static broker_t *s_broker_new (int verbose)
{
    broker_t *self = (broker_t *) zmalloc (sizeof (broker_t));

    self->ctx = zctx_new ();
    self->socket = zsocket_new (self->ctx, ZMQ_ROUTER);
    self->verbose = verbose;
    self->services = zhash_new ();
    self->workers = zhash_new ();
    self->waiting = zlist_new ();
    self->heartbeat_at = zclock_time () + HEARTBEAT_INTERVAL;
    return self;
}

static void s_service_destroy(void *argument)
{
    service_t *service = (service_t *) argument;
	
    while (zlist_size(service->requests)) {
        zmsg_t *msg = zlist_pop(service->requests);
        zmsg_destroy(&msg);
    }
    
    zlist_destroy(&service->requests);
    zlist_destroy(&service->waiting);
    free(service->name);
    free(service);
}

static service_t *s_service_require(broker_t *self, zframe_t *service_frame)
{
    assert (service_frame);
    char *name = zframe_strdup(service_frame);

    service_t *service = (service_t *)zhash_lookup(self->services, name);
    if (service == NULL) {
        service = (service_t *) zmalloc (sizeof (service_t));
        service->broker = self;
        service->name = name;
        service->requests = zlist_new();
        service->waiting = zlist_new();
        zhash_insert(self->services, name, service);
        zhash_freefn(self->services, name, s_service_destroy);
        if (self->verbose)
            zclock_log ("I: added service: %s", name);
    } else {
        free (name);
	}

    return service;
}

static void s_broker_purge(broker_t *self)
{
    worker_t *worker = (worker_t *) zlist_first(self->waiting);
    while (worker) {
        if (zclock_time () < worker->expiry) {
            break;                  //  Worker is alive, we're done here
		}
		
        if (self->verbose) {
            zclock_log ("I: deleting expired worker: %s", worker->id_string);
		}

        s_worker_delete(worker, 0);
        worker = (worker_t *) zlist_first (self->waiting);
    }
}

static void s_worker_send(worker_t *self, char *command, zmsg_t *msg)
{
    msg = (msg ? zmsg_dup(msg): zmsg_new());
	
    zmsg_pushstr(msg, command);
    zmsg_pushstr(msg, MDPW_WORKER);

    //  Stack routing envelope to start of message
!    zmsg_wrap(msg, zframe_dup(self->identity));

    if (self->broker->verbose) {
        zclock_log ("I: sending %s to worker", mdps_commands [(int) *command]);
        zmsg_dump(msg);
    }
    zmsg_send(&msg, self->broker->socket);
}

static void s_service_dispatch(service_t *self, zmsg_t *msg)
{
    assert(self);
    if (msg) {                    //  Queue message if any
        zlist_append(self->requests, msg);
	}

    s_broker_purge(self->broker);
    while (zlist_size(self->waiting) && zlist_size(self->requests)) {
        worker_t *worker = zlist_pop(self->waiting);
        zlist_remove(self->broker->waiting, worker);
        zmsg_t *msg = zlist_pop(self->requests);
        s_worker_send(worker, MDPW_REQUEST, msg);
        zmsg_destroy (&msg);
    }
}

static void s_broker_client_msg(broker_t *self, zframe_t *sender, zmsg_t *msg)
{
    assert (zmsg_size(msg) >= 2);     //  Service name + body

    zframe_t *service_frame = zmsg_pop(msg);
    service_t *service = s_service_require(self, service_frame);
! // Не должен создавать сервис, в случаи запроса от клиента

    //  Set reply return identity to client sender
!    zmsg_wrap(msg, zframe_dup(sender));
// Что делают эти функции?

    //  If we got a MMI service request, process that internally
    if (zframe_size(service_frame) >= 4 &&  memcmp(zframe_data(service_frame), "mmi.", 4) == 0) {
        char *return_code;
		
        if (zframe_streq(service_frame, "mmi.service")) {
            char *name = zframe_strdup (zmsg_last (msg));
            service_t *service = (service_t *) zhash_lookup(self->services, name);
            return_code = service && (service->workers ? "200": "404");
            free(name);
        } else {
            return_code = "501";
		}

!        zframe_reset(zmsg_last(msg), return_code, strlen(return_code));

        //  Remove & save client return envelope and insert the
        //  protocol header and service name, then rewrap envelope.
!        zframe_t *client = zmsg_unwrap (msg);
        zmsg_push(msg, zframe_dup(service_frame));
        zmsg_pushstr(msg, MDPC_CLIENT);
        zmsg_wrap(msg, client);
        zmsg_send(&msg, self->socket);
    } else {
        //  Else dispatch the message to the requested service
        s_service_dispatch(service, msg);
	}
	
    zframe_destroy(&service_frame);
}

static worker_t *s_worker_require(broker_t *self, zframe_t *identity)
{
    assert (identity);

    //  self->workers is keyed off worker identity
    char *id_string = zframe_strhex(identity);
    worker_t *worker = (worker_t *) zhash_lookup(self->workers, id_string);

    if (worker == NULL) {
        worker = (worker_t *) zmalloc (sizeof (worker_t));
        worker->broker = self;
        worker->id_string = id_string;
        worker->identity = zframe_dup(identity);
        zhash_insert(self->workers, id_string, worker);
        zhash_freefn(self->workers, id_string, s_worker_destroy);
        if (self->verbose) {
            zclock_log ("I: registering new worker: %s", id_string);
		}
    } else {
        free (id_string);
	}
	
    return worker;
}

static void s_worker_delete (worker_t *self, int disconnect)
{
    assert (self);
    if (disconnect) {
        s_worker_send (self, MDPW_DISCONNECT, NULL, NULL); // ! Расширить до отключение через mmi
	}

    if (self->service) {
        zlist_remove (self->service->waiting, self);
        self->service->workers--;
    }
    
    zlist_remove (self->broker->waiting, self);
    //  This implicitly calls s_worker_destroy
    zhash_delete (self->broker->workers, self->id_string);
}

static void s_worker_waiting (worker_t *self)
{
    //  Queue to broker and service waiting lists
    assert (self->broker);
    zlist_append (self->broker->waiting, self);
    zlist_append (self->service->waiting, self);
    self->expiry = zclock_time () + HEARTBEAT_EXPIRY;
    s_service_dispatch (self->service, NULL);
}

static void s_broker_worker_msg(broker_t *self, zframe_t *sender, zmsg_t *msg)
{
    assert (zmsg_size(msg) >= 1);     //  At least, command

    zframe_t *command = zmsg_pop(msg);
!    char *id_string = zframe_strhex(sender);
    int worker_ready = (zhash_lookup(self->workers, id_string) != NULL);
    free (id_string);
	
    worker_t *worker = s_worker_require(self, sender);

    if (zframe_streq(command, MDPW_READY)) {
        if (worker_ready) {               //  Not first command in session
            s_worker_delete(worker, 1);
			// Додумать, по идеи синоним сердцебиения
		} else {
			if (zframe_size(sender) >= 4 &&  memcmp(zframe_data (sender), "mmi.", 4) == 0) {
				s_worker_delete(worker, 1);
				// Додумать, по идеи синоним сердцебиения
			} else {
				//  Attach worker to service and mark as idle
				zframe_t *service_frame = zmsg_pop(msg);
				worker->service = s_service_require(self, service_frame);
				worker->service->workers++;
				s_worker_waiting(worker);
				zframe_destroy(&service_frame);
			}
		}
    } else if (zframe_streq(command, MDPW_REPLY)) {
        if (worker_ready) {
            //  Remove and save client return envelope and insert the
            //  protocol header and service name, then rewrap envelope.
            zframe_t *client = zmsg_unwrap(msg);
            zmsg_pushstr(msg, worker->service->name);
            zmsg_pushstr(msg, MDPC_CLIENT);
            zmsg_wrap(msg, client);
            zmsg_send(&msg, self->socket);
            s_worker_waiting(worker);
        } else {
			// Просто обрыв связи между воркером и брокером
			// синоним сердцебиения
            s_worker_delete(worker, 1);
		}
    } else if (zframe_streq(command, MDPW_HEARTBEAT)) {
        if (worker_ready) {
            worker->expiry = zclock_time () + HEARTBEAT_EXPIRY;
		} else {
			// Просто обрыв связи между воркером и брокером
			// синоним сердцебиения
            s_worker_delete(worker, 1);
		}
    } else if (zframe_streq (command, MDPW_DISCONNECT)) {
        s_worker_delete(worker, 0);
	} else {
        zclock_log ("E: invalid input message");
        zmsg_dump (msg);
    }
    free (command);
    zmsg_destroy (&msg);
}

int main ()
{
    broker_t *self = s_broker_new(1);
	zsocket_bind(self, "tcp://*:5555");

    while (true) {
        zmq_pollitem_t items[] = { { self->socket,  0, ZMQ_POLLIN, 0 } };
        int rc = zmq_poll(items, 1, HEARTBEAT_INTERVAL * ZMQ_POLL_MSEC);
		
        if (rc == -1) {
            break;     
		}

        if (items [0].revents & ZMQ_POLLIN) {
            zmsg_t *msg = zmsg_recv(self->socket);
			
            if (!msg) {
                break;
			}
			
            if (self->verbose) {
                zclock_log("I: received message:");
                zmsg_dump(msg);
            }
            
            zframe_t *sender = zmsg_pop(msg);
!            zframe_t *empty  = zmsg_pop(msg);
            zframe_t *header = zmsg_pop(msg);

            if (zframe_streq(header, MDPC_CLIENT)) {
                s_broker_client_msg(self, sender, msg);
			} else {
			  if (zframe_streq(header, MDPW_WORKER)) {
				  s_broker_worker_msg(self, sender, msg);
			  } else {
				  zclock_log("E: invalid message:");
				  zmsg_dump(msg);
				  zmsg_destroy(&msg);
			  }
			}
            zframe_destroy(&sender);
            zframe_destroy(&empty);
            zframe_destroy(&header);
        }
        //  Disconnect and delete any expired workers
        //  Send heartbeats to idle workers if needed
! // Зачем посылать им, пусть они нам присылают
        if (zclock_time() > self->heartbeat_at) {
            s_broker_purge(self);
            worker_t *worker = (worker_t *) zlist_first(self->waiting);
            while (worker) {
                s_worker_send(worker, MDPW_HEARTBEAT, NULL);
                worker = (worker_t *) zlist_next(self->waiting);
            }
            self->heartbeat_at = zclock_time () + HEARTBEAT_INTERVAL;
        }
    }
    if (zctx_interrupted)
        printf ("W: interrupt received, shutting down...\n");

	zctx_destroy(&self->ctx);
	zhash_destroy(&self->services);
	zhash_destroy(&self->workers);
	zlist_destroy(&self->waiting);
	free(self);

	return 0;
}
