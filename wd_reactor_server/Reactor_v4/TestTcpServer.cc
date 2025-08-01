#include "EventLoop.hpp"
#include "TcpServer.hpp"
#include "ThreadPool.h"
#include <unistd.h>
#include <iostream>
#include <functional>
using std::cout;
using std::endl;

ThreadPool threadpool(5, 10);

class Task
{
public:
    Task(const string & msg, wd::TcpConnectionPtr conn)
    : _msg(msg)
    , _conn(conn)
    {}

    //process方法的调用是在线程池中的某一个计算线程中来完成的
    void process()
    {
        cout << "Task::process is running" << endl;
        //消息有了之后，就需要进行处理
        //decode   解码
        //compute  业务逻辑的计算(可以再抽象业务逻辑层)
        //encode   编码
        //send
        //sleep(1);
        //消息的发送一定要依赖于一个TCP连接来完成
        string response = _msg;//通过回显服务测试连接是否成功
        //问题1：以下操作（数据的发送）能不能在这里完成? 能
        //问题2：数据的发送操作应该在这里完成吗？ 不应该
        //如果在这里直接发送消息，那计算线程所做的工作就有点儿冗余，
        //相当于该计算线程即做了计算操作，又做了IO操作，没有做到职责分离
        //
        //问题3： 该如何进行呢？ 应该有计算线程通知IO线程进行数据的发送
        //_conn->send(response);
        _conn->sendInLoop(response);//通知IO线程进行发送
    }

private:
    string _msg;//消息内容
    wd::TcpConnectionPtr _conn;
};


void onConnection(wd::TcpConnectionPtr conn)
{
    cout << conn->toString() << " has connected\n";
}

void onMessage(wd::TcpConnectionPtr conn)
{
    cout << "onMessage is running" << endl;
    //有消息到达
    string msg = conn->receive();
    //当消息到达之后，将其封装成一个任务，交给线程池来处理
    Task task(msg, conn);
    //注意：在绑定参数时，传递的是task对象，而不能传递地址；
    //传递地址的风险在于，onMessage函数执行结束之后，task对象是一个局部对象，
    //会被销毁的
    //
    //要确保当Task::process函数在执行时，task对象的生命周期是没有结束的
    threadpool.addTask(std::bind(&Task::process, task));
}

void onClose(wd::TcpConnectionPtr conn)
{
    cout << conn->toString() << "has closed\n";
}
 
int main(void)
{
    threadpool.start();//让线程池运行起来

    wd::TcpServer server(8000);
    server.setAllCallbacks(onConnection, onMessage, onClose);

    server.start();
	return 0;
}
