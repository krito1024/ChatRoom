#include "login.h"
#include "ui_login.h"

login::login(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::login)
{
    ui->setupUi(this);
    this->setWindowTitle("瞎勾吧聊");

    //初始化tcp
    m_Tcp = new QTcpSocket();
    m_Tcp->setProxy(QNetworkProxy::NoProxy);//禁用代理
    //设置登录界面图标
    pix.load(":/images/src/images.png");
    QPixmap temp_pic = pix.scaled(pix.size() * 0.7, Qt::KeepAspectRatio);
    ui->picture->setPixmap(temp_pic);
    //设置密码框
    ui->password->setEchoMode(QLineEdit::Password);
    //禁用注册和登录按钮
    ui->Register->setEnabled(false);
    ui->Login->setEnabled(false);

    //连接成功
    connect(m_Tcp, &QTcpSocket::connected, this, [=](){
        QMessageBox::information(this, "连接状态", "服务器连接成功,请注册或登录!");
        ui->conn->setEnabled(false);
        ui->Register->setEnabled(true);
        ui->Login->setEnabled(true);
    });
    //连接失败
    connect(m_Tcp, &QTcpSocket::errorOccurred, this, [=](QAbstractSocket::SocketError socketError){
        QMessageBox::information(this, "连接状态", "服务器连接失败,请重试!");
        qDebug() << "Connection error occurred: " << socketError;
        qDebug() << "Error description: " << m_Tcp->errorString();
    });

    //设置连接请求信号
    connect(m_Tcp, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_Tcp->readAll();
        QString tx_msg = data.data();
        qDebug() << "msg:" << tx_msg;
        if(tx_msg == "用户已注册，可直接登陆"){
            QMessageBox::warning(this, "注册", "用户已注册，可直接登陆!");
        }
        else if(tx_msg == "注册成功"){
            QMessageBox::information(this, "注册", "注册成功!");
        }
        else if(tx_msg == "请先进行注册"){
            QMessageBox::warning(this, "登录", "请先进行注册!");
        }
        else if(tx_msg == "登陆成功"){
            QMessageBox::information(this, "登录", "登录成功!");
            //创建main窗口
            main = new mainwindow(ui->name->text());
            main->setname();
            main->show();

            this->hide();
        }
        else if(tx_msg == "密码错误"){
            QMessageBox::warning(this, "登录", "密码错误!");
        }
        else if(tx_msg.contains("*adDR.", Qt::CaseSensitive)){//收到了别人想要加自己为好友
            std::string rec = tx_msg.toStdString();
            auto it = rec.find("+");
            QString fromname = QString::fromStdString(rec.substr(6,it-6));
            qDebug() << fromname;
            main->fromname = fromname;
            main->set_added();
        }
        else if(tx_msg.contains("*ACCEPT.", Qt::CaseSensitive)){
            QMessageBox::information(this, "添加好友", "您已成功添加好友!");
        }
        else if(tx_msg == "对方拒绝了您的申请"){
            QMessageBox::information(this, "添加好友", "对方拒绝了您的申请!");
        }
    });

}

login::~login()
{
    delete ui;
    m_Tcp->disconnectFromHost();
}


void login::on_conn_clicked()
{
    if(ui->name->text() == "" || ui->password->text() == ""){
        QMessageBox::warning(this, "警告", "账号或密码为空，请输入后重试");
        return;
    }
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    login::m_Tcp->connectToHost(QHostAddress(ip), port);
}


void login::on_Register_clicked()
{
    QString msg(REGISTER);
    QString name = ui->name->text();
    QString password = ui->password->text();
    msg += name + "\t" + password;
    m_Tcp->write(msg.toUtf8());
}


void login::on_Login_clicked()
{
    QString msg(LOGIN);
    QString name = ui->name->text();
    QString password = ui->password->text();
    msg += name + "\t" + password;
    m_Tcp->write(msg.toUtf8());
}

