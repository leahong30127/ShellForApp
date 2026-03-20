#include "SamFirmController.h"
#include <QThread>
#include <QDebug>
#include <Windows.h>
#include <UIAutomation.h>   // ⭐ 必须
#include <comutil.h>
SamFirmController::SamFirmController(QObject *parent)
    : QObject(parent)
{
    m_proc = new QProcess(this);

    CoInitialize(NULL);
    CoCreateInstance(CLSID_CUIAutomation, NULL,
                     CLSCTX_INPROC_SERVER,
                     IID_IUIAutomation,
                     (void**)&automation);
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

    m_proc->start();
}

// 查找窗口
HWND SamFirmController::findWindow()
{
    for (int i = 0; i < 10; i++)
    {
        HWND hwnd = FindWindow(NULL, L"SamFirm");
        if (hwnd)
            return hwnd;

        QThread::sleep(1);
    }
    return NULL;
}

// 隐藏窗口
void SamFirmController::hideWindow(HWND hwnd)
{
    // if (hwnd)
    //     ShowWindow(hwnd, SW_HIDE);

    if (!hwnd) return;

    // 👉 移到屏幕外（推荐）
    SetWindowPos(hwnd, NULL,
                 -2000, -2000,   // 关键：移出屏幕
                 800, 600,
                 SWP_NOZORDER | SWP_SHOWWINDOW);

    qDebug() << "SamFirm 已移出屏幕";
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

      hideWindow(hwnd);   // 👈 放这里

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

    // ❗第一阶段：不要隐藏窗口
    hideWindow(hwnd);

    IUIAutomationElement *root = getRootElement(hwnd);
    if (!root)
    {
        qDebug() << "获取UIA Root失败";
        return;
    }

    qDebug() << "开始填写 Model 和 Region";

    // 第0个输入框：Model
    setEditText(root, 0, model);

    QThread::msleep(300); // 稍微等一下

    // 第1个输入框：Region
    setEditText(root, 1, region);

    qDebug() << "填写完成";
}

void SamFirmController::clickStart(IUIAutomationElement *root)
{
clickButton(root, "Start"); // 有些版本可能叫 Check Update
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
    QString result;

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_I4;
    var.lVal = UIA_EditControlTypeId;

    IUIAutomationCondition *cond = nullptr;
    automation->CreatePropertyCondition(
        UIA_ControlTypePropertyId,
        var,
        &cond
        );

    IUIAutomationElementArray *arr = nullptr;
    root->FindAll(TreeScope_SubTree, cond, &arr);

    if (!arr) return result;

    int count = 0;
    arr->get_Length(&count);

    // 👉 一般最后一个 Edit 是日志框
    for (int i = count - 1; i >= 0; i--)
    {
        IUIAutomationElement *elem = nullptr;
        arr->GetElement(i, &elem);

        if (!elem) continue;

        IValueProvider *value = nullptr;
        elem->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&value);

        if (value)
        {
            BSTR bstr;
            value->get_CurrentValue(&bstr);

            result = QString::fromWCharArray(bstr);
            SysFreeString(bstr);

            break;
        }
    }

    return result;
}
