#ifndef COMMAND_H_
#define COMMAND_H_

#define IP "127.0.0.1"
#define m_ip "0.0.0.0"
#define m_port 8888
#define WORK_NUM 5//工作线程数
#define MAX_EVENTS 1024//最大事件监听数
#define BUFF_NUM 1024//数据缓冲区大小
#define REGISTER    '1'
#define LOGIN       '2'
#define MESSAGE     '3'
#define ADDFRIEND   '4'
#define ADDREQUEST  '5'
#define ACCEPT		'6'
#define REFUSE		'7'
#define FRIENDSLIST	'8'
#define ONLINE      '9'
#define ONLINESTATE	'o'
#define PICCHANGE	'p'
#define INITPIC     'I'
#endif // COMMAND_H
