#include "Reactor.h"

//mysqpdlog
auto mylogger = spdlog::stdout_color_mt("mylog");

Reactor::Reactor(){//构造函数

}
Reactor::~Reactor(){//析构函数

}
struct ARG{//向子线程参数中传递当前对象的结构体
    Reactor *pthis;
};
bool Reactor::init(const char* ip, unsigned short port){//完成服务器初始化操作
    //创建服务器监听
    create_server_listener(ip, port);
    //连接数据库
    connect();
    ARG *arg = new ARG();
    arg->pthis = this;
    //创建连接线程
    pthread_create(&accept_t, NULL, accept_thread_proc, (void *)arg);
    //创建广播线程
    pthread_create(&broadcast_t, NULL, send_thread_proc, (void *)arg);
    //创建工作线程
    for(int i = 0; i < WORK_NUM; i++){
        pthread_create(&work_t[i], NULL, worker_thread_proc, (void *)arg);
    }
}
void* Reactor::main_loop(Reactor *p){//epoll监听
    mylogger->info("主线程id为{}",pthread_self());
    mylogger->info("服务器启动成功...");
    //循环监听
    while(!m_bStop){
        epoll_event evs[MAX_EVENTS];
        int num = epoll_wait(epoll_fd, evs, MAX_EVENTS, -1);
        if(num == -1){
            mylogger->error("epoll_wait failed");
        }
        for(int i = 0; i < num; i++){
            int fd = evs[i].data.fd;
            if(fd == server_fd){//说明有新连接来了
                pthread_cond_signal(&cond_accept);//唤醒accept线程
            }
            else{//说明需要进行消息通信,把处于激活状态的文件描述符放入m_clientlist链表中
                pthread_mutex_lock(&mutex_m_clientlist);
                m_clientlist.push_back(fd);
                pthread_mutex_unlock(&mutex_m_clientlist);
                pthread_cond_signal(&cond_m_clientlist);
            }
        }

    }

}
bool Reactor::uninit(void *p){//关闭服务器端
    m_bStop = true;

    shutdown(server_fd, SHUT_RDWR);
    close(server_fd);
    close(epoll_fd);
    mysql_close(mysql);

    return true;
}
bool Reactor::close_client(int client_fd){//关闭客户端
    if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1){
        mylogger->error("epoll_ctl_del failed");
    }
    close(client_fd);
    return true;
}
void *Reactor::accept_thread_proc(void *args){//accept线程
    //转换参数
    ARG *arg = (ARG *)args;
    Reactor *pReactor = arg->pthis;

    while(!pReactor->m_bStop){
        //加锁同时锁互斥量并且等待条件变量
        pthread_mutex_lock(&pReactor->mutex_accept);
        pthread_cond_wait(&pReactor->cond_accept, &pReactor->mutex_accept);
        //accept
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int cfd = accept(pReactor->server_fd, (sockaddr *)&addr, &addr_len);    
        pthread_mutex_unlock(&pReactor->mutex_accept);

        if(cfd == -1){
            mylogger->error("accept failed");
            continue;
        }  
        //accpet成功，在终端显示一下
        mylogger->info("新客户端连接成功! fd:{}", cfd);
        //设置文件描述符为非阻塞状态
        int flag = fcntl(cfd, F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(cfd, F_SETFL, flag);
        //将连接后的文件描述符放到m_fds中
        pthread_mutex_lock(&pReactor->mutex_m_fds);
        pReactor->m_fds.insert(cfd);
        pthread_mutex_unlock(&pReactor->mutex_m_fds);
        //创建ev结构体
        epoll_event ev;
        ev.data.fd = cfd;
        ev.events = EPOLLIN | EPOLLET | EPOLLHUP;
        //放到epoll树上
        epoll_ctl(pReactor->epoll_fd, EPOLL_CTL_ADD, cfd, &ev);
    }
    return NULL;

    
}
void *Reactor::worker_thread_proc(void *args){//工作线程
    ARG *arg = (ARG *)args;
    Reactor *pReactor = arg->pthis;
    while(!pReactor->m_bStop){
        pthread_mutex_lock(&pReactor->mutex_m_clientlist);
        while(pReactor->m_clientlist.empty()){
            pthread_cond_wait(&pReactor->cond_m_clientlist, &pReactor->mutex_m_clientlist);
        }
        //取出链表最前面的节点
        int cfd = pReactor->m_clientlist.front();
        std::cout << "当前通信的fd:  " << cfd << std::endl;
        pReactor->m_clientlist.pop_front();
        pthread_mutex_unlock(&pReactor->mutex_m_clientlist);
        //获取当前时间
        time_t now = time(NULL);
        struct tm* nowstr = localtime(&now);
        std::ostringstream ostimestr;

        std::string strclientmsg;
        //是否出错的标志
        bool bError = false;
        //接收消息的缓冲数组
        char buff[BUFF_NUM];
        std::string clientmsg;
        //可以进行通信了
        while(1){
            bzero(buff, sizeof(buff));

            int len = recv(cfd, buff, sizeof(buff), 0);
            if(len == -1){//错误情况或者接受完毕
                if(errno == EAGAIN){
                    break;
                }
                else{//出错了，关闭客户端，退出
                    mylogger->error("recv出错,客户端断开连接,fd = {}", cfd);
                    pReactor->close_client(cfd);
                    bError = true;
                    break;
                }
            }
            else if(len == 0){//客户端已断开连接
                //删除掉accept链表里的客户端套接字
                pthread_mutex_lock(&pReactor->mutex_m_fds);
                pReactor->m_fds.erase(cfd);
                pthread_mutex_unlock(&pReactor->mutex_m_fds);
                char sql[100];//用于存放sql语句
                sprintf(sql, "select username from UserInfo where clientfd=%d", cfd);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                if(ret == -1){
                    mylogger->warn("数据库查询用户名失败!");
                }
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
                char name[20];
                bzero(name, sizeof(name));
                std::string str_temp;
                pthread_mutex_lock(&pReactor->mutex_clientname);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                if(!pReactor->m_clientname.empty()){//如果在线用户名字链表不为空的话
                    while(pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes)){
                        for(auto it = pReactor->m_clientname.begin(); it != pReactor->m_clientname.end(); it++){
                            if(pReactor->sqlrow[0] == *it){
                                str_temp = *it;
                                strcpy(name, (*it).c_str());//获取到名字
                                printf("客户端名字:%s, fd:%d\n", name, cfd);
                                //释放一下结果集
                                mysql_free_result(pReactor->sqlRes);
                                bzero(sql, sizeof(sql));
                                sprintf(sql, "select online from UserInfo where username='%s'", name);
                                ret = pReactor->sqlQuery(sql);//查询该名字在线状态
                                if(ret == -1){
                                    mylogger->warn("数据库查询在线状态失败!");
                                }
                                pReactor->sqlRes = mysql_store_result(pReactor->mysql);//获取结果集
                                pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);//从结果集获取一行
                                if(atoi(pReactor->sqlrow[0]) == 1){//如果状态是在线的话
                                    bzero(sql, sizeof(sql));
                                    sprintf(sql, "update UserInfo set online=%d where username='%s'", 0, name);
                                    ret = pReactor->sqlQuery(sql);
                                    if(ret == -1){
                                        mylogger->warn("数据库更新在线状态失败!");
                                    }
                                    pReactor->m_clientname.remove(str_temp);
                                    str_temp.clear();
                                    break;
                                }
                                
                            }
                        }
                    }
                }
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
                mylogger->info("客户端已断开连接,fd:{}", cfd);
                pReactor->close_client(cfd);
                bError = true;
                pthread_mutex_unlock(&pReactor->mutex_clientname);
                break;
            }
            std::cout << "输出当前recv到的buff数据:  " << buff << std::endl;
            if(buff[0] == REGISTER){//如果需要注册
                //std::cout << "进入了注册选项" << std::endl;
                std::string rec(buff);
                auto it = rec.find("\t");
                char name[20];
                bzero(name, sizeof(name));
                strcpy(name, rec.substr(1, it - 1).c_str());//取名字
                //std::cout << "名字是" << name << std::endl;
                char pw[30];
                bzero(pw, sizeof(pw));
                strcpy(pw, rec.substr(it + 1, rec.size()).c_str());//取密码
                //std::cout << "密码是" << pw << std::endl;
                //定义sql语句数组
                char sql[100];
                bzero(sql, sizeof(sql));
                sprintf(sql, "select username from UserInfo where username='%s'", name);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                if(pReactor->sqlrow){
                    //std::cout << "进入了已注册过选项" << std::endl;
                    clientmsg += "用户已注册，可直接登陆";
                }
                else{
                    //std::cout << "进入了未注册过选项" << std::endl;
                    bzero(sql, sizeof(sql));
                    //写一个插入注册的用户数据的sql语句
                    sprintf(sql, "insert into UserInfo value('%s', '%s', %d, %d, %d)", name, pw, cfd, 0, 0);
                    ret = pReactor->sqlQuery(sql);
                    if(ret == -1){
                        mylogger->warn("数据库插入失败");
                    }
                    else{//插入成功了
                        //std::cout << "进入了插入成功选项" << std::endl;
                        bzero(sql, sizeof(sql));
                        sprintf(sql, "create table %s(friend char(30))", name);
                        ret = pReactor->sqlQuery(sql);
                        if(ret == -1){
                            mylogger->warn("创建好友表失败了!");
                        }
                        //把自己也加入到好友列表中
                        bzero(sql, sizeof(sql));
                        sprintf(sql, "insert into %s value('%s')", name, name);
                        ret = pReactor->sqlQuery(sql);
                        if(ret == -1){
                            mylogger->warn("将自己插入好友列表失败了!");
                        }
                        if(mysql_errno(pReactor->mysql))//是否发生了错误
                        {
                            mylogger->error("retrive error {}", mysql_error(pReactor->mysql));
                        }
                        clientmsg += "注册成功";
                        mylogger->info("用户{}注册成功!", name);
                    }
                }
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
            }
            else if(buff[0] == LOGIN){//登陆
                std::string rec(buff);
                auto it = rec.find("\t");
                char name[20];
                bzero(name, sizeof(name));
                strcpy(name, rec.substr(1, it - 1).c_str());//取名字
                char pw[30];
                bzero(pw, sizeof(pw));
                strcpy(pw, rec.substr(it + 1, rec.size()).c_str());//取密码
                char sql[100];
                bzero(sql, sizeof(sql));
                sprintf(sql, "select username from UserInfo where username='%s'", name);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                if(!pReactor->sqlrow){//没有查询到相应的名字
                    //std::cout << "没找到名字\n";
                    clientmsg += "请先进行注册!";
                }
                else{//找到名字了
                    //std::cout << "找到名字了\n";
                    bzero(sql, sizeof(sql));//清空sql数组
                    mysql_free_result(pReactor->sqlRes);//清空结果集
                    sprintf(sql, "select password from UserInfo where username='%s'", name);
                    ret = pReactor->sqlQuery(sql);
                    if(ret == -1){
                        mylogger->warn("查询密码失败!");
                    }
                    pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                    pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                    //std::cout << pReactor->sqlrow[0] << "\n";
                    if(strcmp(pReactor->sqlrow[0], pw) == 0){//如果密码相同
                        //std::cout << "密码相同\n"; 
                        bzero(sql,sizeof(sql));
                        pReactor->m_clientname.push_back(name);//向在线客户端名字链表中加入
                        sprintf(sql, "update UserInfo set clientfd=%d where username='%s'", cfd, name);//更新客户端文件描述符fd
                        ret = pReactor->sqlQuery(sql);
                        if(ret == -1){
                            mylogger->warn("更新文件描述符失败!");
                        }
                        //更新在线状态
                        bzero(sql,sizeof(sql));
                        pReactor->m_clientname.push_back(name);//向在线客户端名字链表中加入
                        sprintf(sql, "update UserInfo set online=%d where username='%s'", 1, name);//更新客户端文件描述符fd
                        ret = pReactor->sqlQuery(sql);
                        if(ret == -1){
                            mylogger->warn("更新在线状态失败!");
                        }
                        clientmsg += "登陆成功";
                        mylogger->info("用户{}登陆成功!", name);
                    }
                    else{//密码错误
                        std::cout << "密码错误\n";
                        clientmsg += "密码错误";
                    }
                    if(mysql_errno(pReactor->mysql))//是否发生了错误
                    {
                        mylogger->error("retrive error {}", mysql_error(pReactor->mysql));
                    }
                }
                mysql_free_result(pReactor->sqlRes);//清空结果集
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
            }
            else if(buff[0] == MESSAGE){//发送消息
                clientmsg += buff;
            }
            else if(buff[0] == ADDFRIEND){//添加朋友
                std::cout << "进入最开始添加朋友了" << std::endl;
                std::string rec(buff);
                char fromname[20], toname[20];//添加朋友的人，被添加的人
                bzero(fromname, sizeof(fromname));
                bzero(toname, sizeof(toname));
                auto it = rec.find('+');
                strcpy(fromname, rec.substr(1, it - 1).c_str());
                strcpy(toname, rec.substr(it + 1, sizeof(rec)).c_str());
                char sql[100];
                bzero(sql, sizeof(sql));
                sprintf(sql, "select username from UserInfo where username='%s'", toname);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                if(ret == -1){
                    mylogger->warn("查询被加人失败!");
                }
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
                if(!pReactor->sqlrow){
                    clientmsg += "查无此人";
                }
                else{//有这个人
                    std::cout << "进入有这个人了" << std::endl;
                    bzero(sql, sizeof(sql));
                    sprintf(sql, "select friend from %s where friend='%s'", fromname, toname);
                    pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                    ret = pReactor->sqlQuery(sql);
                    if(ret == -1){
                        mylogger->warn("查询朋友列表失败");
                    }
                    pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                    pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                    mysql_free_result(pReactor->sqlRes);
                    pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
                    if(pReactor->sqlrow){
                        clientmsg += "你们已经是好友了!";
                    }
                    else{
                        std::cout << "进入添加朋友了" << std::endl;
                        bzero(sql, sizeof(sql));
                        sprintf(sql, "select clientfd from UserInfo where username='%s'", toname);
                        pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                        ret = pReactor->sqlQuery(sql);
                        if(ret == -1){
                            mylogger->warn("查询被加人fd失败!");
                        }                   
                        pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                        pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                        mysql_free_result(pReactor->sqlRes);
                        pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
                        int tx_fd = atoi(pReactor->sqlrow[0]);
                        std::cout << "被加人姓名:" << toname << "fd:" << tx_fd << std::endl;
                        std::string msg("*adDR.");
                        std::string temp_str;
                        msg += fromname;
                        msg += "+";
                        msg += toname;
                        send(tx_fd, msg.c_str(), msg.size(), 0);  
                        clientmsg += "添加信息已发送"; 
                        mylogger->info("添加好友信息已发送");                     
                    }
                }
            }
            else if(buff[0] == PICCHANGE){//更换头像
                std::string rec(buff);
                auto it = rec.find(':');
                char name[20];
                strcpy(name, rec.substr(1, it - 1).c_str());
                char pic = *rec.substr(it + 1, rec.size()).c_str();
                std::cout << "姓名:" << name << "头像号:" << pic << std::endl;
                char sql[100];
                bzero(sql, sizeof(sql));
                sprintf(sql, "update UserInfo set pic=%c where username='%s'", pic, name);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
                if(ret == -1){
                    mylogger->warn("头像更新失败!");
                }
                else{
                    mylogger->info("用户{}头像更新成功!", name);
                }
            }
            else if(buff[0] == INITPIC){//将头像号发给客户端
                std::string rec(buff);
                char name[20];
                bzero(name, sizeof(name));
                strcpy(name, rec.substr(1, rec.size()).c_str());
                //std::cout << "name:" << name <<std::endl;
                char sql[100];
                bzero(sql, sizeof(sql));
                sprintf(sql, "select pic from UserInfo where username='%s'", name);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                if(ret == -1){
                    mylogger->warn("查询头像失败!");
                }
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                //std::cout << "pReactor->sqlrow[0]:" << pReactor->sqlrow[0] << std::endl;
                char pic[5];
                strcpy(pic, pReactor->sqlrow[0]);
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
                //std::cout << "pic:" << pic << std::endl;
                clientmsg += "*PIC." + std::string(1, pic[0]);
                //std::cout << "clientmsg:" << clientmsg <<std::endl;
            }
            else if(buff[0] == ACCEPT){//接受
				std::string rec(buff);
				auto it = rec.find('\t');
				char fromname[20];
				char toname[20];
				char sql1[100];
                bzero(sql1, sizeof(sql1));
				char sql2[100];
                bzero(sql2, sizeof(sql2));
				char sql[100];
                bzero(sql, sizeof(sql));
				int ret;
				strcpy(fromname,rec.substr(1,it-1).c_str());
				strcpy(toname,rec.substr(it+1,rec.size()).c_str());
				std::cout << "fromname " << fromname << std::endl;
				std::cout << "toname " << toname << std::endl;
				sprintf(sql1,"insert into %s values('%s')",fromname,toname);
				sprintf(sql2,"insert into %s values('%s')",toname,fromname);
				sprintf(sql,"select clientfd from UserInfo where username='%s'",fromname);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
				ret = pReactor->sqlQuery(sql1);
                if(ret == -1){
                    mylogger->warn("向{}插入数据{}时出错", fromname,toname);
                }
				ret = pReactor->sqlQuery(sql2);
                if(ret == -1){
                    mylogger->warn("向{}插入数据{}时出错", toname,fromname);
                }
				int sel_ret = pReactor->sqlQuery(sql);
                if(sel_ret == -1){
                    mylogger->error("查找添加人fd时出错");
                }
				pReactor->sqlRes = mysql_store_result(pReactor->mysql);
				pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
				int tx_fd = atoi(pReactor->sqlrow[0]);
				if(ret == -1)
                {
                     mylogger->error("insert error {}", mysql_error(pReactor->mysql));
                }
				std::string str="*ACCEPT.";
				send(tx_fd,str.c_str(),str.size(),0);
				clientmsg += buff;	               
            }
            else if(buff[0] == REFUSE){//拒绝
                std::string rec(buff);
                char fromname[20];
                bzero(fromname, sizeof(fromname));
                strcpy(fromname, rec.substr(1, rec.size()).c_str());
                std::cout << fromname << std::endl;
                char sql[100];
                sprintf(sql, "select clientfd from UserInfo where username='%s'", fromname);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                if(ret == -1){
                    mylogger->warn("查询加好友人fd失败!");
                }
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes);
                int tx_fd = atoi(pReactor->sqlrow[0]);
                std::string msg;
                msg += "对方拒绝了您的申请";
                send(tx_fd, msg.c_str(), msg.size(), 0);
                mylogger->info("拒绝消息发送成功!");
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
            }
            else if(buff[0] == FRIENDSLIST){//朋友列表
                char sql[100];
                char name[20];
                bzero(name, sizeof(name));
                bzero(sql, sizeof(sql));
                std::string rec(buff);
                strcpy(name, rec.substr(1, rec.size()).c_str());
                sprintf(sql, "select * from %s", name);
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                if(ret == -1){
                    mylogger->warn("查询朋友列表失败!");
                }
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                int row = (int)mysql_num_fields(pReactor->sqlRes);//统计一下朋友总数
                //std::cout << "row:" << row << std::endl; 
                std::string friendmsg;
                std::string temp_str;
                int count = 0;
                while(pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes)){
                    for(int i = 0; i < row; i++){
                        //std::cout << "名字：" << pReactor->sqlrow[i] << std::endl;
                        friendmsg += pReactor->sqlrow[i];
                        friendmsg += ":";
                        //std::cout << "friendmsg:" << friendmsg << std::endl;
                        temp_str += friendmsg;
                        //std::cout << "temp_str:" << temp_str << std::endl;
                        friendmsg.clear();
                    }
                    count++;
                }
                char c[5];
                sprintf(c, "%d", count);
                temp_str += c;
                std::cout << temp_str << std::endl;
                send(cfd, temp_str.c_str(), temp_str.size(), 0);
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
            }
            else if(buff[0] == ONLINESTATE){//在线
                std::cout << "进入在线了" << std::endl;
                char sql[100];
                bzero(sql, sizeof(sql));
                sprintf(sql, "select username,online,pic from UserInfo");
                pthread_mutex_lock(&pReactor->mutex_sql);//数据库加锁
                int ret = pReactor->sqlQuery(sql);
                if(ret == -1){
                    mylogger->warn("查询在线状态失败!");
                }
                pReactor->sqlRes = mysql_store_result(pReactor->mysql);
                std::string username;
                std::string online;
                std::string pic;
                std::string allmsg;
                std::string temp_msg;
                int count = 0;
                while(pReactor->sqlrow = mysql_fetch_row(pReactor->sqlRes)){
                    username = pReactor->sqlrow[0];
                    online = pReactor->sqlrow[1];
                    pic = pReactor->sqlrow[2];
                    temp_msg = username + "," + online + "," + pic + ",:";
                    allmsg += temp_msg;
                    username.clear();
                    online.clear();
                    pic.clear();
                    count++;
                }
                char c[10];
                sprintf(c, "%d", count);
                allmsg += c;
                std::cout << "输出在线状态信息：" << allmsg << std::endl;
                send(cfd, allmsg.c_str(), allmsg.size(), 0);
                mysql_free_result(pReactor->sqlRes);
                pthread_mutex_unlock(&pReactor->mutex_sql);//数据库解锁
            }
            else{
                clientmsg += buff;
            }
        }
        if(bError){//出错就直接跳过
            continue;
        }
        if(clientmsg[0]==REGISTER||clientmsg[0]==LOGIN||clientmsg[0]==MESSAGE||clientmsg[0]==ADDFRIEND||clientmsg[0]==ACCEPT||clientmsg[0]==INITPIC)
		{
        	mylogger->info("服务器说: {}", clientmsg.substr(1,clientmsg.size()-1));
		}
		else 
		{
			 if(clientmsg!="")
			{
       		 mylogger->info("服务器说: {}", clientmsg);
			}
		}

        if(clientmsg[0] == MESSAGE||clientmsg[0] == ACCEPT)
        {
            clientmsg.erase(0, 1); //将命令标识符去掉
            /* 将消息加上时间戳 */
            ostimestr << "[" << nowstr->tm_year + 1900 << "-"
                << std::setw(2) << std::setfill('0') << nowstr->tm_mon + 1 << "-"
                << std::setw(2) << std::setfill('0') << nowstr->tm_mday << " "
                << std::setw(2) << std::setfill('0') << nowstr->tm_hour << ":"
                << std::setw(2) << std::setfill('0') << nowstr->tm_min << ":"
                << std::setw(2) << std::setfill('0') << nowstr->tm_sec << " ] ";

            clientmsg.insert(0, ostimestr.str());//将时间戳插入到消息字符串的前面
        }
        else//不是交流的消息（注册、登录信息），就直接单独发送给客户端，不用广播
        {
			if(clientmsg!="")
			{
            	send(cfd, clientmsg.c_str(), clientmsg.length(), 0);
			}
            continue;//阶数本次循环，不会再广播
        }

        /* 将消息交给发送广播消息的线程 */
        pReactor->m_msgs.push(clientmsg);
        pthread_cond_signal(&pReactor->cond_send);
    }
    return NULL;
}
void *Reactor::send_thread_proc(void *args){//发送信息线程
        ARG *arg = (ARG *)args;
        Reactor *pReactor = arg->pthis;
        while(!pReactor->m_bStop){
            pthread_mutex_lock(&pReactor->mutex_send);
            while(pReactor->m_msgs.empty()){
                pthread_cond_wait(&pReactor->cond_send, &pReactor->mutex_send);
            }
            std::string msg = pReactor->m_msgs.front();
            pReactor->m_msgs.pop();
            pthread_mutex_unlock(&pReactor->mutex_send);
            //遍历，给所有用户发送信息
            int ret;
            for(auto it:pReactor->m_fds){
                std::cout << "当前fd:" << it << std::endl;
                ret = send(it, msg.c_str(), msg.size(), 0);
                if(ret == -1){
                    if(errno == EAGAIN){
                        sleep(10);
                        continue;
                    }
                    else{
                        mylogger->error("向fd:{}发送数据错误!",it);
                        pReactor->close_client(it);
                    }
                }
            }
            mylogger->info("发送的数据为:{}", msg);
        }
        
}
bool Reactor::create_server_listener(const char *ip, short port){//创建服务器监听线程
    //创建服务器套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        mylogger->error("socket create failed");
        return false;
    }
    //绑定
    sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    //设置套接字ip端口复用
    int on = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on));
    if(bind(server_fd, (sockaddr *)&server_addr, sizeof(server_addr)) == -1){
        mylogger->error("socket bind failed");
        return false;
    }
    //监听
    if(listen(server_fd, 128) == -1){
        mylogger->error("socket listen failed");
        return false;
    }
    //创建epoll
    epoll_fd = epoll_create(1);
    epoll_event ev;
    ev.data.fd = server_fd;
    ev.events = EPOLLIN | EPOLLHUP;
    //将服务器监听文件被描述符加入epoll
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1){
        mylogger->error("epoll_ctl faild");
        return false;
    }
    return true;
}
int Reactor::connect(){//连接数据库
    //初始化数据库
    mysql = mysql_init(NULL);
    if(!mysql){
        mylogger->error("服务器初始化失败!");
    }
    mysql = mysql_real_connect(mysql, IP, "root", "1074305390", "ChatRoom", 0, NULL, 0);
    if(mysql){
        mylogger->info("数据库连接成功!");
    }
    else{
        mylogger->warn("数据库连接失败!");
    }
    return 0;
}
int Reactor::sqlQuery(const char *sql){//执行sql语句
    int ret = mysql_query(mysql, sql);
    if(ret){//执行失败
        return -1;
    }
    return 0;
}
