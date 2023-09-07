#ifndef REACTOR_H_
#define REACTOR_H_

#include <iostream>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <list>
#include <set>
#include <pthread.h>
#include <queue>
#include <mysql.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sstream>
#include <time.h>
#include <iomanip>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "command.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/basic_file_sink.h"

class Reactor{
    private:
        //服务器监听套接字
        int server_fd = 0;
        //epoll句柄
        int epoll_fd = 0;
        //决定程序是否停止
        bool m_bStop = false;
        //在线状态
        int onlineState = 0;
        //创建线程id
        pthread_t accept_t;//连接接受信息线程
        pthread_t broadcast_t;//广播线程
        pthread_t work_t[WORK_NUM];//工作线程

        std::list<int> m_clientlist;//存放用于通信的客户端文件描述符
        pthread_mutex_t mutex_m_clientlist = PTHREAD_MUTEX_INITIALIZER;//m_client的互斥锁
        pthread_cond_t cond_m_clientlist = PTHREAD_COND_INITIALIZER;//m_client的条件变量

        std::set<int> m_fds;//存放accept后的文件描述符
        pthread_mutex_t mutex_m_fds = PTHREAD_MUTEX_INITIALIZER;//m_fds的互斥锁
        pthread_cond_t cond_m_fds = PTHREAD_COND_INITIALIZER;//m_fds的条件变量

        std::list<std::string> m_clientname; //用于存放在线的客户端名字，方便之后对数据库的在线状态进行修改
        pthread_mutex_t mutex_clientname = PTHREAD_MUTEX_INITIALIZER;//对m_clientname进行操作的互斥锁

        std::queue<std::string> m_msgs;//用于存放发送的消息的队列

        pthread_mutex_t mutex_accept = PTHREAD_MUTEX_INITIALIZER;//用于接收客户端的互斥锁
        pthread_cond_t cond_accept = PTHREAD_COND_INITIALIZER;

        pthread_mutex_t mutex_send = PTHREAD_MUTEX_INITIALIZER;//用于发送消息的互斥锁
        pthread_cond_t cond_send = PTHREAD_COND_INITIALIZER;

        pthread_mutex_t mutex_sql = PTHREAD_MUTEX_INITIALIZER;//用于控制数据库的访问

        //MySQL数据库
        MYSQL *mysql;
        MYSQL_RES *sqlRes;
        MYSQL_ROW sqlrow;

    public:
        Reactor();
        ~Reactor();

        void* main_loop(Reactor *p);
        bool init(const char* ip, unsigned short port);
        bool uninit(void *p);
        bool close_client(int client_fd);

        //数据库操作
        int connect();//数据库连接
        int sqlQuery(const char* sql);//执行sql语句

    private:
        static void *accept_thread_proc(void* args);//接收信息线程工作函数
        static void *worker_thread_proc(void* args);//
        static void *send_thread_proc(void* args);//发送信息线程工作函数
        bool create_server_listener(const char* ip, short port);//创建服务器监听
};

#endif