#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "TcpConnection.hpp"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
using std::cout;
using std::endl;


void onConnection(wd::TcpConnectionPtr conn)
{
    cout << conn->toString() << " has connected\n";
}

void onMessage(wd::TcpConnectionPtr conn)
{
    //有消息到达
    string msg = conn->receive();
    //decode   解码
    //compute  业务逻辑的计算(可以再抽象业务逻辑层)
    //encode   编码
    //send
    conn->send(msg);//通过回显服务测试连接是否成功

}

void onClose(wd::TcpConnectionPtr conn)
{
    cout << conn->toString() << "has closed\n";
}
 
int main(void)
{
	//wd::Acceptor acceptor("192.168.30.129", 8000);
	wd::Acceptor acceptor(8000);
	acceptor.ready();

    wd::EventLoop loop(acceptor);
    loop.setAllCallbacks(onConnection, onMessage, onClose);
    loop.loop();

	return 0;
}
