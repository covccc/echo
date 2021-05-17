#include "EpollManager.h"
#include "Log.h"
#include <stdlib.h>
#include <string.h>

EpollManager::EpollManager(int waitTime, int eventSize){
    m_waitTime = waitTime;
    m_eventSize = eventSize;
    m_pEvents = (struct epoll_event*)malloc(sizeof(struct epoll_event) * m_eventSize);
    m_fd = -1;
}

EpollManager::~EpollManager(){
    if(m_pEvents != NULL){
        free(m_pEvents);
        m_pEvents = NULL;
    }
}

bool EpollManager::Init(){
    if(m_eventSize < 1){
        log_msg("epoll init failed, eventSize = %d", m_eventSize);
        return false;
    }

    m_fd = epoll_create(m_eventSize);
    if(m_fd == -1){
        log_msg("epoll create failed, errinfo = %s", strerror(errno));
        return false;
    }

    return m_pEvents != NULL;
}

bool EpollManager::Ctrl(int fd, int type, int filter, NetworkInterface *network){
    if (m_fd == 0){
        log_msg("epoll init failed, epollfd = %d", m_fd);
        return false;
    }

    struct epoll_event event;
    event.data.ptr = network;
    event.events = type;

    int ctl = 0;
    if(filter == NET_CTL_ADD) {
		ctl = EPOLL_CTL_ADD;
	} else if(filter == NET_CTL_DEL) {
		ctl = EPOLL_CTL_DEL;
	} else {
		ctl = EPOLL_CTL_MOD;
	}

    if(epoll_ctl(m_fd, ctl, fd, &event) == -1){
        log_msg("epoll ctl failed, errinfo = %s", strerror(errno));
        return false;
    }
    return true;
}

void EpollManager::Wait(){
    int changeEventSize = 0;
    changeEventSize = epoll_wait(m_fd, m_pEvents, m_eventSize, m_waitTime);

    if(changeEventSize == -1){
        log_msg("epoll wait failed, errinfo = %s", strerror(errno));
        return;
    }

    for (int i = 0; i < changeEventSize; i++){
        if(m_pEvents[i].events & EPOLLIN){
            ((NetworkInterface *)(m_pEvents[i].data.ptr))->OnRead();
        }else if(m_pEvents[i].events & EPOLLOUT){
            ((NetworkInterface *)(m_pEvents[i].data.ptr))->OnWrite();
        }else if(m_pEvents[i].events & EPOLLERR){
            ((NetworkInterface *)(m_pEvents[i].data.ptr))->OnClose();
        }
    }
}