#ifndef __WD_SOCKET_H__
#define __WD_SOCKET_H__

#include "Noncopyable.hpp"

namespace wd
{

class Socket
: Noncopyable  //���ഴ���Ķ��󲻿��Խ��и��ƣ�����������
{
public:
	Socket();
	explicit
	Socket(int fd);

    //�����ڲ�ʵ�ֵĺ�������inline����
	int fd() const { return _fd;   }

	void shutdownWrite();//�����Ͽ����ӣ��ر�д��

	~Socket();

private:
	int _fd;
};

}//end of namespace wd


#endif
