#ifndef ONLINESTATE_H
#define ONLINESTATE_H

#include <QObject>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QVector>
#include <pthread.h>

#include "commond.h"

class onlinestate : public QObject
{
    Q_OBJECT
public:
    explicit onlinestate(QObject *parent = nullptr);

signals:
    void StateUpdateDone();
public:
    struct friend_msg{//朋友信息结构体
        QString username;
        int online;
        int pic;
    };
    QTcpSocket *m_Tcp;
    QVector<friend_msg> friendOnlineState;
public:
    void getfriendOnlineState();

};

#endif // ONLINESTATE_H
