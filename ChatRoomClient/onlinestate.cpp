#include "onlinestate.h"

onlinestate::onlinestate(QObject *parent)
    : QObject{parent}
{
    //创建m_TcpStateUpdateDone
    m_Tcp = new QTcpSocket();
    m_Tcp->setProxy(QNetworkProxy::NoProxy);
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    m_Tcp->connectToHost(QHostAddress(ip), port);

    connect(m_Tcp, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_Tcp->readAll();
        QString msg = data.data();
        qDebug() << "online类里面的msg:" << msg;
        if(msg.contains(":")){
            int num = msg.mid(msg.size() - 1, 1).toInt();
            qDebug() << "num:" << num;
            for(int i = 0; i < num; i++){
                QString big_section = msg.section(':', i, i);
                qDebug() << "big_section:" << big_section;
                friend_msg temp_friend;
                temp_friend.username = big_section.section(',', 0, 0);
                temp_friend.online = big_section.section(',', 1, 1).toInt();
                temp_friend.pic = big_section.section(',', 2, 2).toInt();
                friendOnlineState.push_back(temp_friend);
            }
            emit StateUpdateDone();
        }
        m_Tcp->disconnectFromHost();
    });
}

void onlinestate::getfriendOnlineState()//向服务端发送需要查询在线状态
{
    m_Tcp->setProxy(QNetworkProxy::NoProxy);
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    m_Tcp->connectToHost(QHostAddress(ip), port);
    QString msg(ONLINESTATE);
    m_Tcp->write(msg.toUtf8());
}
