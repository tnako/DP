#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 12345 

#define MAX_CON (1000)

int main(int argc, char *argv[])
{
    struct sockaddr_in serveraddr;
    struct epoll_event events[MAX_CON];
    int fdmax;
    int listener;   
    int yes;
    int epfd = -1;
    struct epoll_event ev;

    if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
            perror("Server-socket() error!");
            exit(1);
    }

    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
            perror("Server-setsockopt() error!");
            exit(1);
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(PORT);
    memset(&(serveraddr.sin_zero), '\0', 8);
    if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
            perror("Server-bind() error!");
            exit(1);
    }
    if(listen(listener, 300) == -1)
    {
            perror("Server-listen() error!");
            exit(1);
    }
    fdmax = listener;

    if ((epfd = epoll_create(MAX_CON)) == -1) {
            perror("epoll_create");
            exit(1);
    }
    ev.events = EPOLLIN;
    ev.data.fd = fdmax;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fdmax, &ev) < 0) {
            perror("epoll_ctl");
            exit(1);
    }
    //time(&start);
    for(;;) {
            int res = epoll_wait(epfd, events, MAX_CON, -1);

            for (int index = 0; index < res; index++) {
		    int client_fd = events[index].data.fd;
                    if(client_fd == listener) {
			    struct sockaddr_in clientaddr;
                            socklen_t addrlen = sizeof(clientaddr);

			    int newfd;
                            if((newfd = accept(listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1) {
                                    perror("Server-accept() error!");
                            } else {
				    int flag = fcntl(newfd, F_GETFL, 0);
				    fcntl(newfd, F_SETFL, flag | O_NONBLOCK);
                                    ev.events = EPOLLIN;
                                    ev.data.fd = newfd;
                                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &ev) < 0) {
                                            perror("epoll_ctl");
                                            exit(1);
                                    }
                            }
                    } else {
			    if (events[index].events & EPOLLHUP) {
                                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev);
                                    close(client_fd);
                            } else {
				    char buf[6];
				    int ret = read(client_fd, buf, 6);
                                    if(ret <= 0) {
					    //if (errno == EAGAIN) continue;
					    if (ret != 0) perror("recv");
                                            epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev);
                                            close(client_fd);
                                    } else {
                                            if(write(client_fd, buf, ret) < 0) {
                                                    perror("send() error!");
					    }
                                    }

                            } 
                    }
            }
    }
    return 0;
}