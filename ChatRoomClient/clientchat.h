#ifndef CLIENTCHAT_H
#define CLIENTCHAT_H

#include <QDialog>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QCloseEvent>

#include "commond.h"

namespace Ui {
class clientchat;
}

class clientchat : public QDialog
{
    Q_OBJECT

public:
    explicit clientchat(QString username, QWidget *parent = nullptr);
    ~clientchat();
    void closeEvent(QCloseEvent *event);

private:
    Ui::clientchat *ui;

public:
    QTcpSocket* m_Tcp;
    QString username;
private slots:
    void on_pushButton_clicked();
};

#endif // CLIENTCHAT_H
