#include "widget.h"
#include "ui_widget.h"
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QMessageBox>

#ifdef Q_OS_WIN
#include <windows.h>   // 👈 必须加这个
#endif

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    controller = new SamFirmController(this);

    connect(controller, &SamFirmController::sigFirmwareListReady,
            this, [=](const QStringList &list){

                ui->listWidget->clear();
                ui->listWidget->addItems(list);

            });
    connect(ui->listWidget, &QListWidget::itemClicked,
            this, [=](QListWidgetItem *item){

                QString text = item->text();
                qDebug() << "Qt选中:" << text;

                controller->selectFirmware(text);
            });

    connect(controller, &SamFirmController::sigLogUpdated,
            this, [=](const QString &log){

                ui->textEdit->setPlainText(log);

                // 👉 自动滚动到底部
                QTextCursor cursor = ui->textEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                ui->textEdit->setTextCursor(cursor);

            });
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_pushButton_clicked()
{
    QString exePath = "SamFirm.exe";

    if (!QFile::exists(exePath))
    {
        QMessageBox::warning(this, "错误", "SamFirm.exe 不存在！");
        return;
    }


    controller->startSamFirm(exePath);

    // 延迟执行（等窗口加载）
    QTimer::singleShot(3000, this, [=](){


        controller->fillModelRegion("SM-S911N", "KOO");
        //controller->autoStart("SM-S911N", "KOO");
    });


    QTimer::singleShot(5000, this, [=](){
        controller->fetchFirmwareList();
    });
}

