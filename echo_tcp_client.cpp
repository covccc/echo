#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "Log.h"
#include "define.h"

#define CLIENT_TOTAL 20

void *clientThread(void *arg){
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd == -1){
        log_msg("socket create failed, pid = %lu", pthread_self());
        pthread_exit((void*)-1);
    }

    if(connect(sfd,(struct sockaddr*)arg,sizeof(struct sockaddr_in)) != 0){
        log_msg("connet failed, pid = %lu", pthread_self());
        pthread_exit((void*)-2);
    }
    while (1){
        srand((unsigned)time(NULL));
        //int payLen = rand() % 300 + 10;
        int payLen = 99999+rand()% 100;
        int expectLen =999+ rand() % 100;
        int sendDataLen = payLen+sizeof(int32_t)*3;
        char *pSendData = (char *)malloc(sendDataLen);
        if(pSendData == NULL){
            log_msg("call malloc failed");
            continue;
        }
        //开始组装数据，最后一段内容不校验，故无需设置
        ClientRequest *pClientRequest = (ClientRequest*)pSendData;
        pClientRequest->magicNum = htonl(1);
        pClientRequest->expectLen = htonl(expectLen);
        pClientRequest->payloadLen = htonl(payLen);
        //printf_bin(pSendData, sendDataLen);
        //回复数据
        int n_snd = 0;
        int retry = 0;
        while(n_snd < sendDataLen){
            int realSendLen = send(sfd, pSendData + n_snd, sendDataLen - n_snd, 0);
            log_msg("sendLend = %d", realSendLen);
            if(realSendLen > 0){
            n_snd += realSendLen;
            }else if(realSendLen < 0 && (errno == EINTR || errno == EAGAIN)){
                if(++retry > 3){    //尝试三次，防止逻辑阻塞
                    break;
                }
                continue;
            }else{
                break;
            }
        }

        //投机取巧下 只为测试服务端
        int n_rcv = 0; //已接收
        char recvBuf[expectLen + 4] = {0};
        while(n_rcv < expectLen + 4){
            int recvLen = recv(sfd, recvBuf, expectLen + 4, 0);
            log_msg("recvLen = %d", recvLen);
            if(recvLen > 0){
                n_rcv += recvLen;
            }else if(recvLen < 0){
                if(errno == EAGAIN ||  errno == EWOULDBLOCK){
                    break;
                }
                close(sfd);
                pthread_exit((void*)-3);
            }else{
                close(sfd);
                pthread_exit((void*)-4);
            }
        }
        ServerResponse *svrResData = (ServerResponse *)recvBuf;
        if(ntohl(svrResData->payloadLen) != expectLen){
            log_msg("*****************client err**********************");
            exit(0);
        }else{
            log_msg("-----------------client succ---------------------");
        }
        if(pSendData != NULL){
            free(pSendData);
            pSendData = NULL;
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv){
    char ch;
    int port = 0;
    char *pSvrIp = NULL;
    while ((ch = getopt(argc, argv, "a:p:")) != EOF){
        if (ch == 'p'){
            port = atoi(optarg);
        }else if(ch == 'a'){
            pSvrIp = optarg;
        }
    }
    
    if (port <= 0 || pSvrIp == NULL){
        log_msg("you must input : ./echo_tcp_client -a [IP] -p [PORT]\n");
        return -1;
    }

    struct sockaddr_in client;
    bzero(&client, sizeof(struct sockaddr_in));
    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    inet_pton(AF_INET, pSvrIp, &client.sin_addr.s_addr);

    pthread_attr_t attr;
    int ret_num = pthread_attr_init(&attr);
    if(ret_num != 0){
        log_msg("attr init failed, %s", strerror(errno));
        return -1;
    }

    ret_num = pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    if(ret_num != 0){
        log_msg("attr setdetachstate failed, %s", strerror(errno));
        return -1;
    }

    for(int i = 0;i < CLIENT_TOTAL; ++i){
        pthread_t pid;
        ret_num = pthread_create(&pid, &attr, clientThread, &client);
        if(ret_num != 0){
           log_msg("pthread create failed, i = %d, errmsg = %s", i, strerror(ret_num));
        }
    }

    while (1){
        sleep(2);
    }
    pthread_attr_destroy(&attr);
    return 0;
}