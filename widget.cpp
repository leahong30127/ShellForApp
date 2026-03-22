#include "widget.h"
#include "ui_widget.h"
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QMessageBox>
#include <QDir>
#include <QResource>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QIODevice>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 辅助函数：带 MD5 校验的安全释放文件
    static bool safeExtractResource(const QString &resPath, const QString &targetPath)
{
    // 1. 从资源读取数据
    QResource res(resPath);
    if (!res.isValid()) {
        qWarning() << "[ERROR] Resource not found:" << resPath;
        return false;
    }

    // 将资源数据加载到内存
    QByteArray newData = QByteArray(reinterpret_cast<const char*>(res.data()), res.size());
    if (newData.isEmpty()) {
        qWarning() << "[ERROR] Resource data is empty:" << resPath;
        return false;
    }

    // 2. 检查目标文件是否存在且内容一致 (避免重复写入和文件占用问题)
    bool needWrite = true;
    if (QFile::exists(targetPath)) {
        QFile existingFile(targetPath);
        if (existingFile.open(QIODevice::ReadOnly)) {
            QByteArray existingData = existingFile.readAll();
            existingFile.close();

            // 计算 MD5
            QByteArray oldMd5 = QCryptographicHash::hash(existingData, QCryptographicHash::Md5);
            QByteArray newMd5 = QCryptographicHash::hash(newData, QCryptographicHash::Md5);

            if (oldMd5 == newMd5) {
                needWrite = false;
                qDebug() << "[INFO] File already exists and matches MD5, skipping write:" << targetPath;
            } else {
                qDebug() << "[INFO] File exists but MD5 mismatch, will overwrite:" << targetPath;
                // 如果需要覆盖，先尝试删除旧文件 (防止权限问题)
                if (!QFile::remove(targetPath)) {
                    qWarning() << "[WARN] Failed to remove old file, attempting overwrite anyway...";
                }
            }
        } else {
            qWarning() << "[WARN] Cannot open existing file for check, will attempt overwrite.";
        }
    }

    // 3. 写入文件
    if (needWrite) {
        QFile outFile(targetPath);
        // 使用 Truncate 确保清空旧内容
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "[ERROR] Cannot open for writing:" << targetPath << "Error:" << outFile.errorString();
            return false;
        }

        qint64 written = outFile.write(newData);
        if (written != newData.size()) {
            qWarning() << "[ERROR] Write incomplete:" << targetPath;
            outFile.close();
            return false;
        }

        // ⭐ 关键步骤：强制刷新缓冲区到磁盘
        outFile.flush();
        outFile.close();

        qDebug() << "[OK] Extracted successfully:" << targetPath;
    }

    // 4. 设置执行权限 (特别是 Windows 下从临时目录运行)
#ifdef Q_OS_WIN
    // 赋予所有者、组、其他人 读+执行 权限
    QFile::setPermissions(targetPath,
                          QFileDevice::ExeOwner | QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ExeGroup | QFileDevice::ReadGroup |
                              QFileDevice::ExeOther | QFileDevice::ReadOther);
#endif

    // 5. 最终验证
    if (!QFile::exists(targetPath) || QFile(targetPath).size() == 0) {
        qWarning() << "[ERROR] Final verification failed for:" << targetPath;
        return false;
    }

    return true;
}


Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    controller = new SamFirmController(this);
        QString exePath;
        QString workDir;
        qDebug()<<"exe1:"<<":/resource/SamFirm.runtimeconfig.json";
        if (!prepareSamFirmEnvironment(exePath, workDir))
        {
            qWarning() << "[ERROR] Failed to prepare SamFirm environment";
            return;
        }
        controller->startSamFirm(exePath);
        qDebug()<<"exePath:"<<exePath;

        qDebug() << "SamFirm 已后台预启动";


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

    connect(controller, &SamFirmController::sigStartButtonStateChanged,
            this, [=](bool enabled){

                ui->btn_Start->setEnabled(enabled);

                if (enabled)
                    qDebug() << "SamFirm 下载完成，Start恢复";
                else
                    qDebug() << "SamFirm 正在下载...";
            });
}

Widget::~Widget()
{
    delete ui;
}

