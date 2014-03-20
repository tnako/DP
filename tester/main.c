#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <time.h>
#include <sys/resource.h>

#include <glib.h>
#include <czmq.h>


#define CYCLES 1
#define MAX_THREADS 128 // x2
#define MAX_CONNECTIONS 16
#define MESSAGES 200000


#define BUFFER_READ 16

#define MES_TEXT "HELLO"



static struct sockaddr_in saddr;
static unsigned int discon_counter = 0;
static unsigned int sockets_wo_send_counter = 0;
static unsigned int reads_counter = 0;
static unsigned int sends_counter = 0;

static unsigned int messages_per_conn_min = MESSAGES + 1;
static unsigned int messages_per_conn_avg = 0;
static unsigned int messages_per_conn_max = 0;


pthread_t tid[MAX_THREADS];
pthread_mutex_t mutex_update = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_messages_per_conn = PTHREAD_MUTEX_INITIALIZER;

unsigned int curr_ = 0;
unsigned int max_ = 0;
time_t last_ts_;

void update(int events)
{
    time_t t = time(NULL);
    pthread_mutex_lock(&mutex_update);
    if (last_ts_ == t) {
        curr_ += events;
    } else {
        // recahrge
        if (curr_ > max_) {
            max_ = curr_;
        }
        curr_ = events;
        last_ts_ = t;
    }
    pthread_mutex_unlock(&mutex_update);
}

static void find_min_max(gpointer key, gpointer value, gpointer user_data)
{
    if (user_data == NULL && key) {
        unsigned int tmp_val = GPOINTER_TO_INT(value);
        pthread_mutex_lock(&mutex_messages_per_conn);
        if (messages_per_conn_min > tmp_val) {
            messages_per_conn_min = tmp_val;
        }

        if (messages_per_conn_avg < 1) {
            messages_per_conn_avg = tmp_val;
        } else {
            messages_per_conn_avg = (messages_per_conn_avg + tmp_val) / 2;
        }

        if (messages_per_conn_max < tmp_val) {
            messages_per_conn_max = tmp_val;
        }
        pthread_mutex_unlock(&mutex_messages_per_conn);
    }
}

