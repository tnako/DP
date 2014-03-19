#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <netinet/in.h>

#define DEFAULT_PORT (12345)
#define MAX_EVENTS   (1000)

using namespace std;


struct conn {
    int sock;
    char buf[6];
    size_t alloced, head, tail;
    bool read_end;
    bool error;

    conn(int sock) :
        sock(sock), head(0), tail(0), read_end(false), error(false)
    {

    }
    ~conn() { close(sock); }

    void read() {
	buf[0] = 0;
            //printf("reading: %d, %p, %ld\n", sock, buf+tail, alloced-tail);
            int n = ::read(sock, buf, 6);
            if (n < 0) {
                perror("read");
                error = true;
                return;
            }
    }
    int write() {
	int n = ::write(sock, buf, 6);
	if (n < 0) {
	    //if (errno == EAGAIN) break;
	    perror("write");
	    error = true;
	    return -1;
	}

        return 6;
    }
    void handle() {
        if (error) return;
        read();
        if (error) return;
        write();
    }
    int done() const {
        return error;
    }
};

static void setnonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

static int setup_server_socket(int port)
{
    int sock;
    struct sockaddr_in sin;
    int yes=1;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    memset(&sin, 0, sizeof sin);

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &sin, sizeof sin) < 0) {
        close(sock);
        perror("bind");
        exit(-1);
    }

    if (listen(sock, 300) < 0) {
        close(sock);
        perror("listen");
        exit(-1);
    }

    return sock;
}

int main(int argc, char *argv[])
{
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
    int listener, epfd;
    int procs=1;

    int opt, port=DEFAULT_PORT;

    while (-1 != (opt = getopt(argc, argv, "p:f:"))) {
        switch (opt) {
        case 'p':
            port = atoi(optarg);
            break;
        case 'f':
            procs = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Unknown option: %c\n", opt);
            return 1;
        }
    }

    listener = setup_server_socket(port);

    for (int i=1; i<procs; ++i)
        fork();

    if ((epfd = epoll_create(128)) < 0) {
        perror("epoll_create");
        exit(-1);
    }

    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.fd = listener;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev);

    printf("Listening port %d\n", port);

    unsigned long proc = 0;
    struct timeval tim, tim_prev;
    gettimeofday(&tim_prev, NULL);
    tim_prev = tim;

    for (;;) {
        int i;
        int nfd = epoll_wait(epfd, events, MAX_EVENTS, -1);

        for (i = 0; i < nfd; i++) {
            if (events[i].data.fd == listener) {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof client_addr;

                int client = accept(listener, (struct sockaddr *) &client_addr, &client_addr_len);
                if (client < 0) {
                    perror("accept");
                    continue;
                }

                setnonblocking(client);
		memset(&ev, 0, sizeof ev);
                ev.events = EPOLLIN;
                ev.data.ptr = (void*)new conn(client);
                epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev);
            } else {
		if (events[i].events & EPOLLHUP) {
		    conn *pc = (conn*)events[i].data.ptr;
		    epoll_ctl(epfd, EPOLL_CTL_DEL, pc->sock, &ev);
		    delete pc;
		    break;
		} else {
		    conn *pc = (conn*)events[i].data.ptr;
		    pc->handle();
		    if (pc->done()) {
			printf("delete\n");
			epoll_ctl(epfd, EPOLL_CTL_DEL, pc->sock, &ev);
			delete pc;
		    }
		}
            }
        }
    }

    return 0;
}

