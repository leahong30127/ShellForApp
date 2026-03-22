#ifndef SAMFIRMCONTROLLER_H
#define SAMFIRMCONTROLLER_H

#include <QObject>
#include <QProcess>
#include <windows.h>
#include <UIAutomation.h>
#include <QTimer>
#include <atomic>

class SamFirmController : public QObject
{
    Q_OBJECT
public:
    explicit SamFirmController(QObject *parent = nullptr);
~SamFirmController();
    void startSamFirm(const QString &exePath);
    void monitorStartButton(); //监测 Start按钮状态
    void autoStart(const QString &model, const QString &region);
    void fillModelRegion(const QString &model, const QString &region);
    void clickStart(IUIAutomationElement *root); //点击 start按钮。
    void clickCancel(); //点击 Concel按钮。
    QStringList getFirmwareList(IUIAutomationElement *root);
    void fetchFirmwareList();

    void selectFirmware(const QString &text);   //选中对应 ListItem
    void clickSelectButton(IUIAutomationElement *root); //点击 Select 按钮
    void clickYesDialog();                      //处理 Yes 弹窗（关键）

    void startAutoDownload(const QString &exePath,
                           const QString &model,
                           const QString &region);
    QString getLogText(IUIAutomationElement *root);//日志读取函数
    QString readLogByWin32(HWND hwnd);
signals:
    void sigFirmwareListReady(const QStringList &list);
    void sigLogUpdated(const QString &log);
    void sigStartButtonStateChanged(bool enabled); //Start 状态信号
private:
    QProcess *m_proc;
    IUIAutomation *automation;

    HWND findWindow();
    void hideWindow(HWND hwnd);
    IUIAutomationElement* getRootElement(HWND hwnd);

    void setEditText(IUIAutomationElement *root, int index, const QString &text);
    void clickButton(IUIAutomationElement *root, const QString &name);


    QTimer *m_logTimer = nullptr;
    QString m_lastLogText;
    bool m_logMonitoringStarted = false;
    QString m_lastLogTail;

    HWND m_logWnd = nullptr;   // 👉 缓存日志窗口句柄
    HWND findLogWindow(HWND mainWnd);
    HWINEVENTHOOK m_hook = NULL;
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hWinEventHook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD dwEventThread,
        DWORD dwmsEventTime
        );

    QTimer *startTimer; // “轻量轮询” Start 按钮
};

#endif
