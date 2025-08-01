#ifndef __EventLoop_H__
#define __EventLoop_H__

#include <sys/epoll.h>    //C头文件


#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <mutex>

using std::map;
using std::vector;
using std::shared_ptr;
using std::function;


#include <sys/epoll.h>
namespace wd
{
using Functor=std::function<void()>;

class TcpConnection;
using TcpConnectionPtr=shared_ptr<TcpConnection>;
using TcpConnectionCallback=std::function<void(TcpConnectionPtr)>;

class Acceptor;//类的前向声明

class EventLoop
{
public:
    EventLoop(Acceptor & acceptor);
    ~EventLoop();

    void loop();
    void unloop();
    void runInLoop(Functor && cb);

    void setAllCallbacks(TcpConnectionCallback && cb1,
                         TcpConnectionCallback && cb2,
                         TcpConnectionCallback && cb3)
    {
        _onConnectionCb = std::move(cb1);
        _onMessageCb = std::move(cb2);
        _onCloseCb = std::move(cb3);
    }

private:
    int createEpollfd();
    void addEpollReadEvent(int fd);
    void delEpollReadEvent(int fd);

    void waitEpollFd();

    void handleNewConnection();
    void handleMessage(int fd);

    int createEventfd();
    void handleRead();
    void wakeup();

    void doPendingFunctors();

private:
    int _epfd;
    int _eventfd;
    Acceptor & _acceptor;
    bool _isLooping;

    vector<struct epoll_event> _evtArr;

    map<int, TcpConnectionPtr> _conns;

    vector<Functor> _pendingFunctors;
    std::mutex _mutex;

    //这三个函数对象需要在创建好EventLoop对象之后进行注册
    TcpConnectionCallback _onConnectionCb;
    TcpConnectionCallback _onMessageCb;
    TcpConnectionCallback _onCloseCb;
};


}//end of namespace wd



#endif

