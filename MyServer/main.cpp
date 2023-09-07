#include "Reactor.h"

int main(){
    Reactor m_reactor;
    m_reactor.init(m_ip, m_port);//初始化服务器,绑定，连接数据库，创建线程
    m_reactor.main_loop(&m_reactor);//设置epoll监听
    return 0;
}