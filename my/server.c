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
  
  //int val = 1;
  //nn_setsockopt(sock, NN_TCP, NN_TCP_NODELAY, &val, sizeof(val));
  
  assert (nn_bind (sock, "tcp://*:55155") >= 0);

  while (1)
    {
      char *buf = NULL;
      int bytes = nn_recv (sock, &buf, NN_MSG, 0);
      assert (bytes >= 0);
      printf("%.*s\n", bytes, buf);
//usleep(300000);
	  bytes = nn_send (sock, buf, bytes, 0);


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