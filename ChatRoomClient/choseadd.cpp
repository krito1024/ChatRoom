#include "choseadd.h"
#include "ui_choseadd.h"

choseadd::choseadd(QString fromname, QString username, QWidget *parent) :
    fromname(fromname),
    toname(username),
    QDialog(parent),
    ui(new Ui::choseadd)
{
    ui->setupUi(this);
    //连接服务器
    m_Tcp = new QTcpSocket();
    m_Tcp->setProxy(QNetworkProxy::NoProxy);
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    m_Tcp->connectToHost(QHostAddress(ip), port);
}

choseadd::~choseadd()
{
    delete ui;
}

void choseadd::closeEvent(QCloseEvent *event)
{
    m_Tcp->disconnectFromHost();
    m_Tcp->waitForDisconnected();
    event->accept();
}

void choseadd::on_accept_clicked()
{
    QString msg(ACCEPT);
    msg += fromname + "\t" + toname;
    m_Tcp->write(msg.toUtf8());
    this->close();
}


void choseadd::on_pushButton_2_clicked()
{
    QString msg(REFUSE);
    msg += fromname;
    m_Tcp->write(msg.toUtf8());
    this->close();
}

