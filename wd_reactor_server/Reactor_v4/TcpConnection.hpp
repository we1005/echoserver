#ifndef __WD_TCPCONNECTION_H__
#define __WD_TCPCONNECTION_H__

#include "Noncopyable.hpp"
#include "Socket.hpp"
#include "InetAddress.hpp"
#include "SocketIO.hpp"

#include <string>
#include <memory>
#include <functional>
using std::string;
using std::shared_ptr;

namespace wd
{

class EventLoop;
class TcpConnection;
using TcpConnectionPtr=shared_ptr<TcpConnection>;
using TcpConnectionCallback=std::function<void(TcpConnectionPtr)>;

class TcpConnection
: Noncopyable
, public std::enable_shared_from_this<TcpConnection>
{
public:
	TcpConnection(int fd, EventLoop * p);
	~TcpConnection();

	string receive();
    //...接收数据的方式可以在这里进行扩展，封装新的函数
	void send(const string & msg);
    void sendInLoop(const string & msg);

	string toString() const;//获取五元组信息
	void shutdown();
    bool isClosed() const;

    void setAllCallbacks(const TcpConnectionCallback & cb1,
                         const TcpConnectionCallback & cb2,
                         const TcpConnectionCallback & cb3)
    {   
        _onConnectionCb = cb1;//这里发生的是复制
        _onMessageCb = cb2;
        _onCloseCb = cb3;
    }

    void handleNewConnectionCallback();
    void handleMessageCallback();
    void handleCloseCallback();

private:
	InetAddress getLocalAddr(int fd);
	InetAddress getPeerAddr(int fd);
private:
	Socket _sock;
	SocketIO _socketIo;
	InetAddress _localAddr;
	InetAddress _peerAddr;
	bool _isShutdwonWrite;//是否要主动关闭连接
    EventLoop * _ploop;

    TcpConnectionCallback _onConnectionCb;
    TcpConnectionCallback _onMessageCb;
    TcpConnectionCallback _onCloseCb;
};

}//end of namespace wd

#endif
