#include "EchoTcpServer.h"
#include <arpa/inet.h>

TcpServerByAccept::TcpServerByAccept(EpollManager *epoll){
    m_epoll = epoll;
}

TcpServerByAccept::~TcpServerByAccept(){
    std::map<int, TcpServerByRecv *>::iterator it = m_clients.begin();
    while (it != m_clients.end()){
        delete it->second;
        close(it->first);
        it = m_clients.erase(it);
    }
}

void TcpServerByAccept::OnRead(){
    //说明此时有新链接请求建链
    int acceptfd = accept(m_sfd, NULL, NULL);   //后面两个参数是客户端信息
    if (acceptfd == -1){
        log_msg("accept failed, errinfo = %s", strerror(errno));
        return;
    }
    if(!setFdNonblock(acceptfd)){
        log_msg("set fd nonblock failed");
        close(acceptfd);
        return;
    }
    NetworkInterface *server = new TcpServerByRecv(m_epoll, acceptfd);
    if(!m_epoll->Ctrl(acceptfd, NET_EVENT_READ|NET_EVENT_WRITE, NET_CTL_ADD, server)){
        log_msg("epoll Register failed, clientID = %d", acceptfd);
        close(acceptfd);
        delete server;
        return;
    }
    //记录客户端
    std::pair<std::map<int, TcpServerByRecv*>::iterator, bool> ist_ret = 
        m_clients.insert(std::make_pair(acceptfd, (TcpServerByRecv *)server));
    if(!ist_ret.second){
        log_msg("map insert failed");
        m_epoll->Ctrl(acceptfd, 0, NET_CTL_DEL, NULL);
        close(acceptfd);
        delete server;
        return;
    }
}

void TcpServerByAccept::OnWrite(){
}

void TcpServerByAccept::OnClose(){
    m_epoll->Ctrl(m_sfd, 0, NET_CTL_DEL, NULL);
    close(m_sfd);
}

void TcpServerByAccept::JudgeAllClientActive(){
    std::map<int, TcpServerByRecv *>::iterator it = m_clients.begin();
    while (it != m_clients.end()){
        it->second->CheckActive();
        if(it->second->GetCloseState()){
            log_msg("cleat = %d", it->first);
            delete it->second;
            close(it->first);
            it = m_clients.erase(it);
            continue;
        }
        it++;
    }
}

bool TcpServerByAccept::Create(int port){
    int sfd = socket(AF_INET, SOCK_STREAM, 0); //读缓冲区有数据说明有新的连接请求建立
    if(sfd < 0){
        log_msg("socket create failed, errinfo = %s", strerror(errno));
        return sfd;
    }

    struct sockaddr_in serv;
    bzero(&serv, sizeof(struct sockaddr));
    serv.sin_family = AF_INET;
    serv.sin_port = ntohs(port);
    serv.sin_addr.s_addr = ntohl(INADDR_ANY);

    if((bind(sfd, (struct sockaddr*)&serv, sizeof(struct sockaddr)) == -1)){
        log_msg("socket bind failed, errinfo = %s", strerror(errno));
        close(sfd);
        return false;
    }

    if(listen(sfd, LISTEN_NUM) != 0){
        log_msg("socket listen failed, errinfo = %s", strerror(errno));
        close(sfd);
        return false;
    }

    //将socketfd注册到epoll
    if(!m_epoll->Ctrl(sfd, NET_EVENT_READ, NET_CTL_ADD, this)){
        log_msg("epoll Register failed");
        close(sfd);
        return false;
    }

    m_sfd = sfd;
    return true;
}

/*////////////////////////
TcpServerByRecv class code...
////////////////////////*/

TcpServerByRecv ::TcpServerByRecv(EpollManager *epoll, int afd){
    m_afd = afd;
    m_recvDataLen = 0;
    m_pRecvData = NULL;
    m_sendDataLen = 0;
    m_pSendData = NULL;
    m_isClose = false;
    m_isOutBufInEpoll = true;
    m_lastDealTime = 0;
    m_epoll = epoll;
}

TcpServerByRecv ::~TcpServerByRecv(){
    //析构的时候确保没有内存泄漏
    if(m_pRecvData != NULL){
        free(m_pRecvData);
        m_pRecvData = NULL;
    }
    if(m_pSendData != NULL){
        free(m_pSendData);
        m_pSendData = NULL;
    }
}

