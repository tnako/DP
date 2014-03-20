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



#define CYCLES 256
#define MAX_THREADS 1
#define CONNECTIONS 32
#define MESSAGES 90


#define BUFFER_READ 4096

#define MES_TEXT " HTTP/1.1\r\nHost: %s:%s\r\nAccept: */*\r\nUser-Agent: C.H.A.D.o_Bench/1.0\r\n"



static char msg[BUFFER_READ];
static struct sockaddr_in saddr;
static unsigned int discon_counter = 0;
static unsigned int sockets_wo_send_counter = 0;
static unsigned int reads_counter = 0;
static unsigned int sends_counter = 0;

static unsigned int messages_per_conn_min = MESSAGES + 1;
static unsigned int messages_per_conn_avg = 0;
static unsigned int messages_per_conn_max = 0;

static unsigned int stage = 0;

static unsigned long long time_nano = 0;


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

        flockfile(stdout);
        printf("\33[2K\r"); // Clear line
        printf("Done: %.1f%%", ++stage*100.0/(MAX_THREADS*CYCLES));
        funlockfile(stdout);


        GHashTable* sockets = g_hash_table_new(g_direct_hash, g_direct_equal);

        int epfd = epoll_create(CONNECTIONS);
        if (epfd < 1) {
            printf("can't create epoll\n");
            if (sockets) {
                g_hash_table_destroy(sockets);
                sockets = NULL;
            }
            return NULL;
        }

        struct epoll_event ev, ev_read, events[CONNECTIONS];
        memset(&ev, 0x0, sizeof(struct epoll_event));
        memset(&ev_read, 0x0, sizeof(struct epoll_event));
        memset(events, 0x0, sizeof(struct epoll_event) * CONNECTIONS);

        ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
        ev_read.events = EPOLLIN | EPOLLRDHUP;

        //unsigned int sends = 0;

        int sd[CONNECTIONS] = { 0 };

        for (size_t i = 0; i < CONNECTIONS; ++i) {
            sd[i] = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (sd[i] < 0) {
                sd[i] = 0;
                printf("can't create socket #%d\n", (int)i);
                if (sockets) {
                    g_hash_table_destroy(sockets);
                    sockets = NULL;
                }
                return NULL;
            }

            const int o = 1;
            if (setsockopt(sd[i], IPPROTO_TCP, TCP_NODELAY, &o, sizeof(o))) {
                printf("can't set TCP_NODELAY on socket #%d\n", (int)i);
                return NULL;
            }

            if (!connect(sd[i], (struct sockaddr *)&saddr, sizeof(saddr))) {
                printf("can't connect() #%d, %d\n", (int)i, errno);
                if (sockets) {
                    g_hash_table_destroy(sockets);
                    sockets = NULL;
                }
                return NULL;
            }

            if (sd[i]) {
                g_hash_table_insert(sockets, GINT_TO_POINTER(sd[i]), GINT_TO_POINTER(0));
                ev.data.fd = sd[i];
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd[i], &ev)) {
                    printf("can't epoll_ctl\n");
                    if (sockets) {
                        g_hash_table_destroy(sockets);
                        sockets = NULL;
                    }
                    return NULL;
                }
            }
        }

        char msg_send[BUFFER_READ];
        memset(msg_send, 0x0, BUFFER_READ);

        while (1) {
            int rv = epoll_wait(epfd, events, CONNECTIONS, 300);

            if (rv == 0) {
                break;
            } else if (rv < 0) {
                flockfile(stdout);
                printf("#%ld | can't epoll_wait: %s (%d)\n", tid, strerror(errno), errno);
                funlockfile(stdout);
                break;
            }

            for (int epoll_event = 0; epoll_event < rv ; ++epoll_event) {
                flockfile(stdout);
                //printf("#%ld | Debug: got event 0x%04x on fd=%04d | %p\n", tid, events[epoll_event].events, events[epoll_event].data.fd, &events);
                funlockfile(stdout);

                int so_error;
                socklen_t len = sizeof so_error;

                getsockopt(events[epoll_event].data.fd, SOL_SOCKET, SO_ERROR, &so_error, &len);

                if (so_error != 0) {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[epoll_event].data.fd, NULL)) {
                        flockfile(stdout);
                        printf("#%ld | Couldn't epoll_ctl1, fd=%d, error: %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                        funlockfile(stdout);
                    }
                    /*
                    flockfile(stdout);
                    printf("#%ld | Error %d with socket #%d: %s\n", tid, so_error, events[epoll_event].data.fd, strerror(so_error));
                    funlockfile(stdout);
                    */
                    ++discon_counter;
                    continue;
                }

                if (events[epoll_event].events & EPOLLIN) {
                    ++reads_counter;
                    char buffer[BUFFER_READ];
                    while (recv(events[epoll_event].data.fd, buffer, BUFFER_READ, 0) > 0) {
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
                    //int val = GPOINTER_TO_INT(g_hash_table_lookup(sockets, GINT_TO_POINTER(events[epoll_event].data.fd)));
                    //sprintf(msg_send, "GET /ajax/checkticketwin.php?s=AAAA&n=%08d&time=%d", val, val);
                    //sprintf(msg_send, "GET /%d", GPOINTER_TO_INT(g_hash_table_lookup(sockets, GINT_TO_POINTER(events[epoll_event].data.fd))));
                    sprintf(msg_send, "GET /");
                    strcat(msg_send, msg);
                    /*
                if (sockets[events[epoll_event].data.fd] == MESSAGES) {
                    strcat(msg_send, "Connection: close\r\n");
                }*/
                    strcat(msg_send, "\r\n");

                    if (send(events[epoll_event].data.fd, msg_send, strlen(msg_send), MSG_NOSIGNAL) < 0) {
                        flockfile(stdout);
                        printf("#%ld | can't send on socket #%d %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                        funlockfile(stdout);

                        ev.data.fd = events[epoll_event].data.fd;
                        if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[epoll_event].data.fd, &ev)) {
                            flockfile(stdout);
                            printf("#%ld | Couldn't epoll_ctl4, fd=%d, error: %s (%d)\n", tid, events[epoll_event].data.fd, strerror(errno), errno);
                            funlockfile(stdout);
                        }
                        close(events[epoll_event].data.fd);

                        break;
                    } else {
                        int a = GPOINTER_TO_INT(g_hash_table_lookup(sockets, GINT_TO_POINTER(events[epoll_event].data.fd)));
                        g_hash_table_insert(sockets, GINT_TO_POINTER(events[epoll_event].data.fd), GINT_TO_POINTER(++a));
                        struct timespec first, second;
                        clock_gettime(CLOCK_MONOTONIC_RAW,&first);
                        update(1);
                        clock_gettime(CLOCK_MONOTONIC_RAW,&second);
                        if (time_nano == 0) {
                            time_nano = ((second.tv_sec*1000000000.0 + second.tv_nsec) - (first.tv_sec*1000000000.0 + first.tv_nsec));
                        } else {
                            time_nano += ((second.tv_sec*1000000000.0 + second.tv_nsec) - (first.tv_sec*1000000000.0 + first.tv_nsec));
                            time_nano /= 2.0;
                        }
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
            fflush(stdout);
        }

        for (size_t i = 0; i < CONNECTIONS; ++i) {
            if (GPOINTER_TO_INT(g_hash_table_lookup(sockets, GINT_TO_POINTER(sd[i]))) == 0) {
                ++sockets_wo_send_counter;
            }
            close(sd[i]);
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
        if (rlim.rlim_cur < MAX_THREADS * CONNECTIONS
                || rlim.rlim_max < MAX_THREADS * CONNECTIONS)
        {
            printf("please adjust limit of open files to %d\n", MAX_THREADS * CONNECTIONS);
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

    sprintf(msg, MES_TEXT, argv[1], argv[2]);
    last_ts_ = time(NULL);

    for (long i = 0; i < MAX_THREADS; ++i) {
        if (pthread_create(&(tid[i]), NULL, run_test, (void *)i) != 0) {
            printf("Can't create thread :[%s]\n ", strerror(errno));
        }
    }

    for (long i = 0; i < MAX_THREADS; ++i) {
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
    printf("\nRPS: %d\nDisconnects: %d\nSockets without sends: %d\nReads: %d\nSends: %d\nMessages per connection (min/avg/max): %u/%u/%u\n",
           value,
           discon_counter,
           sockets_wo_send_counter,
           reads_counter,
           sends_counter,
           messages_per_conn_min,
           messages_per_conn_avg,
           messages_per_conn_max);

    printf("\ntime: %llu\n", time_nano);

    fflush(stdout);

    return 0;
}
