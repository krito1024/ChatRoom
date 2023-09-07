#ifndef CHOSEADD_H
#define CHOSEADD_H

#include <QDialog>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QCloseEvent>

#include "commond.h"

namespace Ui {
class choseadd;
}

class choseadd : public QDialog
{
    Q_OBJECT

public:
    explicit choseadd(QString fromname, QString username, QWidget *parent = nullptr);
    ~choseadd();

    void closeEvent(QCloseEvent* event);

private slots:
    void on_accept_clicked();

    void on_pushButton_2_clicked();

private:
    Ui::choseadd *ui;
public:
    QTcpSocket* m_Tcp;
    QString fromname;
    QString toname;
};

#endif // CHOSEADD_H
