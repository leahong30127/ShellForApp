#include "SamFirmController.h"
#include <QThread>
#include <QDebug>
#include <Windows.h>
#include <UIAutomation.h>   // ⭐ 必须
#include <QtConcurrent>
#include <comutil.h>

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    std::vector<HWND>* list = (std::vector<HWND>*)lParam;
    list->push_back(hwnd);
    return TRUE;
}

// 👉 找日志窗口
HWND SamFirmController::findLogWindow(HWND mainWnd)
{
    std::vector<HWND> children;
    EnumChildWindows(mainWnd, EnumChildProc, (LPARAM)&children);

    HWND best = NULL;
    int maxArea = 0;

    for (HWND h : children)
    {
        wchar_t className[256] = {0};
        GetClassName(h, className, 256);

        QString cls = QString::fromWCharArray(className);

        // 👉 只要 WindowsForms 的 Edit
        if (!cls.contains("WindowsForms10.Edit", Qt::CaseInsensitive))
            continue;

        RECT rc;
        GetWindowRect(h, &rc);

        int w = rc.right - rc.left;
        int hgt = rc.bottom - rc.top;
        int area = w * hgt;

        // 👉 排除小输入框（关键）
        if (area < 20000)   // Model/Region 很小
            continue;

        if (area > maxArea)
        {
            maxArea = area;
            best = h;
        }
    }

    if (best)
    {
        qDebug() << "✔ 选中日志窗口:" << best;
    }
    else
    {
        qDebug() << "❌ 未找到日志窗口";
    }

    return best;
}

void SamFirmController::WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (!hwnd) return;

    wchar_t title[256] = {0};
    GetWindowText(hwnd, title, 256);

    // 👉 只处理 SamFirm
    if (wcscmp(title, L"SamFirm") == 0)
    {
        qDebug() << "捕获到 SamFirm 窗口（无闪）";

        // 👉 隐藏窗口
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        exStyle &= ~WS_EX_APPWINDOW;
        exStyle |= WS_EX_TOOLWINDOW;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

        SetWindowPos(hwnd, NULL,
                     -2000, -2000,
                     800, 600,
                     SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}
// 👉 读取文本
QString SamFirmController::readLogByWin32(HWND hwnd)
{
    if (!hwnd) return "";

    int len = SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
    if (len <= 0) return "";

    std::wstring buffer(len + 1, L'\0');

    SendMessage(hwnd, WM_GETTEXT,
                (WPARAM)(len + 1),
                (LPARAM)buffer.data());

    return QString::fromWCharArray(buffer.c_str());
}

SamFirmController::SamFirmController(QObject *parent)
    : QObject(parent),
    m_proc(new QProcess(this)),
    automation(nullptr),
    m_logTimer(new QTimer(this))
{
    CoInitialize(NULL);
    CoCreateInstance(CLSID_CUIAutomation, NULL,
                     CLSCTX_INPROC_SERVER,
                     IID_IUIAutomation,
                     (void**)&automation);

    m_logTimer->setInterval(500);

    connect(m_logTimer, &QTimer::timeout, this, [this]() {

        HWND hwnd = findWindow();
        if (!hwnd)
            return;

        // 👉 第一次找日志窗口
        if (!m_logWnd)
        {
            m_logWnd = findLogWindow(hwnd);

            if (!m_logWnd)
            {
                qDebug() << "未找到日志窗口";
                return;
            }

            qDebug() << "日志窗口已锁定:" << m_logWnd;
        }

        QString text = readLogByWin32(m_logWnd);

        if (text.isEmpty())
            return;

        // 👉 只要有变化就更新
        if (text != m_lastLogText)
        {
            m_lastLogText = text;
            emit sigLogUpdated(text);
        }
    });

    QTimer *timer = new QTimer(this);

    connect(timer, &QTimer::timeout, this, [=](){
        monitorStartButton();
    });

    timer->start(500); // 每500ms检查一次（合理）
}

SamFirmController::~SamFirmController()
{
    if (m_logTimer && m_logTimer->isActive())
        m_logTimer->stop();

    if (automation)
    {
        automation->Release();
        automation = nullptr;
    }

    CoUninitialize();
}

// 启动 SamFirm
void SamFirmController::startSamFirm(const QString &exePath)
{
#ifdef Q_OS_WIN
    m_proc->setProgram(exePath);

    m_proc->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args){
            args->flags |= CREATE_NO_WINDOW;
        }
        );
#endif

    // 🔥 先安装 Hook（关键：必须在 start 之前）
    m_hook = SetWinEventHook(
        EVENT_OBJECT_SHOW,   // 监听窗口显示
        EVENT_OBJECT_SHOW,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
        );

    m_proc->start();
}