bool Widget::prepareSamFirmEnvironment(QString &exePath, QString &workDir)
{
    // ✅ 修改点1: 使用应用程序所在目录作为工作目录，避免权限问题
    workDir = QCoreApplication::applicationDirPath() + "/samfirm_tmp";
    if (!QDir().mkpath(workDir)) {
        qCritical() << "[ERROR] 无法创建工作目录:" << workDir;
        return false;
    }

    // ✅ 修改点2: 定义文件列表，并使用正确的资源路径 ":/resource/..."
    struct FileItem {
        QString resPath;
        QString fileName;
    };

    QList<FileItem> files = {
        {":/resource/SamFirm.exe", "SamFirm.exe"}, // 路径已修正
        {":/resource/SamFirm.dll", "SamFirm.dll"},
        {":/resource/SamFirm.runtimeconfig.json", "SamFirm.runtimeconfig.json"}
    };

    // ✅ 1. 获取系统真实临时目录（避免 Boxed 虚拟路径问题）
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty())
        tempDir = QDir::tempPath();

    // 👉 我们自己的子目录（规范）
    QString baseDir = tempDir + "/samfirm_tmp";
    QDir().mkpath(baseDir);


    // ✅ 修改点3: 使用与按钮代码完全一致的释放逻辑
    for (const auto &f : files) {

        // ✅ 1. 获取系统真实临时目录（避免 Boxed 虚拟路径问题）
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (tempDir.isEmpty())
            tempDir = QDir::tempPath();

        // 👉 我们自己的子目录（规范）
        QString baseDir = tempDir + "/samfirm_tmp";
        QDir().mkpath(baseDir);


        QString target = baseDir + "/" + f.fileName;
        qDebug() << "[INFO] 正在释放:" << f.resPath << "->" << target;

        // --- 开始复制按钮代码的逻辑 ---
        QFile resFile(f.resPath); // 使用修正后的路径
        if (!resFile.open(QIODevice::ReadOnly)) {
            qCritical() << "[ERROR] 打开资源失败:" << f.resPath;
            return false;
        }

        QByteArray data = resFile.readAll();
        resFile.close();

        if (data.isEmpty()) {
            qCritical() << "[ERROR] 资源数据为空:" << f.resPath;
            return false;
        }

        // 写入文件
        QFile outFile(target);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qCritical() << "[ERROR] 写入失败:" << target;
            return false;
        }

        qint64 written = outFile.write(data);
        if (written != data.size()) {
            qCritical() << "[ERROR] 写入不完整:" << target;
            outFile.close();
            return false;
        }

        outFile.flush(); // ⭐ 关键：确保写入磁盘
        outFile.close();

#ifdef Q_OS_WIN
        // 设置执行权限
        QFile::setPermissions(target,
                              QFileDevice::ExeOwner | QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                  QFileDevice::ExeGroup | QFileDevice::ReadGroup |
                                  QFileDevice::ExeOther | QFileDevice::ReadOther);
#endif
        // --- 结束复制按钮代码的逻辑 ---

        // 最终验证
        if (!QFile::exists(target) || QFile(target).size() == 0) {
            qCritical() << "[ERROR] 最终验证失败，文件可能不存在或为空:" << target;
            return false;
        }
        qDebug() << "[OK] 释放成功:" << target;
    }

    exePath = baseDir + "/SamFirm.exe";
    return true;
}

void Widget::on_btn_Start_clicked()
{
    ui->btn_Start->setEnabled(false);

    // 延迟执行（等窗口加载）
    QTimer::singleShot(200, this, [=](){


        controller->fillModelRegion(ui->lineEdit_Model->text().trimmed(), ui->lineEdit_Region->text().trimmed());
        //controller->autoStart("SM-S911N", "KOO");
    });


    QTimer::singleShot(1000, this, [=](){
        controller->fetchFirmwareList();
    });
}


void Widget::on_btn_Cancel_clicked()
{
    QTimer::singleShot(200, this, [=](){
        controller->clickCancel();
    });
}

void Widget::on_pushButton_clicked()
{
    QString exePath;
    QString workDir;
    bool prepareSamFirmEnvironment(QString &exePath, QString &workDir);
    return;

    // ✅ 1. 获取系统真实临时目录（避免 Boxed 虚拟路径问题）
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty())
        tempDir = QDir::tempPath();

    // 👉 我们自己的子目录（规范）
    QString baseDir = tempDir + "/samfirm_tmp/";
    QDir().mkpath(baseDir);

    QString outPath = baseDir + "SamFirm.exe";

    // ✅ 2. 从 Qt 资源读取
    QFile resFile(":/resource/SamFirm.exe");
    if (!resFile.open(QIODevice::ReadOnly)) {
        qDebug() << "打开资源失败";
        return;
    }

    QByteArray data = resFile.readAll();
    resFile.close();

    // ✅ 3. MD5 校验（避免重复写 & 防止文件被占用）
    bool needWrite = true;

    if (QFile::exists(outPath)) {
        QFile existing(outPath);
        if (existing.open(QIODevice::ReadOnly)) {
            QByteArray oldData = existing.readAll();
            existing.close();

            QByteArray oldMd5 = QCryptographicHash::hash(oldData, QCryptographicHash::Md5);
            QByteArray newMd5 = QCryptographicHash::hash(data, QCryptographicHash::Md5);

            if (oldMd5 == newMd5) {
                needWrite = false;
            }
        }
    }

    // ✅ 4. 写入文件（关键步骤）
    if (needWrite) {
        QFile outFile(outPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qDebug() << "写入失败:" << outPath;
            return;
        }

        qint64 written = outFile.write(data);
        if (written != data.size()) {
            qDebug() << "写入不完整";
            outFile.close();
            return;
        }

        outFile.flush();   // ⭐ 关键：确保写入磁盘
        outFile.close();
    }

#ifdef Q_OS_WIN
    // ✅ 5. 设置执行权限（保险）
    QFile::setPermissions(outPath,
                          QFileDevice::ExeOwner | QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ExeGroup | QFileDevice::ReadGroup |
                              QFileDevice::ExeOther | QFileDevice::ReadOther);
#endif

    qDebug() << "释放完成:" << outPath;

    // ✅ 6. 测试执行（可选）
    bool ok = QProcess::startDetached(outPath);
    if (!ok) {
        qDebug() << "启动失败";
    } else {
        qDebug() << "已启动 SamFirm";
    }
}

