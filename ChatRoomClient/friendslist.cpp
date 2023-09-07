#include "friendslist.h"

friendslist::friendslist(QObject *parent)
    : QObject{parent}
{
    //创建m_Tcp
    m_Tcp = new QTcpSocket();
    m_Tcp->setProxy(QNetworkProxy::NoProxy);
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    m_Tcp->connectToHost(QHostAddress(ip), port);
    //收到数据时
    connect(m_Tcp, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_Tcp->readAll();
        QString msg = data.data();
        if(msg.contains(":", Qt::CaseSensitive)){
            int count = msg.mid(msg.size() - 1, 1).toInt();
            for(int i = 0; i < count; i++){
                QString name = msg.section(":", i, i);
                flist.push_back(name);
            }
        }
    });
}

void friendslist::sendRQ()
{
    QString msg(FRIENDSLIST);
    msg += username;
    m_Tcp->write(msg.toUtf8());
}