void SamFirmController::monitorStartButton()
{
    qDebug() << "Start 轮询开始了";

#ifdef Q_OS_WIN
    static bool lastState = true;

    HWND hwnd = findWindow();
    if (!hwnd) return;

    IUIAutomationElement *root = getRootElement(hwnd);
    if (!root) return;

    IUIAutomationCondition *cond = nullptr;
    automation->CreatePropertyCondition(
        UIA_NamePropertyId,
        _variant_t((LPCWSTR)QString("Start").utf16()),
        &cond
        );

    IUIAutomationElement *btn = nullptr;
    root->FindFirst(TreeScope_SubTree, cond, &btn);

    if (!btn) return;

    BOOL enabled = FALSE;
    btn->get_CurrentIsEnabled(&enabled);

    if (enabled != lastState)
    {
        lastState = enabled;
        qDebug() << "Start状态变化:" << enabled;
        emit sigStartButtonStateChanged(enabled);
    }

    if (btn) btn->Release();
    if (cond) cond->Release();
    if (root) root->Release();
#endif
}

// 查找窗口
HWND SamFirmController::findWindow()
{
    // for (int i = 0; i < 10; i++)
    // {
    //     HWND hwnd = FindWindow(NULL, L"SamFirm");
    //     if (hwnd)
    //         return hwnd;

    //     QThread::sleep(1);
    // }
    // return NULL;
    return FindWindow(NULL, L"SamFirm");
}

// 隐藏窗口
void SamFirmController::hideWindow(HWND hwnd)
{
    // if (hwnd)
    //     ShowWindow(hwnd, SW_HIDE);

    if (!hwnd) return;

    // // 👉 移到屏幕外（推荐）
    // SetWindowPos(hwnd, NULL,
    //              -2000, -2000,   // 关键：移出屏幕
    //              800, 600,
    //              SWP_NOZORDER | SWP_SHOWWINDOW);

    // qDebug() << "SamFirm 已移出屏幕";

    // 1️⃣ 获取当前扩展样式
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    // 2️⃣ 移除任务栏属性
    exStyle &= ~WS_EX_APPWINDOW;

    // 3️⃣ 添加工具窗口属性（不会出现在任务栏）
    exStyle |= WS_EX_TOOLWINDOW;

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    // 4️⃣ 应用样式变化（关键）
    SetWindowPos(hwnd, NULL,
                 -2000, -2000,
                 800, 600,
                 SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    qDebug() << "SamFirm 已隐藏 + 从任务栏移除";
}

// 获取 UIA 根节点
IUIAutomationElement* SamFirmController::getRootElement(HWND hwnd)
{
    IUIAutomationElement *root = nullptr;
    automation->ElementFromHandle(hwnd, &root);
    return root;
}

// 设置输入框
void SamFirmController::setEditText(IUIAutomationElement *root, int index, const QString &text)
{
    IUIAutomationCondition *cond = nullptr;
    automation->CreatePropertyCondition(
        UIA_ControlTypePropertyId,
        _variant_t(UIA_EditControlTypeId),
        &cond
        );

    IUIAutomationElementArray *arr = nullptr;
    root->FindAll(TreeScope_SubTree, cond, &arr);

    if (!arr) return;

    IUIAutomationElement *elem = nullptr;
    arr->GetElement(index, &elem);

    if (!elem) return;

    IValueProvider *value = nullptr;
    elem->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&value);

    if (value)
        value->SetValue((LPCWSTR)text.utf16());
}

// 点击按钮
void SamFirmController::clickButton(IUIAutomationElement *root, const QString &name)
{
    IUIAutomationCondition *cond = nullptr;

    automation->CreatePropertyCondition(
        UIA_NamePropertyId,
        _variant_t((LPCWSTR)name.utf16()),
        &cond
        );

    IUIAutomationElement *btn = nullptr;
    root->FindFirst(TreeScope_SubTree, cond, &btn);

    if (!btn) return;

    IInvokeProvider *invoke = nullptr;
    btn->GetCurrentPattern(UIA_InvokePatternId, (IUnknown**)&invoke);

    if (invoke)
        invoke->Invoke();
}

