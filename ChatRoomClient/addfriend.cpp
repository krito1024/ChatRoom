#include "addfriend.h"
#include "ui_addfriend.h"

addfriend::addfriend(QString username, QWidget *parent) :
    username(username),
    QDialog(parent),
    ui(new Ui::addfriend)
{
    ui->setupUi(this);
    //创建m_Tcp
    m_Tcp = new QTcpSocket();
    m_Tcp->setProxy(QNetworkProxy::NoProxy);
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    m_Tcp->connectToHost(QHostAddress(ip), port);

    //接收数据
    connect(m_Tcp, &QTcpSocket::readyRead, this, [=](){
        QByteArray data =m_Tcp->readAll();
        QString msg = data.data();
        qDebug() << "添加好友类收到数据了";
        if(msg == "查无此人"){
            qDebug() << "显示了查无此人";
            QMessageBox::warning(this, "添加好友", "查无此人");
        }
        else if(msg == "你们已经是好友了!"){
            QMessageBox::warning(this, "添加好友", "你们已经是好友了!");
        }
        else if(msg == "添加信息已发送"){
            QMessageBox::information(this, "添加好友", "添加信息已发送");
            this->close();
        }
    });

}

addfriend::~addfriend()
{
    delete ui;
}

void addfriend::closeEvent(QCloseEvent *event)
{
    m_Tcp->disconnectFromHost();
    m_Tcp->waitForDisconnected();
    event->accept();
}

void addfriend::on_send_clicked()
{
    QString toname = ui->toname->text();
    QString fromname = username;
    QString msg(ADDFRIENDS);
    msg += fromname;
    msg += "+";
    msg += toname;
    qDebug() << msg;
    int len = addfriend::m_Tcp->write(msg.toUtf8());
    if(len == -1){
        qDebug() << "传输出错";
    }
}
