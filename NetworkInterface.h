#ifndef __NETWORK_INTERFACE_H__
#define __NETWORK_INTERFACE_H__

class NetworkInterface{ //网络接口必须继承这个类，并实现下面三个接口
public:
    NetworkInterface(/* args */){};
    ~NetworkInterface(){};
    virtual void OnRead() = 0;
    virtual void OnWrite() = 0;
    virtual void OnClose() = 0;
};

#endif