void* run_test(void  *threadid)
{
    long tid = (long)threadid;

    for (int cycle = 0; cycle < CYCLES; ++cycle) {

        GHashTable* sockets = g_hash_table_new(g_direct_hash, g_direct_equal);

        int epfd = epoll_create(MAX_CONNECTIONS);
        if (epfd < 1) {
            printf("can't create epoll\n");
            if (sockets) {
                g_hash_table_destroy(sockets);
                sockets = NULL;
            }
            return NULL;
        }

        struct epoll_event ev, ev_read, events[MAX_CONNECTIONS];
        memset(&ev, 0x0, sizeof(struct epoll_event));
        memset(&ev_read, 0x0, sizeof(struct epoll_event));
        memset(events, 0x0, sizeof(struct epoll_event) * MAX_CONNECTIONS);

        ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
        ev_read.events = EPOLLIN | EPOLLRDHUP;

        //unsigned int sends = 0;

        int sd[MAX_CONNECTIONS] = { 0 };
        void *context[MAX_CONNECTIONS] = { 0 };
        void *requester[MAX_CONNECTIONS] = { 0 };

        for (size_t i = 0; i < MAX_CONNECTIONS; ++i) {
            context[i] = zmq_ctx_new ();
            requester[i] = zmq_socket (context[i], ZMQ_REQ);
            size_t sd_size = sizeof(int);
            zmq_getsockopt(requester[i], ZMQ_FD, &sd[i], &sd_size);

            if (sd[i] < 0) {
                sd[i] = 0;
                printf("can't create socket #%d\n", (int)i);
                if (sockets) {
                    g_hash_table_destroy(sockets);
                    sockets = NULL;
                }
                zmq_close(requester[i]);
                zmq_ctx_destroy(context[i]);
                return NULL;
            }

            zmq_connect(requester[i], "tcp://127.0.0.1:12345");

            if (sd[i]) {
                g_hash_table_insert(sockets, GINT_TO_POINTER(sd[i]), GINT_TO_POINTER(0));
                ev.data.fd = sd[i];
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd[i], &ev)) {
                    printf("can't epoll_ctl\n");
                    if (sockets) {
                        g_hash_table_destroy(sockets);
                        sockets = NULL;
                    }
                    zmq_close(requester[i]);
                    zmq_ctx_destroy(context[i]);
                    return NULL;
                }
            }
        }

        while (1) {
            int rv = epoll_wait(epfd, events, MAX_CONNECTIONS, 300);

            if (rv == 0) {
                break;
            } else if (rv < 0) {
                flockfile(stdout);
                printf("#%ld | can't epoll_wait: %s (%d)\n", tid, strerror(errno), errno);
                funlockfile(stdout);
                break;
            }

            for (int epoll_event = 0; epoll_event < rv ; ++epoll_event) {

                int num = -1;
                for (size_t i = 0; i < MAX_CONNECTIONS; ++i) {
                    if (sd[i] == events[epoll_event].data.fd) {
                        num = i;
                        break;
                    }
                }
                if (num == -1) {
                    continue;
                }

                int so_error;
                socklen_t len = sizeof so_error;

                getsockopt(events[epoll_event].data.fd, SOL_SOCKET, SO_ERROR, &so_error, &len);

                if (so_error != 0) {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[epoll_event].data.fd, NULL)) {
                        flockfile(stdout);
                        printf("#%ld | Couldn't epoll_ctl1, fd=%d, error: %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                        funlockfile(stdout);
                    }
                    ++discon_counter;
                    continue;
                }

                if (events[epoll_event].events & EPOLLIN) {

                    unsigned int     zmq_events;
                    size_t           zmq_events_size  = sizeof(zmq_events);

                    zmq_getsockopt (requester[num], ZMQ_EVENTS, &zmq_events, &zmq_events_size);

                    if (zmq_events & ZMQ_POLLIN) {
                        ++reads_counter;

                        char buffer[BUFFER_READ];
                        if (zmq_recv(requester[num], buffer, 5, ZMQ_NOBLOCK) < 0) {
                            flockfile(stdout);
                            printf("#%ld | can't recv on socket #%d %s (%d)\n", tid, events[epoll_event].data.fd, zmq_strerror(zmq_errno()), zmq_errno());
                            funlockfile(stdout);
                        }

                        if (GPOINTER_TO_INT(g_hash_table_lookup(sockets, GINT_TO_POINTER(events[epoll_event].data.fd))) < MESSAGES) {
                            ev.data.fd = events[epoll_event].data.fd;
                            if (epoll_ctl(epfd, EPOLL_CTL_MOD, events[epoll_event].data.fd, &ev)) {
                                flockfile(stdout);
                                printf("#%ld | Couldn't epoll_ctl3.1, fd=%d, error: %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                                funlockfile(stdout);
                            }
                        }
                    }
                }
                if (events[epoll_event].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    ++discon_counter;
                    flockfile(stdout);
                    //printf("#%ld | Debug: Close conn: %d - 0x%04x\n", tid, events[epoll_event].data.fd, events[epoll_event].events);
                    funlockfile(stdout);

                    ev.data.fd = events[epoll_event].data.fd;
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[epoll_event].data.fd, &ev)) {
                        flockfile(stdout);
                        printf("#%ld | Couldn't epoll_ctl2, fd=%d, error: %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                        funlockfile(stdout);
                    }
                    //shutdown(events[epoll_event].data.fd, SHUT_RDWR);
                    continue;
                }
                if (events[epoll_event].events & EPOLLOUT) {
                    unsigned int     zmq_events;
                    size_t           zmq_events_size  = sizeof(zmq_events);
                    zmq_getsockopt (requester[num], ZMQ_EVENTS, &zmq_events, &zmq_events_size);
                    if (zmq_events & ZMQ_POLLOUT) {
                        if (zmq_send(requester[num], "Hello", 5, ZMQ_NOBLOCK) < 0) {
                            flockfile(stdout);
                            printf("#%ld | can't send on socket #%d %s (%d)\n", tid, events[epoll_event].data.fd, zmq_strerror(zmq_errno()), zmq_errno());
                            funlockfile(stdout);

                            ev.data.fd = events[epoll_event].data.fd;
                            if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[epoll_event].data.fd, &ev)) {
                                flockfile(stdout);
                                printf("#%ld | Couldn't epoll_ctl4, fd=%d, error: %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                                funlockfile(stdout);
                            }
                            zmq_close(requester[num]);
                            zmq_ctx_destroy(context[num]);
                            close(events[epoll_event].data.fd);

                            break;
                        } else {
                            int a = GPOINTER_TO_INT(g_hash_table_lookup(sockets, GINT_TO_POINTER(events[epoll_event].data.fd)));
                            g_hash_table_insert(sockets, GINT_TO_POINTER(events[epoll_event].data.fd), GINT_TO_POINTER(++a));
                            update(1);
                            ++sends_counter;
                        }

                        ev_read.data.fd = events[epoll_event].data.fd;
                        if (epoll_ctl(epfd, EPOLL_CTL_MOD, events[epoll_event].data.fd, &ev_read)) {
                            flockfile(stdout);
                            printf("#%ld | Couldn't epoll_ctl3, fd=%d, error: %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                            funlockfile(stdout);
                        }
                    }
                }
            }
            fflush(stdout);
        }

        for (size_t i = 0; i < MAX_CONNECTIONS; ++i) {
            if (GPOINTER_TO_INT(g_hash_table_lookup(sockets, GINT_TO_POINTER(sd[i]))) == 0) {
                ++sockets_wo_send_counter;
            }
            zmq_close(requester[i]);
            zmq_ctx_destroy(context[i]);
        }

        if (sockets) {
            g_hash_table_foreach(sockets, find_min_max, NULL);
            g_hash_table_destroy(sockets);
        }
    }
    return NULL;
}

