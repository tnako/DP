//  Hello World client
#include <czmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define SIZE 200000

int pid;
int gcper;
int max_cper;
int min_cper = 999999999;
int done;

static void *sender_task (void *args)
{
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, "tcp://127.0.0.1:12345");
    
    int64_t start = zclock_time ();
    for (int requests = 0; requests < SIZE; requests++) { 
        zmq_send(requester, "Hello", 5, 0);
    }
    for (int requests = 0; requests < SIZE; requests++) { 
	char buffer [6];
        zmq_recv (requester, buffer, 5, 0);
    }
    
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

    printf ("threads | min | avg | max\n");
    int64_t start = zclock_time ();
    for (test = 0; test < 8; ++test) {
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
      printf ("%d %d %d %d\n", threads, min_cper, gcper / threads, max_cper);
      
      threads *= 2;
    }
    printf ("time: %d\n", (int) (zclock_time () - start));
    
    return 0;
}