// 自动执行
void SamFirmController::autoStart(const QString &model, const QString &region)
{
    HWND hwnd = findWindow();
    if (!hwnd)
    {
        qDebug() << "未找到 SamFirm 窗口";
        return;
    }

    qDebug()<<"隐藏原始窗口。";
    hideWindow(hwnd);

    IUIAutomationElement *root = getRootElement(hwnd);

    if (!root)
        return;

   //   hideWindow(hwnd);   // 👈 放这里



    // 填输入框（第0=型号，第1=地区）
    setEditText(root, 0, model);
    setEditText(root, 1, region);

    // 点击 Start
    clickButton(root, "Start");
}

void SamFirmController::fillModelRegion(const QString &model, const QString &region)
{
    HWND hwnd = findWindow();
    if (!hwnd)
    {
        qDebug() << "未找到 SamFirm 窗口";
        return;
    }

    IUIAutomationElement *root = getRootElement(hwnd);
    if (!root)
    {
        qDebug() << "获取UIA Root失败";
        return;
    }

    qDebug() << "开始填写 Model 和 Region";

    // 启动日志轮询
    if (m_logTimer && !m_logTimer->isActive())
        m_logTimer->start();

    // 第0个输入框：Model
    setEditText(root, 0, model);
    QThread::msleep(300);

    // 第1个输入框：Region
    setEditText(root, 1, region);

    qDebug() << "填写完成";

    root->Release();
}

void SamFirmController::clickStart(IUIAutomationElement *root)
{
    clickButton(root, "Start"); // 有些版本可能叫 Check Update
}

void SamFirmController::clickCancel()
{
    HWND hwnd = findWindow();
    if (!hwnd) return;

    IUIAutomationElement *root = getRootElement(hwnd);
    if (!root) return;
   clickButton(root, "Cancel");
}

QStringList SamFirmController::getFirmwareList(IUIAutomationElement *root)
{
    QStringList list;

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_I4;
    var.lVal = UIA_ListItemControlTypeId;

    IUIAutomationCondition *cond = nullptr;
    automation->CreatePropertyCondition(
        UIA_ControlTypePropertyId,
        var,
        &cond
        );

    IUIAutomationElementArray *items = nullptr;

    // 🔥 全局查找（关键）
    root->FindAll(TreeScope_SubTree, cond, &items);

    if (!items)
    {
        qDebug() << "没有找到 ListItem";
        return list;
    }

    int count = 0;
    items->get_Length(&count);

    qDebug() << "ListItem 数量:" << count;

    for (int i = 0; i < count; i++)
    {
        IUIAutomationElement *item = nullptr;
        items->GetElement(i, &item);

        if (!item) continue;

        BSTR name;
        item->get_CurrentName(&name);

        QString text = QString::fromWCharArray(name);

        // 👉 过滤空的和无效的
        if (!text.trimmed().isEmpty() && text.contains("Version"))
        {
            list << text;
            qDebug() << text;
        }

        SysFreeString(name);
    }

    return list;
}
void SamFirmController::fetchFirmwareList()
{
    HWND hwnd = findWindow();
    if (!hwnd) return;

    IUIAutomationElement *root = getRootElement(hwnd);
    if (!root) return;

    // 1️⃣ 点击 Start
    clickStart(root);

    // 2️⃣ 等服务器返回
    qDebug() << "等待版本列表...";
    QThread::sleep(5);

    // 3️⃣ 读取列表
    QStringList list = getFirmwareList(root);

    qDebug() << "获取到版本数量:" << list.size();

    for (auto &s : list)
        qDebug() << s;

    emit sigFirmwareListReady(list);
}

void SamFirmController::selectFirmware(const QString &text)
{
    HWND hwnd = findWindow();
    if (!hwnd) return;

    IUIAutomationElement *root = getRootElement(hwnd);
    if (!root) return;

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_I4;
    var.lVal = UIA_ListItemControlTypeId;

    IUIAutomationCondition *cond = nullptr;
    automation->CreatePropertyCondition(
        UIA_ControlTypePropertyId,
        var,
        &cond
        );

    IUIAutomationElementArray *items = nullptr;
    root->FindAll(TreeScope_SubTree, cond, &items);

    if (!items) return;

    int count = 0;
    items->get_Length(&count);

    for (int i = 0; i < count; i++)
    {
        IUIAutomationElement *itemElem = nullptr;
        items->GetElement(i, &itemElem);

        if (!itemElem) continue;

        BSTR name;
        itemElem->get_CurrentName(&name);

        QString itemText = QString::fromWCharArray(name);
        SysFreeString(name);

        if (itemText == text)  // 🔥 精确匹配
        {
            qDebug() << "找到匹配项:" << itemText;

            ISelectionItemProvider *select = nullptr;
            itemElem->GetCurrentPattern(UIA_SelectionItemPatternId, (IUnknown**)&select);

            if (select)
                select->Select();

            // 👉 选中后继续流程
            QThread::msleep(300);
            clickSelectButton(root);

            QThread::sleep(1);
            clickYesDialog();

            return;
        }
    }

    qDebug() << "没有找到匹配项";
}