void sig_handler(int sig_num)
{
    printf("received signal %d\n", sig_num);
    exit(0);
}

void set_sig_handlers()
{
    struct sigaction sa;

    sigemptyset (&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);
    sigaddset(&sa.sa_mask, SIGPIPE);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaddset(&sa.sa_mask, SIGUSR2);

    sa.sa_handler = sig_handler;
    sa.sa_flags = SA_RESTART;

    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Please specify server address and port\n");
        return 1;
    }

    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim)) {
        printf("getrlimit() failed\n");
        return 1;
    } else {
        if (rlim.rlim_cur < MAX_THREADS * MAX_CONNECTIONS
                || rlim.rlim_max < MAX_THREADS * MAX_CONNECTIONS)
        {
            printf("please adjust limit of open files to %d\n", MAX_THREADS * MAX_CONNECTIONS);
            return 2;
        }
    }

    set_sig_handlers();

    memset(tid, 0x0, sizeof(pthread_t) * MAX_THREADS);
    memset(&saddr, 0x0, sizeof(struct sockaddr_in));

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &saddr.sin_addr.s_addr) <= 0) {
        printf("Bad address: [%s]\n ", argv[1]);
        return 3;
    }

    time_t AllTime = time(NULL);
    printf("\nRPS: \nDisconnects: \nSockets without sends: \nReads: \nSends: \nMessages per connection (min/avg/max): \nCycle time: \n");
    for (int threads = 1; threads < MAX_THREADS; threads = threads * 2) {
        printf("----------\n Start with %d threads\n----------\n", threads);
        last_ts_ = time(NULL);

        time_t OneCycle = time(NULL);
        for (long i = 0; i < threads; ++i) {
            if (pthread_create(&(tid[i]), NULL, run_test, (void *)i) != 0) {
                printf("Can't create thread :[%s]\n ", strerror(errno));
            }
        }

        for (long i = 0; i < threads; ++i) {
            if (tid[i] != 0) {
                pthread_join(tid[i], NULL);
            }
        }

        unsigned int value;
        if (curr_ > max_) {
            value = curr_;
        } else {
            value = max_;
        }
        printf("%d\n%d\n%d\n%d\n%d\n%u/%u/%u\n%d\n",
               value,
               discon_counter,
               sockets_wo_send_counter,
               reads_counter,
               sends_counter,
               messages_per_conn_min,
               messages_per_conn_avg,
               messages_per_conn_max,
               (int)(time(NULL) - OneCycle));


        fflush(stdout);

        discon_counter = 0;
        sockets_wo_send_counter = 0;
        reads_counter = 0;
        sends_counter = 0;

        messages_per_conn_min = MESSAGES + 1;
        messages_per_conn_avg = 0;
        messages_per_conn_max = 0;
        curr_ = 0;
        max_ = 0;
    }
    printf("----------\nAllTime: %d\n", (int)(time(NULL) - AllTime));

    return 0;
}
