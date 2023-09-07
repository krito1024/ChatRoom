#include "clientchat.h"
#include "ui_clientchat.h"

clientchat::clientchat(QString username, QWidget *parent) :
    username(username),
    QDialog(parent),
    ui(new Ui::clientchat)
{
    ui->setupUi(this);
    m_Tcp = new QTcpSocket();
    m_Tcp->setProxy(QNetworkProxy::NoProxy);
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    m_Tcp->connectToHost(QHostAddress(ip), port);
    //当连接上服务器时
    connect(m_Tcp, &QTcpSocket::connected, this, [=](){
        QString welcome_msg(MESSAGE);
        welcome_msg += "用户:" + username + "已进入聊天室";
        m_Tcp->write(welcome_msg.toUtf8());
    });
    //当收到数据时
    connect(m_Tcp, &QTcpSocket::readyRead, this, [=](){
        qDebug() << "聊天室类收到数据了";
        QByteArray data = m_Tcp->readAll();
        QString tx_msg = data.data();
        ui->plainTextEdit->appendPlainText(tx_msg);
    });
    // 当窗口关闭时
    connect(this, &clientchat::closeEvent, this, [=](QCloseEvent *event) {
        m_Tcp->disconnectFromHost();
        m_Tcp->close();
        event->accept();
    });

}

clientchat::~clientchat()
{
    delete ui;
}

void clientchat::on_pushButton_clicked()
{
    //点击发送
    QString msg(MESSAGE);
    if(ui->lineEdit->text() == ""){
        return;
    }
    msg += username + ":" + ui->lineEdit->text();
    m_Tcp->write(msg.toUtf8());
    ui->lineEdit->clear();
}

//在关闭窗口之前，断开与服务器的连接
void clientchat::closeEvent(QCloseEvent *event)
{
    m_Tcp->disconnectFromHost();
    m_Tcp->waitForDisconnected();
    event->accept();
}
