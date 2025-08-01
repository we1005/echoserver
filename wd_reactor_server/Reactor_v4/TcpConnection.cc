#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "InetAddress.hpp"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <sstream>

namespace wd
{
TcpConnection::TcpConnection(int fd, EventLoop * p)
: _sock(fd)
, _socketIo(fd)
, _localAddr(getLocalAddr(fd))
, _peerAddr(getPeerAddr(fd))
, _isShutdwonWrite(false)
, _ploop(p)
{
}

TcpConnection::~TcpConnection()
{
	if(!_isShutdwonWrite) {
		shutdown();
	}
}

string TcpConnection::receive()
{
	char buff[65536] = {0};
    //这里使用的是readline,所以消息的边界为'\n'
	_socketIo.readline(buff, sizeof(buff));
	return string(buff);
}
	
void TcpConnection::send(const string & msg)
{
	_socketIo.writen(msg.c_str(), msg.size());
}

//sendInLoop函数的执行是在计算线程(线程池中的线程)中
void TcpConnection::sendInLoop(const string & msg)
{
    if(_ploop) {
        _ploop->runInLoop(std::bind(&TcpConnection::send, this, msg));
    }
}

void TcpConnection::shutdown()
{
	if(!_isShutdwonWrite) {
		_isShutdwonWrite = true;
		_sock.shutdownWrite();
	}
}

string TcpConnection::toString() const
{
	std::ostringstream oss;
	oss << "tcp " << _localAddr.ip() << ":" << _localAddr.port() << " --> "
		<< _peerAddr.ip() << ":" << _peerAddr.port();
	return oss.str();
}

bool TcpConnection::isClosed() const
{
    char buff[20] = {0};
    int ret = -1;
    do{
        ret = recv(_sock.fd(), buff, sizeof(buff), MSG_PEEK);
    }while(ret == -1 && errno == EINTR);
    return ret == 0;
}

void TcpConnection::handleNewConnectionCallback()
{
    //在本对象成员函数内部获取本对象的智能指针shared_ptr
    //只能通过先继承自辅助类std::enable_shared_from_this
    //来完成
    if(_onConnectionCb) {
        _onConnectionCb(shared_from_this());
    }
}

void TcpConnection::handleMessageCallback()
{
    if(_onMessageCb) {
        _onMessageCb(shared_from_this());
    }

}
    
void TcpConnection::handleCloseCallback()
{
    if(_onCloseCb) {
        _onCloseCb(shared_from_this());
    }
}


InetAddress TcpConnection::getLocalAddr(int fd)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(struct sockaddr);
	if(getsockname(_sock.fd(), (struct sockaddr*)&addr, &len) == -1) {
		perror("getsockname");
	}
	return InetAddress(addr);
}

InetAddress TcpConnection::getPeerAddr(int fd)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(struct sockaddr);
	if(getpeername(_sock.fd(), (struct sockaddr*)&addr, &len) == -1) {
		perror("getpeername");
	}
	return InetAddress(addr);
}

}//end of namespace wd
