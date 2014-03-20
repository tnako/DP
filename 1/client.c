#include "czmq.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define SIZE 200000
void Die(char *mess) { perror(mess); exit(1); }

int pid;
int gcper;
int max_cper;
int min_cper = 999999999;
int done;

static void *sender_task (void *args)
{
    int sock;
    
    struct sockaddr_in echoserver;
    char buffer[6];
    int requests;
    int64_t start;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	Die("Failed to create socket");
    }
    memset(&echoserver, 0, sizeof(echoserver));
    echoserver.sin_family = AF_INET;
    echoserver.sin_addr.s_addr = inet_addr("127.0.0.1");
    echoserver.sin_port = htons(12345);
    if (connect(sock, (struct sockaddr *) &echoserver, sizeof(echoserver)) < 0) {
      Die("Failed to connect with server");
    }
    
    start = zclock_time ();
    for (requests = 0; requests < SIZE; requests++) { 
	if (send(sock, "hello", 5, 0) != 5) {
	  Die("Mismatch in number of sent bytes");
	}
    }
    for (requests = 0; requests < SIZE; requests++) { 
	if ((recv(sock, buffer, 5, 0)) < 1) {
	  Die("Failed to receive bytes from server");
	}
	
    }
    int cpers = (1000 * SIZE) / (int) (zclock_time () - start);
    gcper += cpers;
    
    if (max_cper < cpers) {
      max_cper = cpers;
    }
    if (min_cper > cpers) {
      min_cper = cpers;
    }
    
    close(sock);
            
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