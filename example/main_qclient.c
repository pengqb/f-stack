//
// Created by 彭强兵 on 17/12/30.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <zconf.h>
#include <time.h>
#include <errno.h>
#include "ff_api.h"

int nConn = 0;
int nReceiveMsg = 0;
char html[] = "clientsendabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";

int main(int argc, char *argv[]) {
    ff_init(argc, argv);

    char *serIp = "10.11.1.3";
    int beginPort = 8010;
    int endPort = 8050;
    int connNum = 30000;
    int connInterval = 10;//unit second,connInterval秒内建立所有连接
    int hbInterval = 10;//unit second,hbInterval秒内所有连接发送一次心跳
    printf("server ip: %s begin_port: %d end_port %d conn num %d conn interval %d heartbeat interval %d \n",
           serIp, beginPort, endPort,
           connNum, connInterval, hbInterval);
    int servPorts = endPort - beginPort;
    int sockfds[servPorts][connNum];
    struct timeval delay;
    delay.tv_sec = 0;
    delay.tv_usec = 10 * 1000; // 10 ms
    int connBlock = connNum / connInterval / 10;

    int opt = 1;
    for (int l = 0; l < servPorts; l++) {
        int port = beginPort + l;
        struct sockaddr_in my_addr;
        my_addr.sin_family = AF_INET;
        my_addr.sin_addr.s_addr = inet_addr(serIp);//my_addr.sin_addr.s_addr = htonl(0x7f000001);
        my_addr.sin_port = htons(port);
        for (int i = 0; i < connNum; i++) {
            //for (int j = i * connBlock; j < (i + 1) * connBlock && j < connNum; j++) {
            /*分配一个网络通信套接字，监听文件描述符sockfd*/
            int sockfd = ff_socket(PF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                printf("ff_socket failed:%d, %s\n", errno,
                       strerror(errno));
                exit(1);
            }
            //设置端口复用
            ff_setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const void *) &opt, sizeof(opt));
            int ret = ff_connect(sockfd, (struct linux_sockaddr *) &my_addr, sizeof(my_addr));
            if (ret < 0) {
                printf("ff_connect failed:%d, %s\n", errno,
                       strerror(errno));
                exit(1);
            }
            if ((++nConn & 0xfff) == 0xfff) {
                printf("nCConn=%d,time=%ld\n", nConn, time(NULL));
                //select(0, NULL, NULL, NULL, &delay);
            }
            printf("sockfd:%d conn servip: %s port: %d\n", sockfd, serIp, port);
            sockfds[l][i] = sockfd;
            //}
        }
    }

    while (true) {
        int hbBlock = connNum / hbInterval / 10;
        char buf[256];
        for (int l = 0; l < servPorts; l++) {
            for (int i = 0; i < hbInterval * 10; i++) {
                for (int j = i * hbBlock; j < (i + 1) * hbBlock && j < connNum; j++) {
                    ff_write(sockfds[l][j], html, sizeof(html));
                }
                //select(0, NULL, NULL, NULL, &delay);dfa
                for (int k = i * hbBlock; k < (i + 1) * hbBlock && k < connNum; k++) {
                    size_t readlen = ff_read(sockfds[l][k], buf, sizeof(buf));
                    if ((++nReceiveMsg & 0xffff) == 0xffff) {
                        printf("nCReceiveMsg=%d,time=%ld\n", nReceiveMsg, time(NULL));
                    }
                    //printf("bytes %zu are available to read,data:%s,fd:%d\n", (size_t) readlen, buf, sockfds[l][k]);
                }
            }
        }
    }
}