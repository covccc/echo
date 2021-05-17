#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <cerrno>
#include "Log.h"

int main(int argc, char** argv){
    char ch;
    int port = 0;
    while ((ch = getopt(argc, argv, "p:")) != EOF){
        if (ch == 'p'){
            port = atoi(optarg);
        }
    }

    if (port == 0){
        printf("bind port==0, you must input --> ./echo_udp_svr -p [PORT]\n");
        return -1;
    }
    

    int lfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(lfd == -1){
        perror("socket create err");
        return -1;
    }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(lfd,(struct sockaddr*)&serv,sizeof(struct sockaddr)) == -1){
        perror("bind err");
        return -1;
    }
    
    char buf[256]={0};
    struct sockaddr client_addr;
    int addr_len=sizeof(struct sockaddr_in);
    
    while(1){
        int recv_len = recvfrom(lfd, buf, 256, 0, &client_addr, (socklen_t*)&addr_len);
        if (recv_len > 0){
            for(;;){
                int send_len = sendto(lfd, buf, recv_len, 0, &client_addr, addr_len);
                if(send_len > 0){
                    break;
                }else if(send_len == -1 && (errno == EAGAIN ||  errno == EWOULDBLOCK)){//阻塞不会产生
                    sleep(1);
                    continue;
                }else if(recv_len == EINTR){
                    continue;
                }else{
                    break;
                }
            }
        }else if(recv_len == -1 && (errno == EAGAIN ||  errno == EWOULDBLOCK)){ //资源阻塞，缓存区满了
            sleep(1);
            continue;
        }else if(recv_len == EINTR){
            continue;
        }else if(recv_len < 0){
            break;
        }
    }
    printf("socket close");
    close(lfd);
    return 0;
}