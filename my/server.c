#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/tcp.h>
// #include <msgpack.h>

int main ()
{
  int sock = nn_socket (AF_SP_RAW, NN_REP);
  assert (sock >= 0);

  int val = 1;
  nn_setsockopt(sock, NN_TCP, NN_TCP_NODELAY, &val, sizeof(val));
  
  assert (nn_bind (sock, "tcp://*:55155") >= 0);
  size_t sd_size = sizeof(int);
  int fd;
nn_getsockopt(sock, NN_SOL_SOCKET, NN_RCVFD, &fd, &sd_size);
  while (1)
    {

    struct nn_msghdr hdr;
    struct nn_iovec iov [2];
    char buf0 [10] = { 0 };
    char buf1 [6] = { 0 };
    iov [0].iov_base = buf0;
    iov [0].iov_len = sizeof (buf0);
    iov [1].iov_base = buf1;
    iov [1].iov_len = sizeof (buf1);
    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_iov = iov;
    hdr.msg_iovlen = 2;
    printf("1\n");
    usleep(830000);
    printf("0\n");
    int bytes = nn_recvmsg(sock, &hdr, NN_DONTWAIT);
    printf("2\n");
    
      //assert (bytes >= 0);
    if (bytes < 1) {
      continue;
    }
      printf("%.*s\n", bytes, buf0);
//usleep(300000);
	  bytes = nn_sendmsg (sock, &hdr, NN_DONTWAIT);


    }
  
/*	
  while (1)
    {
      char *buf = NULL;
      int bytes = nn_recv (sock, &buf, NN_MSG, 0);
      assert (bytes >= 0);

      msgpack_unpacked msg;
      msgpack_unpacked_init(&msg);
      
      size_t upk_pos = 0;
      while (msgpack_unpack_next(&msg, buf, bytes, &upk_pos)) {
	msgpack_object obj = msg.data;
	if (obj.type == MSGPACK_OBJECT_RAW) {
	  int bsize = obj.via.raw.size;
	  char *b = malloc(bsize);
	  memcpy(b, obj.via.raw.ptr, bsize);
	  bytes = nn_send (sock, b, bsize, NN_DONTWAIT);
	  free(b);
	}
      }
      
      nn_freemsg (buf);

    }
  */  
}