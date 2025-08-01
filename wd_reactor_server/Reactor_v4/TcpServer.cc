#include "TcpServer.hpp"
#include <iostream>


using std::cout;
using std::endl;


namespace wd
{

TcpServer::TcpServer(unsigned short port, const string & ip)
: _acceptor(ip, port)
, _loop(_acceptor)
{}

void TcpServer::start()
{
    _acceptor.ready();
    _loop.loop();
}

void TcpServer::stop()
{
    _loop.unloop();
}

}//end of namespace wd

