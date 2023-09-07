#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QTimer>
#include <QQueue>
#include <QListWidgetItem>
#include <QCloseEvent>
#include <QVector>
#include <pthread.h>
#include <QThread>
#include <QMenu>
#include <QContextMenuEvent>

#include "commond.h"
#include "addfriend.h"
#include "clientchat.h"
#include "choseadd.h"
#include "friendslist.h"
#include "onlinestate.h"

namespace Ui {
class mainwindow;
}

class mainwindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit mainwindow(QString username, QWidget *parent = nullptr);
    ~mainwindow();

    void set_added();
    void closeEvent(QCloseEvent *event);
    void setname();
    void showMenu(const QPoint& pos);

private:
    Ui::mainwindow *ui;
    QPixmap p;
    QVector<QString> PicList;

public:
    QString username;
    QString fromname;
    addfriend* addf;
    clientchat* chat;
    choseadd* chadd;
    QQueue<QString> friendList;//主窗口的朋友列表队列
    QTcpSocket *m_Tcp;
    onlinestate *online;
    int friendsNum;
    QMenu *menu;

    QTimer *getfdlTimer;
    QTimer *onlineStateTimer;
    QTimer *havedoneTimer;

    //QVector<onlinestate::friend_msg> friendOnlineState;
private slots:
    void on_addfriend_clicked();
    void on_clientchat_clicked();
    void on_added_clicked();
    void on_tabWidget_currentChanged(int index);

    void on_changepic_currentIndexChanged(int index);

    void on_actChat_triggered();

    void on_actSendFile_triggered();

signals:

    void main_inited();

public:
    void initfriendlist();
    void initPicList();
    void getPic();
    void initfri();

};

#endif // MAINWINDOW_H
