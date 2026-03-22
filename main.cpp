#include "widget.h"

#include <QApplication>
#include <QFile>
#include <QDebug>

bool checkOne()
{
    //  创建互斥量
    HANDLE m_hMutex  =  CreateMutex(NULL, FALSE,  L"Samsung Firmware Downloader" );
    //  检查错误代码
    if  (GetLastError()  ==  ERROR_ALREADY_EXISTS)  {
        //  如果已有互斥量存在则释放句柄并复位互斥量
        CloseHandle(m_hMutex);
        m_hMutex  =  NULL;
        //  程序退出
        return  false;
    }
    else
        return true;
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //防止程序开启两次
    if(!checkOne()) {
        return 0;
    }

    QCoreApplication::addLibraryPath("E:/Samsung/1常用软件/1_CY_Tools/Tools/1超级工具_运营商/SalesCodeSerial2");


    Widget w;
    w.show();
    return a.exec();
}
