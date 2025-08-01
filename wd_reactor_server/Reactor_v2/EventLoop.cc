#include "EventLoop.hpp" //自定义头文件
#include "Acceptor.hpp"
#include "TcpConnection.hpp"

#include <string.h>
//#include <sys/epoll.h>    //C头文件

#include <iostream>      //C++头文件


//#include <log4cpp/Categroy.hh> // 第三方库头文件

using std::cout;
using std::endl;


#define MAX_EVENTS 1000
#define EPOLL_TIMEOUT 5000

namespace wd
{
EventLoop::EventLoop(Acceptor & acceptor)
: _epfd(createEpollfd())
, _acceptor(acceptor)
, _isLooping(false)
, _evtArr(MAX_EVENTS)
{
    //epoll要监听服务器的listenfd
    addEpollReadEvent(_acceptor.fd());
}


void EventLoop::loop()
{
    _isLooping = true;
    while(_isLooping) {
        waitEpollFd();
    }

}
    
void EventLoop::unloop()
{
    _isLooping = false;
}

void EventLoop::waitEpollFd()
{
    int nready = epoll_wait(_epfd, _evtArr.data(), _evtArr.size(), EPOLL_TIMEOUT);
    if(nready == -1 && errno == EINTR) {
        return;
    } else if(nready == -1) {
        perror("epoll_wait");
    } else if(nready == 0) {
        printf("epoll timeout\n");
    } else { //nready > 0
        
        for(int i = 0; i < nready; ++i) {
            int fd = _evtArr[i].data.fd;
            if(fd == _acceptor.fd()) {
                //新连接过来了
                handleNewConnection();
            } else {
                //意见建立好的连接
                handleMessage(fd);
            }

        }
    }
}

void EventLoop::handleNewConnection()
{
    //创建TcpConnection对象, 注册回调函数
    int connfd = _acceptor.accept();
    TcpConnectionPtr sp(new TcpConnection(connfd));
    sp->setAllCallbacks(_onConnectionCb, _onMessageCb, _onCloseCb);
    _conns.insert(std::make_pair(connfd, sp));

    //epoll添加对于connfd的监听
    addEpollReadEvent(connfd);
    //新连接建立时，要调用事件处理器
    sp->handleNewConnectionCallback();
}

void EventLoop::handleMessage(int fd)
{   //对于已经建立好的连接的消息的处理
    auto iter = _conns.find(fd);
    if(iter != _conns.end()) {//查找到了TcpConnection对象
        bool isClosed = iter->second->isClosed();
        if(isClosed) {
            delEpollReadEvent(fd);//从epoll的监听红黑树上删除掉
            iter->second->handleCloseCallback();//TCP连接对象还存在时，才能调用回调函数
            _conns.erase(iter);//从TCP连接对象的容器中删除
        } else {
            iter->second->handleMessageCallback();
        }
    }
}


int EventLoop::createEpollfd()
{
    int fd = epoll_create1(0);
    if(fd < 0) {
        perror("epoll_create1");
    }
    return fd;
}

void EventLoop::addEpollReadEvent(int fd)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int ret = epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev);
    if(ret < 0) {
        perror("epoll_ctl");
    }
}

void EventLoop::delEpollReadEvent(int fd)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int ret = epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, &ev);
    if(ret < 0) {
        perror("epoll_ctl");
    }
}

}//end of namespace wd
