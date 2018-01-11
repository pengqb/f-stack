#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include "ff_config.h"
#include "ff_api.h"

#define MAX_EVENTS 128

/* kevent set */
struct kevent kevSet;
/* events */
struct kevent events[MAX_EVENTS];
/* kq */
int kq;
int *sockfds;
int nConn = 0;
int nReceiveMsg = 0;
int servPorts;
long lps_count = 0, event_count = 0, event_num = 0;

char html[] = "HTTP/1.1 200 OK\r\n";

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
    /* Wait for events to happen */
    //struct timespec delay;
    //delay.tv_sec = 5;
    //delay.tv_nsec = 0; // 0 ns
    int nevents = ff_kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
    lps_count++;
    if (nevents >= 0) {
        event_count++;
        event_num += nevents;
    }
    if ((lps_count & 0xffff) == 0xffff) {
        printf("lps_count=%ld,e_c=%ld,e_n=%ld,time=%ld\n", lps_count, event_count, event_num, time(NULL));
    }
    int i;
    //printf("events num:%d\n", nevents);
    for (i = 0; i < nevents; ++i) {
        struct kevent event = events[i];
        int clientfd = (int) event.ident;

        //Handle disconnect
        if (event.flags & EV_EOF) {
            //Simply close socket
            ff_close(clientfd);
            nConn--;
            if ((nConn & 0xfff) == 0xfff) {
                printf("nConn=%d,time=%ld\n", nConn, time(NULL));
            }
            //printf("A client has left the server...,fd:%d\n", clientfd);
        } else if (isContains(clientfd, sockfds, servPorts)) {
            int available = (int) event.data;
            do {
                int nclientfd = ff_accept(clientfd, NULL, NULL);
                if (nclientfd < 0) {
                    printf("ff_accept failed:%d, %s\n", errno,
                           strerror(errno));
                    break;
                }

                //Add to event list
                EV_SET(&kevSet, nclientfd, EVFILT_READ, EV_ADD, 0, 0, NULL);

                if (ff_kevent(kq, &kevSet, 1, NULL, 0, NULL) < 0) {
                    printf("ff_kevent error:%d, %s\n", errno,
                           strerror(errno));
                    return -1;
                }
                nConn++;
                if ((nConn & 0xfff) == 0xfff) {
                    printf("nConn=%d,time=%ld\n", nConn, time(NULL));
                }
                //printf("A new client connected to the server..., fd:%d\n", nclientfd);
                available--;
            } while (available);
        } else if (event.filter == EVFILT_READ) {
            char buf[256];
            size_t readlen = ff_read(clientfd, buf, sizeof(buf));
            nReceiveMsg++;
            if ((nReceiveMsg & 0xffff) == 0xffff) {
                printf("nReceiveMsg=%d,time=%ld\n", nReceiveMsg, time(NULL));
            }
            //printf("bytes %zu are available to read...,data:%s,fd:%d\n", (size_t)event.data, buf, clientfd);
            ff_write(clientfd, html, sizeof(html));
        } else {
            printf("unknown event: %8.8X\n", event.flags);
        }
    }
}

int main(int argc, char *argv[]) {
    ff_init(argc, argv);

    int beginPort = 8010;
    int endPort = 8090;
    servPorts = endPort - beginPort;
    int sockfdArray[servPorts];
    assert((kq = ff_kqueue()) > 0);
    for (int i = 0; i < servPorts; i++) {
        int sockfd = ff_socket(AF_INET, SOCK_STREAM, 0);
        printf("sockfd:%d\n", sockfd);
        if (sockfd < 0) {
            printf("ff_socket failed\n");
            exit(1);
        }
        int on = 1;
        ff_ioctl(sockfd, FIONBIO | FIOASYNC, &on);

        struct sockaddr_in my_addr;
        bzero(&my_addr, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(beginPort + i);
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

				int ret = ff_bind(sockfd, (struct linux_sockaddr *) &my_addr, sizeof(my_addr));
        if (ret < 0) {
            printf("ff_bind failed\n");
            exit(1);
        }

        ret = ff_listen(sockfd, MAX_EVENTS);
        if (ret < 0) {
            printf("ff_listen failed\n");
            exit(1);
        }
        sockfdArray[i] = sockfd;
        EV_SET(&kevSet, sockfd, EVFILT_READ, EV_ADD, 0, MAX_EVENTS, NULL);
        /* Update kqueue */
        ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
    }
    sockfds = sockfdArray;
    ff_run(loop, NULL);
    return 0;
}
