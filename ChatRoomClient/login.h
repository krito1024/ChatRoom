#ifndef LOGIN_H
#define LOGIN_H

#include <QDialog>
#include <QTcpSocket>
#include <QMessageBox>
#include <QNetworkProxy>
#include <string>

#include "mainwindow.h"
#include "commond.h"

QT_BEGIN_NAMESPACE
namespace Ui { class login; }
QT_END_NAMESPACE

class login : public QDialog
{
    Q_OBJECT

public:
    login(QWidget *parent = nullptr);
    ~login();

private slots:
    void on_conn_clicked();//连接服务器

    void on_Register_clicked();

    void on_Login_clicked();




private:
    Ui::login *ui;
public:
    QPixmap pix;//登录图标
    mainwindow *main;//登录后的窗口
    QTcpSocket *m_Tcp;//用于通信的套接字
    QTcpSocket *Tcp;//用于通信的套接字

};
#endif // LOGIN_H
