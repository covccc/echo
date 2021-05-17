#ifndef __ECHO_TCP_SERVER_H__
#define __ECHO_TCP_SERVER_H__

#include "NetworkInterface.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "Log.h"
#include "define.h"
#include "EpollManager.h"
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <map>

class TcpServerByRecv;

class TcpServerByAccept : public NetworkInterface{
public:
    TcpServerByAccept(EpollManager *epoll);
    ~TcpServerByAccept();
     
    virtual void OnRead() override;
    virtual void OnWrite() override; 
    virtual void OnClose() override;
    //创建socket bind listen 并将该fd注册到epoll
    bool Create(int port);
    //检测所有客户端活性
    void JudgeAllClientActive();
private:
    int m_sfd; //socket fd
    EpollManager *m_epoll;
    std::map<int, TcpServerByRecv *> m_clients;
};

class TcpServerByRecv : public NetworkInterface{
public:
    TcpServerByRecv (EpollManager *epoll, int afd);
    ~TcpServerByRecv ();

    virtual void OnRead() override;
    virtual void OnWrite() override;
    virtual void OnClose() override;
    
    void CheckActive();
    bool GetCloseState();
private:
    //参数是正常数据且合法数据
    void OnDealWith(ClientRequest *pClientReq);
    //参数是可能粘包的数据 返回值决定是否继续接收该链接数据
    bool DealBondPack(char *buf, int size);
    //验证数据是否合法
    inline bool Islegal(int32_t payloadLen, int32_t expectLen, int32_t magicNum);
    //更新处理时间 超时考虑
    inline void UpdateDealTime();
private:
    int m_afd;              //accept fd

    int m_recvDataLen;      //残余处理数据长度
    char *m_pRecvData;      //残余处理数据地址

    int m_sendDataLen;      //残余发送数据长度
    char *m_pSendData;      //残余发送数据长度

    bool m_isClose;         //是否需要关闭该条链接
    bool m_isOutBufInEpoll; //写缓冲区是否受epoll监管

    time_t m_lastDealTime;  //最近一次处理时间
    EpollManager *m_epoll;  //epoll对象
};


#endif