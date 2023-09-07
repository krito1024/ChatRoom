#ifndef ADDFRIEND_H
#define ADDFRIEND_H

#include <QDialog>
#include <QTcpSocket>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QCloseEvent>

#include "commond.h"

namespace Ui {
class addfriend;
}

class addfriend : public QDialog
{
    Q_OBJECT

public:
    explicit addfriend(QString username, QWidget *parent = nullptr);
    ~addfriend();

    void closeEvent(QCloseEvent *event);

public:
    QString username;
    QTcpSocket* m_Tcp;

private slots:
    void on_send_clicked();

private:
    Ui::addfriend *ui;
};

#endif // ADDFRIEND_H
