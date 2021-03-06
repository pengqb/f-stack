//
// Created by ��ǿ�� on 18/1/3.
//
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>

#define MAX_EVENTS 128
int nConn = 0;
int nReceiveMsg = 0;
int servPorts;
struct epoll_event ev;
struct epoll_event events[MAX_EVENTS];

int epfd;
int *sockfds;

char html[] = "HTTP/1.1 200 OK";

int setnonblocking(int sockfd) {
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK | O_ASYNC) == -1) {
        return -1;
    }
    return 0;
}

bool isContains(int elem, int *container, int nElems) {
    bool isContains = false;
    for (int i = 0; i < nElems; i++) {
        if (elem == container[i]) {
            isContains = true;
            break;
        }
    }
    return isContains;
}

int loop(void *arg) {
    for (;;) {
        /* Wait for events to happen */
        int nevents = epoll_wait(epfd, events, MAX_EVENTS, -1);
        int i;
        for (i = 0; i < nevents; ++i) {
            struct epoll_event event = events[i];
            int clientfd = event.data.fd;
            /* Handle new connect */
            if (isContains(clientfd, sockfds, servPorts)) {
                // while (1) {
                int nclientfd = accept(clientfd, NULL, NULL);
                if (nclientfd < 0) {
                    //break;
                    printf("ff_accept failed:%d, %s\n", errno, strerror(errno));
                    continue;
                }

                int ret = setnonblocking(nclientfd);
                if (ret < 0) {
                    printf("setnonblocking failed:%d, %s\n", errno, strerror(errno));
                    continue;
                }
                /* Add to event list */
                ev.data.fd = nclientfd;
                ev.events = EPOLLIN;// | EPOLLET;
                //if (epoll_ctl(epfd, EPOLL_CTL_ADD, nclientfd, &ev) != 0) {
                //    printf("ff_epoll_ctl failed:%d, %s\n", errno,
                //           strerror(errno));
                //    break;
                //}
                assert(epoll_ctl(epfd, EPOLL_CTL_ADD, nclientfd, &ev) == 0);
                if ((++nConn & 0xfff) == 0xfff) {
                    printf("nConn=%d,time=%ld\n", nConn, time(NULL));
                }
                //printf("A new client connected to the server..., fd:%d\n", nclientfd);
                // }
            } else {
                if (event.events & EPOLLERR) {
                    /* Simply close socket */
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, NULL);
                    close(clientfd);
                    if ((--nConn & 0xfff) == 0xfff) {
                        printf("nConn=%d,time=%ld\n", nConn, time(NULL));
                    }
                    //printf("A client has left the server...,fd:%d\n", events[i].data.fd);
                } else if (event.events & EPOLLIN) {
                    char buf[MAX_EVENTS];
                    size_t readlen = read(clientfd, buf, sizeof(buf));
                    //printf("bytes are available to read, readlen:%d,data:%s, fd:%d\n", readlen,buf, events[i].data.fd);
                    if (readlen > 0) {
                        if ((++nReceiveMsg & 0x3fff) == 0x3fff) {
                            printf("nReceiveMsg=%d,time=%ld\n", nReceiveMsg, time(NULL));
                        }
                        write(events[i].data.fd, html, sizeof(html));
                    } else {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                        close(events[i].data.fd);
                        if ((--nConn & 0xfff) == 0xfff) {
                            printf("nConn=%d,time=%ld\n", nConn, time(NULL));
                        }
                        //printf("A client has left the server...,fd:%d\n", events[i].data.fd);
                    }
                } else {
                    printf("unknown event: %8.8X\n", events[i].events);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("usage: %s <begin port> <end port> <subprocesses> <management port>\n", argv[0]);
        return 1;
    }
    int beginPort = atoi(argv[1]);
    int endPort = atoi(argv[2]);
    int processes = atoi(argv[3]);
    int man_port = atoi(argv[4]);
    printf("begin_port: %d end_port %d processes: %d man_port %d\n", beginPort, endPort, processes, man_port);
    servPorts = endPort - beginPort;
    int sockfdArray[servPorts];
    assert((epfd = epoll_create(256)) > 0);
    for (int i = 0; i < servPorts; i++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        printf("sockfd:%d\n", sockfd);
        if (sockfd < 0) {
            printf("ff_socket failed\n");
            exit(1);
        }

        struct sockaddr_in my_addr;
        bzero(&my_addr, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(beginPort + i);
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        int ret = bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr));
        if (ret < 0) {
            printf("ff_bind failed\n");
            exit(1);
        }

        ret = listen(sockfd, MAX_EVENTS);
        if (ret < 0) {
            printf("ff_listen failed\n");
            exit(1);
        }
        sockfdArray[i] = sockfd;
        ev.data.fd = sockfd;
        ev.events = EPOLLIN;// | EPOLLET;
        epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
    }
    sockfds = sockfdArray;
    loop(0);
    return 0;
}