void SamFirmController::clickSelectButton(IUIAutomationElement *root)
{
    clickButton(root, "Select");
}

void SamFirmController::clickYesDialog()
{
    for (int i = 0; i < 10; i++)
    {
        HWND dlg = FindWindow(NULL, L"Confirm");

        if (dlg)
        {
            HWND yesBtn = FindWindowEx(dlg, NULL, NULL, L"Yes");

            if (yesBtn)
            {
                SendMessage(yesBtn, BM_CLICK, 0, 0);
                qDebug() << "已点击 Yes";
                return;
            }
        }

        Sleep(500);
    }

    qDebug() << "未找到 Yes 弹窗";
}

void SamFirmController::startAutoDownload(const QString &exePath, const QString &model, const QString &region)
{

}

QString SamFirmController::getLogText(IUIAutomationElement *root)
{
    return "";
    QString bestText;
    QString bestLogLikeText;

    if (!root || !automation)
        return bestText;

    IUIAutomationCondition *cond = nullptr;
    automation->CreateTrueCondition(&cond);

    IUIAutomationElementArray *arr = nullptr;
    root->FindAll(TreeScope_SubTree, cond, &arr);

    if (cond) cond->Release();
    if (!arr) return bestText;

    int count = 0;
    arr->get_Length(&count);

    for (int i = 0; i < count; ++i)
    {
        IUIAutomationElement *elem = nullptr;
        if (FAILED(arr->GetElement(i, &elem)) || !elem)
            continue;

        CONTROLTYPEID typeId = 0;
        elem->get_CurrentControlType(&typeId);

        if (typeId == UIA_EditControlTypeId ||
            typeId == UIA_DocumentControlTypeId ||
            typeId == UIA_TextControlTypeId)
        {
            QString text;

            // 1) ValuePattern
            IValueProvider *value = nullptr;
            if (SUCCEEDED(elem->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&value)) && value)
            {
                BSTR bstr = nullptr;
                if (SUCCEEDED(value->get_Value(&bstr)) && bstr)
                {
                    text = QString::fromWCharArray(bstr);
                    SysFreeString(bstr);
                }
                value->Release();
            }

            // 2) TextPattern
            if (text.trimmed().isEmpty())
            {
                IUIAutomationTextPattern *textPattern = nullptr;
                if (SUCCEEDED(elem->GetCurrentPatternAs(UIA_TextPatternId,
                                                        IID_IUIAutomationTextPattern,
                                                        (void**)&textPattern)) && textPattern)
                {
                    IUIAutomationTextRange *range = nullptr;
                    if (SUCCEEDED(textPattern->get_DocumentRange(&range)) && range)
                    {
                        BSTR bstr = nullptr;
                        // 先试完整读取
                        if (SUCCEEDED(range->GetText(-1, &bstr)) && bstr)
                        {
                            text = QString::fromWCharArray(bstr);
                            SysFreeString(bstr);
                        }
                        range->Release();
                    }
                    textPattern->Release();
                }
            }

            // 3) LegacyIAccessible
            if (text.trimmed().isEmpty())
            {
                IUIAutomationLegacyIAccessiblePattern *legacy = nullptr;
                if (SUCCEEDED(elem->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId,
                                                        IID_IUIAutomationLegacyIAccessiblePattern,
                                                        (void**)&legacy)) && legacy)
                {
                    BSTR bstr = nullptr;
                    if (SUCCEEDED(legacy->get_CurrentValue(&bstr)) && bstr)
                    {
                        text = QString::fromWCharArray(bstr);
                        SysFreeString(bstr);
                    }
                    legacy->Release();
                }
            }

            QString lower = text.toLower();

            bool looksLikeLog =
                lower.contains("checking") ||
                lower.contains("version") ||
                lower.contains("download") ||
                lower.contains("decrypt") ||
                lower.contains("error") ||
                lower.contains("crc") ||
                lower.contains("model") ||
                lower.contains("region") ||
                lower.contains("binary") ||
                lower.contains("size");

            if (looksLikeLog && text.length() > bestLogLikeText.length())
                bestLogLikeText = text;

            if (text.length() > bestText.length())
                bestText = text;
        }

        elem->Release();
    }

    arr->Release();

    if (!bestLogLikeText.isEmpty())
        return bestLogLikeText;

    return bestText;
}
