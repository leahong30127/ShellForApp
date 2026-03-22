/********************************************************************************
** Form generated from reading UI file 'widget.ui'
**
** Created by: Qt User Interface Compiler version 6.9.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Widget
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLineEdit *lineEdit_Model;
    QLineEdit *lineEdit_Region;
    QPushButton *pushButton;
    QPushButton *btn_Start;
    QPushButton *btn_Cancel;
    QTextEdit *textEdit;
    QListWidget *listWidget;

    void setupUi(QWidget *Widget)
    {
        if (Widget->objectName().isEmpty())
            Widget->setObjectName("Widget");
        Widget->resize(581, 497);
        verticalLayout = new QVBoxLayout(Widget);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        lineEdit_Model = new QLineEdit(Widget);
        lineEdit_Model->setObjectName("lineEdit_Model");

        horizontalLayout->addWidget(lineEdit_Model);

        lineEdit_Region = new QLineEdit(Widget);
        lineEdit_Region->setObjectName("lineEdit_Region");

        horizontalLayout->addWidget(lineEdit_Region);

        pushButton = new QPushButton(Widget);
        pushButton->setObjectName("pushButton");

        horizontalLayout->addWidget(pushButton);

        btn_Start = new QPushButton(Widget);
        btn_Start->setObjectName("btn_Start");
        btn_Start->setMinimumSize(QSize(80, 30));

        horizontalLayout->addWidget(btn_Start);

        btn_Cancel = new QPushButton(Widget);
        btn_Cancel->setObjectName("btn_Cancel");
        btn_Cancel->setMinimumSize(QSize(0, 30));

        horizontalLayout->addWidget(btn_Cancel);


        verticalLayout->addLayout(horizontalLayout);

        textEdit = new QTextEdit(Widget);
        textEdit->setObjectName("textEdit");

        verticalLayout->addWidget(textEdit);

        listWidget = new QListWidget(Widget);
        listWidget->setObjectName("listWidget");

        verticalLayout->addWidget(listWidget);


        retranslateUi(Widget);

        QMetaObject::connectSlotsByName(Widget);
    } // setupUi

    void retranslateUi(QWidget *Widget)
    {
        Widget->setWindowTitle(QCoreApplication::translate("Widget", "Samsung Firmware Downloader 2.0.0", nullptr));
        lineEdit_Model->setText(QCoreApplication::translate("Widget", "SM-S911N", nullptr));
        lineEdit_Region->setText(QCoreApplication::translate("Widget", "KOO", nullptr));
        lineEdit_Region->setPlaceholderText(QCoreApplication::translate("Widget", "Region", nullptr));
        pushButton->setText(QCoreApplication::translate("Widget", "PushButton", nullptr));
        btn_Start->setText(QCoreApplication::translate("Widget", "Start", nullptr));
        btn_Cancel->setText(QCoreApplication::translate("Widget", "Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Widget: public Ui_Widget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WIDGET_H
