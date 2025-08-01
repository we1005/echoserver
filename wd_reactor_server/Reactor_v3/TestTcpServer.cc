#include "TcpServer.hpp"
#include <unistd.h>
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
    //sleep(1);
    conn->send(msg);//通过回显服务测试连接是否成功

}

void onClose(wd::TcpConnectionPtr conn)
{
    cout << conn->toString() << "has closed\n";
}
 
int main(void)
{
    wd::TcpServer server(8000);
    server.setAllCallbacks(onConnection, onMessage, onClose);

    server.start();
	return 0;
}