void TcpServerByRecv::OnRead(){
    char buf[RECV_BUFF_LEN] = {0};
    for(;;){
        int recvLen = recv(m_afd, buf, RECV_BUFF_LEN, 0);
        if(recvLen == -1 && (errno == EAGAIN ||  errno == EWOULDBLOCK)){ // 表示读完成，没有可读的
            break;
        }else if(recvLen == -1 && errno == EINTR){ //被信号打断
            continue;
        }else if(recvLen > 0){
            this->UpdateDealTime();
            if(!this->DealBondPack(buf, recvLen)){
                this->OnClose();
                break;
            }
        }else if(recvLen == 0){ //表示客户端主动断开
            this->OnClose();
            break;
        }else{ //出现未知错误，踢掉客户端并关闭
            this->OnClose();
            break;
        }
    }
}

void TcpServerByRecv::OnWrite(){
    if(m_sendDataLen <= 0){
        //走到这里说明缓冲区的数据已发送完毕，
        //为了避免一直回调这，从epoll树上删掉 -> 修改 -> 只检测读缓冲区即可
        if(m_epoll->Ctrl(m_afd, NET_EVENT_READ, NET_CTL_MOD, this)){
            m_isOutBufInEpoll = false;
        }
        return;
    }
    int n_snd = 0; //已发送长度
    while(n_snd < m_sendDataLen){
        int realSendLen = send(m_afd, m_pSendData + n_snd, m_sendDataLen - n_snd, 0);
        if(realSendLen > 0){
           n_snd += realSendLen;
        }else if(realSendLen == -1 && errno == EINTR){  //被信号打断
            continue;
        }else if(realSendLen == -1 && errno ==EAGAIN){  //写缓冲区满了
            //跳出去，调整长度，等待下次缓冲区可写事件
            break;
        }else{ //出现未知错误 主动关闭
            this->OnClose();
            return;
        }
    }
    if(n_snd == 0){
        //说明一个字节也没发送 返回下次在尝试
        return;
    }
    if(n_snd < m_sendDataLen){
        //说明本次数据未发完
        int unfinishDataLen = m_sendDataLen - n_snd;
        char *pUnfinishSendData = (char *)malloc(unfinishDataLen);
        if(pUnfinishSendData == NULL){
            //分配内存失败 -> 为保护服务器稳定 -> 主动断开客户端
            log_msg("malloc call failed");
            this->OnClose();
            return;
        }
        memcpy(pUnfinishSendData, m_pSendData + n_snd, unfinishDataLen);
        //释放之前的内存
        if(m_pSendData != NULL){
            free(m_pSendData);
        }
        //更新成员变量
        m_pSendData = pUnfinishSendData;
        m_sendDataLen = unfinishDataLen;
    }else{
        //说明发完了 -> 释放内存
        m_sendDataLen = 0;
        if(m_pSendData != NULL){
            free(m_pSendData);
            m_pSendData = NULL;
        }
    }
}

//处理正常且合法数据
void TcpServerByRecv::OnDealWith(ClientRequest *pClientReq){
    if(pClientReq == NULL){
        log_msg("pClientReq is null");
        return;
    }
    int expectLen = ntohl(pClientReq->expectLen);   //客户端期望得到的包体数据长度
    int expectSendLen = expectLen + sizeof(ServerResponse); //客户端期望得到的包体数据总长度
    char* pSendData = (char *)malloc(expectSendLen);
    if(pSendData == NULL){
        log_msg("malloc call failed");
        return;
    }
    //组装数据
    ServerResponse *pSvrRes = (ServerResponse*)pSendData;
    pSvrRes->payloadLen = htonl(expectLen);

    //先监测epoll是否监测写缓冲区 -> 不是 -> 需要注册
    if(!m_isOutBufInEpoll){
        if(m_epoll->Ctrl(m_afd, NET_EVENT_READ|NET_EVENT_WRITE, 
            NET_CTL_MOD, this)){
                m_isOutBufInEpoll = true;
            }
    }
    
    if(m_sendDataLen == 0){
        //说明之前的数据已经发送完毕 -> 内存在发送过程中释放
        m_sendDataLen = expectSendLen;
        m_pSendData = pSendData;
    }else{
        //未发送完毕需要和之前数据拼接
        int realSendLen = m_sendDataLen + expectSendLen;
        char *pRealSendBuf = (char *)malloc(realSendLen);
        if(pRealSendBuf == NULL){
            log_msg("malloc call failed");
            //释放上面分配的内存
            if(pSendData != NULL){
                free(pSendData);
                pSendData = NULL;
            }
            //在返回
            return;
        }
        //拼接数据
        memcpy(pRealSendBuf, m_pSendData, m_sendDataLen);
        memcpy(pRealSendBuf + m_sendDataLen, pSendData, expectSendLen);
        //清理
        if(pSendData != NULL){
            free(pSendData);
            pSendData = NULL;
        }
        if(m_pSendData != NULL){
            free(m_pSendData);
        }
        //更新成员变量
        m_pSendData = pRealSendBuf;
        m_sendDataLen = realSendLen;
    }
}

