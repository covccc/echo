#ifndef __DEFINE_H__
#define __DEFINE_H__

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>

#define RECV_BUFF_LEN 1024      //接收缓冲区大小
#define CLIENT_OVER_TIME 50     //秒 多少秒算超时
#define JUDGE_CLEAR_TIME 10     //秒 多少秒调用一次判断清理函数
#define LISTEN_NUM 20           //listen的第二个参数
#define EPOLL_WAIT_TIME 3000    //epoll wait  秒
#define EPOLL_EVENT_SIZE 100    //epoll event 大小

struct ClientRequest{
    int32_t payloadLen;         //本次请求中的payload长度 网络序
    int32_t expectLen;          //本次请求期望服务端响应的数据长度 网络序
    int32_t magicNum;           //机器码 网络序
    char payload[0];            //包体
}__attribute__((packed));

struct ServerResponse{
    int32_t payloadLen;         //响应payload的长度，与exceptLen一致 网络序
    char payload[0];            //包体
}__attribute__((packed));

enum {
    NET_EVENT_READ = 1,     //1=epollin
    NET_EVENT_WRITE = 4,    //4 == epollout
    NET_EVENT_ERROR = 8     //8 == epollerr
};

enum {
    NET_CTL_ADD = 1,
    NET_CTL_DEL = 2,
	NET_CTL_MOD = 4
};

void printf_bin(char *pBuf, int size);
bool setFdNonblock(int fd);

#endif