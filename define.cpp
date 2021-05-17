#include "define.h"

void printf_bin(char *pBuf, int size){
    if(pBuf == NULL || size <= 0){
        return;
    }
    int realSize = size*3+1;
    char rst_data[realSize];
    memset(rst_data,0,realSize*sizeof(char));
    for(int i=0;i<size;i++)
    {
        sprintf(&(rst_data[i*3]),"%02X ",(unsigned char)(pBuf[i]));
    }
    printf("data = %s\n", rst_data);
}

bool setFdNonblock(int fd){
    int fdflags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, fdflags | O_NONBLOCK) >= 0;
}