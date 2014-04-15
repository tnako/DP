//  Hello World client
#include <czmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define SIZE 1

int pid;
int gcper;
int max_cper;
int min_cper = 999999999;
int done;

static void *sender_task (void *args)
{
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_DEALER);
    zmq_connect (requester, "tcp://127.0.0.1:12345");
    
    int64_t start = zclock_time ();
    for (int requests = 0; requests < SIZE; requests++) { 
	zmsg_t *request = zmsg_new();
	zmsg_addstr(request, "echo1");
	zmsg_addstr(request, "echo2");
	zmsg_addstr(request, "echo3");
	zmsg_addstr(request, "Hello world");
	zmsg_pushstr(request, "echo");
	zmsg_pushstr(request, "PC01");
	zmsg_pushstr(request, "");
	//zmsg_dump (request);
	int ret = zmsg_send (&request, requester);
	if (ret < 0) {
	  printf("send %d | %d | %s\n", ret, zmq_errno(), zmq_strerror(zmq_errno()));
	}
        //zmq_send(requester, "PC01", 4, 0);
        
        
      zmsg_t *msg = zmsg_recv(requester);
      zmsg_dump(msg);
      zmsg_destroy(&msg);

        
    }
//    for (int requests = 0; requests < SIZE; requests++) { 
	//char buffer [6];
        //zmq_recv(requester, buffer, 6, 0);
//    }
    
    int64_t end_time = zclock_time() - start;
    
    int cpers = 0;
    if (end_time != 0) {
      cpers = (1000 * SIZE) / (int) (end_time);
    }
    
    gcper += cpers;
    
    if (max_cper < cpers) {
      max_cper = cpers;
    }
    if (min_cper > cpers) {
      min_cper = cpers;
    }
    
    zmq_close (requester);
    zmq_ctx_destroy (context);
            
    ++done;
    return NULL;
}

int main (void)
{
    int num;
    int threads = 1;
    
    int test;

    printf ("threads | min | avg | max | mrpst\n");
    int64_t start = zclock_time ();
    for (test = 0; test < 1; ++test) {
      pid = 1;
      gcper = 0;
      max_cper = 0;
      done = 0;
      
      for (num = 0; num < threads; ++num) {
	  int rc = zthread_new (sender_task, NULL);
	  assert (rc == 0);
      }
      
      while (done != threads) {
	zclock_sleep (100);
      }
      printf ("%d | %d | %d | %d | %d\n", threads, min_cper, gcper / threads, max_cper, threads * min_cper);
      
      threads *= 2;
    }
    printf ("time: %d\n", (int) (zclock_time () - start));
    
    return 0;
}

