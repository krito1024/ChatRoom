#include "mainwindow.h"
#include "ui_mainwindow.h"

mainwindow::mainwindow(QString username, QWidget *parent) :
    username(username),
    QMainWindow(parent),
    ui(new Ui::mainwindow)
{
    ui->setupUi(this);
    initPicList();
    // 设置样式表
    QString styleSheet = "QListWidget::item { height: 40px; color: black; background-color: white; }"
                         "QListWidget::item { border-bottom: 1px solid gray; padding-bottom: 4px; }";
    ui->friends->setStyleSheet(styleSheet);
    //朋友数量
    friendsNum = 0;
    //设置tab名称
    ui->tabWidget->setTabText(0, "系统功能");
    ui->tabWidget->setTabText(1, "朋友列表");
    //设置头像下拉选项的文字
    ui->changepic->setCurrentText("选择头像");
    //把被加的按钮隐藏
    ui->added->hide();

    //创建m_Tcp
    m_Tcp = new QTcpSocket();
    m_Tcp->setProxy(QNetworkProxy::NoProxy);
    QString ip = SERVER_IP;
    unsigned short port = SERVER_PORT;
    m_Tcp->connectToHost(QHostAddress(ip), port);
    //创建online对象
    online = new onlinestate();
    //创建菜单
    menu = new QMenu(this);
    //连接右键事件
    connect(ui->friends, &QListWidget::customContextMenuRequested, this, &mainwindow::showMenu);

    //friendOnlineState = online->friendOnlineState;
    connect(m_Tcp, &QTcpSocket::connected, this, [=](){
        //获取当前用户头像
        getPic();
    });
    //收到数据时
    connect(m_Tcp, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_Tcp->readAll();
        QString msg = data.data();
        qDebug() << msg;
        if(msg.contains(":", Qt::CaseSensitive)){//当收到好友名字列表
            int count = msg.mid(msg.size() - 1, 1).toInt();
            for(int i = 0; i < count; i++){
                QString name = msg.section(":", i, i);
                friendList.push_back(name);
            }
            emit main_inited();//发射信号，告诉好友列表和在线情况列表都更新好了
        }
        else if(msg.contains("*PIC.", Qt::CaseSensitive)){
            int picNum = msg.mid(5, 1).toInt();
            //显示头像
            p.load(PicList[picNum]);
            QPixmap temp_p = p.scaled(ui->pic->size() * 3, Qt::KeepAspectRatio);
            ui->pic->setPixmap(temp_p);
            ui->changepic->setCurrentIndex(picNum);
        }
    });
    //online类发射信号，更新完毕
    connect(online, &onlinestate::StateUpdateDone, this, &mainwindow::initfri);
    //开始初始化
    connect(this, &mainwindow::main_inited, this, &mainwindow::initfriendlist);
}

void mainwindow::showMenu(const QPoint& pos) {//显示右键菜单
    QListWidgetItem *currentItem = ui->friends->itemAt(pos);
    if(currentItem->text().contains(username)){//如果是自己的话就不显示菜单
        return;
    }
    menu->clear();
    menu->addAction(ui->actChat);
    menu->addAction(ui->actSendFile);
    //设置action的属性值，保存当前项的文本
    ui->actChat->setProperty("itemText", currentItem->text());
    ui->actSendFile->setProperty("itemText", currentItem->text());
    menu->exec(ui->friends->mapToGlobal(pos));
}

void mainwindow::initfriendlist()//初始化朋友
{
    qDebug() << "进入初始化朋友了";
    while(!friendList.empty()){
        qDebug() << "进入初始化循环了";
        QListWidgetItem *new_item = new QListWidgetItem();

        //new_item->setText(friendList.front());
        for(int i = 0; i < online->friendOnlineState.size(); i++){//查找朋友名字等于全部用户列表名字的一项
            if(online->friendOnlineState[i].username == friendList.front()){//找到了
                QIcon ico(PicList[online->friendOnlineState[i].pic]);
                new_item->setIcon(ico);//设置图标
                if(online->friendOnlineState[i].online == 0){
                    //不在线
                    new_item->setText(friendList.front() + "(不在线)");
                }
                else if(online->friendOnlineState[i].online == 1){
                    new_item->setText(friendList.front() + "(在线)");
                }
            }
        }
        friendList.pop_front();
        ui->friends->addItem(new_item);
    }
}

void mainwindow::initPicList()//初始化头像向量
{
    PicList.push_back(":/images/src/liu.png");
    PicList.push_back(":/images/src/m.png");
    PicList.push_back(":/images/src/m2.png");
    PicList.push_back(":/images/src/qq.png");
    PicList.push_back(":/images/src/w.png");
    PicList.push_back(":/images/src/w2.png");
    PicList.push_back(":/images/src/z.png");
}

void mainwindow::getPic()//向服务端发信息获得头像号
{
    QString msg(INITPIC);
    msg += username;
    m_Tcp->write(msg.toUtf8());
}

void mainwindow::initfri()
{
    QString msg(FRIENDSLIST);
    msg += username;
    m_Tcp->write(msg.toUtf8());
}

void mainwindow::on_tabWidget_currentChanged(int index)//页面变化
{
    if(index == 1){
        //获取朋友姓名列表
        QString msg(FRIENDSLIST);
        msg += username;
        //m_Tcp->write(msg.toUtf8());
        //获取所有用户的在线信息
        online->getfriendOnlineState();
        ui->friends->clear();
    }
    else if(index == 0){
        ui->friends->clear();
    }

}

void mainwindow::on_changepic_currentIndexChanged(int index)//当更换头像时
{
    //显示头像
    p.load(PicList[index]);
    ui->pic->setPixmap(p);
    QString msg(PICCHANGE);
    msg += username + ":" + QString::number(index);
   // qDebug() << msg;
    m_Tcp->write(msg.toUtf8());
}

mainwindow::~mainwindow()
{
    delete ui;
}

void mainwindow::set_added()
{
    ui->added->show();
    QString msg;
    msg += fromname + "想添加您为好友";
    ui->added->setText(msg);
}

void mainwindow::closeEvent(QCloseEvent *event)
{
    m_Tcp->disconnectFromHost();
    m_Tcp->waitForDisconnected();
    event->accept();
}

void mainwindow::setname()
{
    //设置名字
    ui->name->setText(username);
}

void mainwindow::on_addfriend_clicked()
{
    addf = new addfriend(username, this);
    addf->username = username;
    addf->show();
}

void mainwindow::on_clientchat_clicked()
{
    //创建聊天室窗口
    chat = new clientchat(username, this);
    chat->show();
}

void mainwindow::on_added_clicked()
{
    //创建是否同意窗口
    chadd = new choseadd(fromname, username, this);
    chadd->show();
    ui->added->hide();
}



void mainwindow::on_actChat_triggered()
{
    QString itemName = ui->actChat->property("itemText").toString();
    int it = itemName.indexOf("(");
    QString name = itemName.mid(0, it);
    //qDebug() << name;

}


void mainwindow::on_actSendFile_triggered()
{
    QString itemName = ui->actChat->property("itemText").toString();
    int it = itemName.indexOf("(");
    QString name = itemName.mid(0, it);
    //qDebug() << name;
}

