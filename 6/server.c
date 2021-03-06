#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/tcp.h>

#define NODE0 "node0"
#define NODE1 "node1"

int node0 (const char *url)
{
  int sock = nn_socket (AF_SP, NN_REP);
  assert (sock >= 0);
  
  int val = 1;
  nn_setsockopt(sock, NN_TCP, NN_TCP_NODELAY, &val, sizeof(val));

  
  assert (nn_bind (sock, url) >= 0);
  
  while (1)
    {
      
      char *buf = NULL;
      int bytes = nn_recv (sock, &buf, NN_MSG, 0);
      assert (bytes >= 0);
      //printf ("NODE0: RECEIVED \"%s\"\n", buf);
      bytes = nn_send (sock, buf, bytes, NN_DONTWAIT);
      nn_freemsg (buf);
      
      /*
struct nn_msghdr hdr;
struct nn_iovec iov [2];
char buf0 [4];
char buf1 [2];
iov [0].iov_base = buf0;
iov [0].iov_len = sizeof (buf0);
iov [1].iov_base = buf1;
iov [1].iov_len = sizeof (buf1);
memset (&hdr, 0, sizeof (hdr));
hdr.msg_iov = iov;
hdr.msg_iovlen = 2;
nn_recvmsg (sock, &hdr, 0);

nn_sendmsg (sock, &hdr, NN_DONTWAIT);*/
      
    }
}

int node1 (const char *url, const char *msg)
{
  int sz_msg = strlen (msg) + 1; // '\0' too
  int sock = nn_socket (AF_SP, NN_REQ);
  assert (sock >= 0);
  assert (nn_connect (sock, url) >= 0);
  printf ("NODE1: SENDING \"%s\"\n", msg);
  int bytes = nn_send (sock, msg, sz_msg, 0);
  assert (bytes == sz_msg);
  char *buf = NULL;
  bytes = nn_recv (sock, &buf, NN_MSG, 0);
  assert (bytes >= 0);
  printf ("NODE1: RECEIVED \"%s\"\n", buf);
  nn_freemsg (buf);
  return nn_shutdown (sock, 0);
}

int main (const int argc, const char **argv)
{
  if (strncmp (NODE0, argv[1], strlen (NODE0)) == 0 && argc > 1)
    return node0 (argv[2]);
  else if (strncmp (NODE1, argv[1], strlen (NODE1)) == 0 && argc > 2)
    return node1 (argv[2], argv[3]);
  else
    {
      fprintf (stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
               NODE0, NODE1);
      return 1;
    }
}