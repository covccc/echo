#ifndef __EPOLL_MANAGER_H__
#define __EPOLL_MANAGER_H__

#include <sys/epoll.h>
#include "NetworkInterface.h"
#include "define.h"

class EpollManager{
public:
    EpollManager(int waitTime = -1, int eventSize = 50);
    ~EpollManager();
    bool Init();
    bool Ctrl(int fd, int type, int filter, NetworkInterface *network);
    void Wait();

private:
    int m_fd;
    int m_waitTime;
    int m_eventSize;
    struct epoll_event* m_pEvents;
};

#endif