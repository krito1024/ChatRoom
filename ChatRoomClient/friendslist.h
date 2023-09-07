#ifndef FRIENDSLIST_H
#define FRIENDSLIST_H

#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QQueue>

#include "commond.h"


class friendslist : public QObject
{
    Q_OBJECT
public:
    explicit friendslist(QObject *parent = nullptr);

signals:
public:
    QString username;
    QTcpSocket *m_Tcp;
    QQueue<QString> flist;
public:
    void sendRQ();

};

#endif // FRIENDSLIST_H