//处理粘包数据
bool TcpServerByRecv::DealBondPack(char *buf, int size){
    char *pRealData = buf;
    int realDataLen = size;
    //先看之前有没有残余数据 -> 有:拼接 -> 无:直接处理
    if(m_recvDataLen > 0){
        int newDataLen = realDataLen + m_recvDataLen;
        //重新分配内存
        char *pNewData = (char *)malloc(newDataLen);
        if(pNewData == NULL){
            log_msg("malloc call failed");
            return false;
        }
        //拼装
        memcpy(pNewData, m_pRecvData, m_recvDataLen);
        memcpy(pNewData + m_recvDataLen, pRealData, realDataLen);
        //释放上次残留的数据内存
        if(m_pRecvData != NULL){
            free(m_pRecvData);
        }
        //更新局部变量
        pRealData = pNewData;
        realDataLen = newDataLen;
        //更新成员变量
        m_recvDataLen = realDataLen;
        m_pRecvData = pRealData;
    }
    //处理数据
    bool isDeal = false; //记录本次是否处理过数据
    while(realDataLen > sizeof(ClientRequest)){ //先判断是否大于最小长度
        ClientRequest *pClientReq = (ClientRequest *)pRealData;
        if(!Islegal(ntohl(pClientReq->payloadLen), ntohl(pClientReq->expectLen), ntohl(pClientReq->magicNum))){
            log_msg("check data unLegal");
            return false;
        }
        //在判断数据是否完整
        int needDataLen = sizeof(ClientRequest) + ntohl(pClientReq->payloadLen);
        if(realDataLen >= needDataLen){
            //说明至少有一段完整数据
            this->OnDealWith((ClientRequest *)(pRealData));
            //减掉已处理的长度 且 移动指针
            pRealData += needDataLen;
            realDataLen -= needDataLen;
            //更新变量
            isDeal = true;
        }else{
            //说明长度不够，那么就等待下一次数据到来
            break;
        }
    }
    if(realDataLen > 0){
        if(!isDeal && m_recvDataLen > 0){ //避免无效调用malloc
            return true;
        }
        //说明还有数据没有处理完
        //1.先重新分配  
        char *pNewData = (char *)malloc(realDataLen);
        if(pNewData == NULL){
            log_msg("malloc call failed");
            return false;
        }
        memcpy(pNewData, pRealData, realDataLen);
        //2.在释放内存
        if(m_pRecvData != NULL){ //这个指针就是上面残留数据拼接分配的指针
            free(m_pRecvData);
        }
        //3.更新成员变量
        m_pRecvData = pNewData;
        m_recvDataLen = realDataLen;
    }else{
        //说明全部处理完毕 -> 释放内存
        if(m_pRecvData != NULL){
            free(m_pRecvData);
            m_pRecvData = NULL;
        }
        m_recvDataLen = 0;
    }
    return true;
}

void TcpServerByRecv::UpdateDealTime(){
    m_lastDealTime = time(NULL);
}

bool TcpServerByRecv::Islegal(int payloadLen, int expectLen, int magicNum){
    return magicNum == 1 && expectLen >= 0 && payloadLen > 0;
}

void TcpServerByRecv::CheckActive(){
    int val = time(NULL) - this->m_lastDealTime;
    if(val < 0 || val > CLIENT_OVER_TIME){
        OnClose();
    }
}

bool TcpServerByRecv::GetCloseState(){
    return m_isClose;
}

void TcpServerByRecv::OnClose(){
    //从epoll树上剔除 -> 读写缓冲区就不会回调了
    m_epoll->Ctrl(m_afd, 0, NET_CTL_DEL, NULL);
    //释放内存
    if(m_pRecvData != NULL){
        free(m_pRecvData);
        m_pRecvData = NULL;
    }
    m_recvDataLen = 0;
    if(m_pSendData != NULL){
        free(m_pSendData);
        m_pSendData = NULL;
    }
    m_sendDataLen = 0;
    m_isClose = true;
}