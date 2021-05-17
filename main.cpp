#include "EchoTcpServer.h"
#include "Log.h"
#include "EpollManager.h"
#include "define.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv){
    char ch;
    int port = 0;
    while ((ch = getopt(argc, argv, "p:")) != EOF){
        if (ch == 'p'){
            port = atoi(optarg);
        }
    }

    if (port <= 0){
        log_msg("you must input : ./echo_tcp_svr -p [PORT]\n");
        return -1;
    }

    EpollManager *epoll = new EpollManager(EPOLL_WAIT_TIME, EPOLL_EVENT_SIZE);
    if(!epoll->Init()){
        log_msg("epoll init failed");
        delete epoll;
        return -1;
    }
    TcpServerByAccept *serverAcc = new TcpServerByAccept(epoll);
    if(!serverAcc->Create(port)){
        log_msg("create socket failed");
        delete epoll;
        delete serverAcc;
        return -1;
    }

    int lastClearTime = time(NULL);
    while (1){
        //处理epoll事件
        epoll->Wait();
        //检查是否有客户端超时
        int val = time(NULL) - lastClearTime;
        if(val > JUDGE_CLEAR_TIME || val < 0){
            serverAcc->JudgeAllClientActive();
            lastClearTime = time(NULL);
        }
    }
    
    //清理工作
    log_msg("start clear");
    serverAcc->OnClose();
    delete epoll;
    delete serverAcc;
}