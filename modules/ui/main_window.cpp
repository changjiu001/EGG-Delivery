#include "main_window.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      controller_(new FileTransferController(QApplication::applicationDirPath() + QStringLiteral("/downloads"), this)) {
    buildUi();

    connect(controller_, &FileTransferController::transferAdded, this, &MainWindow::upsertTransfer);
    connect(controller_, &FileTransferController::transferUpdated, this, &MainWindow::upsertTransfer);
    connect(controller_, &FileTransferController::transferCompleted, this, &MainWindow::markTransferCompleted);
    connect(controller_, &FileTransferController::transferFailed, this, &MainWindow::markTransferFailed);
    connect(controller_, &FileTransferController::logMessage, this, &MainWindow::appendLog);

    connect(sendButton_, &QPushButton::clicked, this, &MainWindow::chooseFilesToSend);
    connect(chooseDirButton_, &QPushButton::clicked, this, &MainWindow::chooseDownloadDirectory);
    connect(openDirButton_, &QPushButton::clicked, this, &MainWindow::openDownloadDirectory);
    connect(loopbackCheck_, &QCheckBox::toggled, controller_, &FileTransferController::setLoopbackEnabled);

    downloadDirEdit_->setText(controller_->downloadDirectory());
    appendLog(QStringLiteral("GUI 已启动。当前使用本机回环演示，等待网络模块接入。"));
}

void MainWindow::chooseDownloadDirectory() {
    const QString directory = QFileDialog::getExistingDirectory(this,
                                                               QStringLiteral("选择下载目录"),
                                                               downloadDirEdit_->text());
    if (directory.isEmpty()) {
        return;
    }

    controller_->setDownloadDirectory(directory);
    downloadDirEdit_->setText(controller_->downloadDirectory());
}

void MainWindow::chooseFilesToSend() {
    const QStringList files = QFileDialog::getOpenFileNames(this,
                                                            QStringLiteral("选择要发送的文件"),
                                                            QString());
    for (const QString& file : files) {
        controller_->sendFile(file, QStringLiteral("局域网用户"));
    }
}

void MainWindow::openDownloadDirectory() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(controller_->downloadDirectory()));
}

void MainWindow::clearLog() {
    logEdit_->clear();
}

void MainWindow::appendLog(const QString& message) {
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    logEdit_->append(QStringLiteral("[%1] %2").arg(now, message));
}

void MainWindow::upsertTransfer(const TransferRecord& record) {
    const int row = rowForTransfer(record.transferId);
    setTextCell(row, FileNameColumn, record.fileName);
    setTextCell(row, DirectionColumn, record.direction);
    setTextCell(row, PeerColumn, record.peerName);
    setTextCell(row, HashColumn, record.fileHash.left(12));
    setProgressCell(row, record.progress);

    QString statusText = record.status;
    if (record.bytesTotal > 0) {
        statusText += QStringLiteral(" · %1 / %2")
                          .arg(formatSize(record.bytesDone), formatSize(record.bytesTotal));
    }
    if (!record.detail.isEmpty()) {
        statusText += QStringLiteral(" · %1").arg(record.detail);
    }
    setTextCell(row, StatusColumn, statusText);
    setActionCell(row, record);
}

void MainWindow::markTransferCompleted(const TransferRecord& record) {
    upsertTransfer(record);
    appendLog(QStringLiteral("%1完成：%2").arg(record.direction, record.fileName));
}

void MainWindow::markTransferFailed(const QString& transferId, const QString& reason) {
    Q_UNUSED(transferId)
    appendLog(QStringLiteral("传输失败：%1").arg(reason));
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("EGG-Delivery"));
    resize(1120, 720);

    QWidget* central = new QWidget(this);
    QVBoxLayout* root = new QVBoxLayout(central);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    root->addWidget(buildHeader());
    root->addWidget(buildToolbar());

    transferTable_ = new QTableWidget(this);
    configureTable();
    root->addWidget(transferTable_, 1);

    QHBoxLayout* logHeader = new QHBoxLayout;
    QLabel* logLabel = new QLabel(QStringLiteral("事件日志"), this);
    QPushButton* clearButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogResetButton),
                                               QStringLiteral("清空"),
                                               this);
    clearButton->setToolTip(QStringLiteral("清空事件日志"));
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearLog);
    logHeader->addWidget(logLabel);
    logHeader->addStretch();
    logHeader->addWidget(clearButton);
    root->addLayout(logHeader);

    logEdit_ = new QTextEdit(this);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(130);
    root->addWidget(logEdit_);

    setCentralWidget(central);

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f7f8fa; }"
        "QLabel { color: #20242a; }"
        "QLineEdit, QTextEdit, QTableWidget {"
        "  background: #ffffff;"
        "  border: 1px solid #d9dee7;"
        "  border-radius: 6px;"
        "}"
        "QPushButton {"
        "  background: #ffffff;"
        "  border: 1px solid #cfd6e3;"
        "  border-radius: 6px;"
        "  padding: 7px 12px;"
        "}"
        "QPushButton:hover { background: #eef4ff; border-color: #96b8f6; }"
        "QPushButton:disabled { color: #8c95a3; background: #f2f4f7; }"
        "QHeaderView::section {"
        "  background: #edf1f7;"
        "  border: 0;"
        "  border-bottom: 1px solid #d4dbe8;"
        "  padding: 8px;"
        "}"
        "QProgressBar {"
        "  border: 1px solid #cfd6e3;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "  background: #f3f5f8;"
        "}"
        "QProgressBar::chunk { background: #2f7df6; border-radius: 4px; }"));
}

QWidget* MainWindow::buildHeader() {
    QWidget* header = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel(QStringLiteral("EGG-Delivery"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);

    QLabel* subtitle = new QLabel(QStringLiteral("局域网聊天 + 文件传输 · Qt GUI"), this);
    subtitle->setStyleSheet(QStringLiteral("color: #5d6675;"));

    QVBoxLayout* titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    statusLabel_ = new QLabel(QStringLiteral("文件模块已接入"), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "background: #eaf7ef;"
        "color: #17683a;"
        "border: 1px solid #bfe6cc;"
        "border-radius: 6px;"
        "padding: 6px 10px;"));

    layout->addLayout(titleBox);
    layout->addStretch();
    layout->addWidget(statusLabel_);
    return header;
}

QWidget* MainWindow::buildToolbar() {
    QWidget* toolbar = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QLabel* dirLabel = new QLabel(QStringLiteral("下载目录"), this);
    downloadDirEdit_ = new QLineEdit(this);
    downloadDirEdit_->setReadOnly(true);

    chooseDirButton_ = new QPushButton(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                       QStringLiteral("选择"),
                                       this);
    chooseDirButton_->setToolTip(QStringLiteral("选择接收文件保存目录"));

    openDirButton_ = new QPushButton(style()->standardIcon(QStyle::SP_DirHomeIcon),
                                     QStringLiteral("打开"),
                                     this);
    openDirButton_->setToolTip(QStringLiteral("打开下载目录"));

    sendButton_ = new QPushButton(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                  QStringLiteral("发送文件"),
                                  this);
    sendButton_->setToolTip(QStringLiteral("选择一个或多个文件并开始发送"));

    loopbackCheck_ = new QCheckBox(QStringLiteral("本机回环演示"), this);
    loopbackCheck_->setChecked(true);
    loopbackCheck_->setToolTip(QStringLiteral("网络模块完成前，用本机模拟一次发送和接收"));

    layout->addWidget(dirLabel);
    layout->addWidget(downloadDirEdit_, 1);
    layout->addWidget(chooseDirButton_);
    layout->addWidget(openDirButton_);
    layout->addSpacing(12);
    layout->addWidget(loopbackCheck_);
    layout->addWidget(sendButton_);
    return toolbar;
}

void MainWindow::configureTable() {
    transferTable_->setColumnCount(ColumnCount);
    transferTable_->setHorizontalHeaderLabels({
        QStringLiteral("文件"),
        QStringLiteral("方向"),
        QStringLiteral("对端"),
        QStringLiteral("哈希"),
        QStringLiteral("进度"),
        QStringLiteral("状态"),
        QStringLiteral("操作"),
    });
    transferTable_->verticalHeader()->setVisible(false);
    transferTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    transferTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    transferTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    transferTable_->setAlternatingRowColors(true);
    transferTable_->horizontalHeader()->setStretchLastSection(false);
    transferTable_->horizontalHeader()->setSectionResizeMode(FileNameColumn, QHeaderView::Stretch);
    transferTable_->horizontalHeader()->setSectionResizeMode(DirectionColumn, QHeaderView::ResizeToContents);
    transferTable_->horizontalHeader()->setSectionResizeMode(PeerColumn, QHeaderView::ResizeToContents);
    transferTable_->horizontalHeader()->setSectionResizeMode(HashColumn, QHeaderView::ResizeToContents);
    transferTable_->horizontalHeader()->setSectionResizeMode(ProgressColumn, QHeaderView::Fixed);
    transferTable_->horizontalHeader()->setSectionResizeMode(StatusColumn, QHeaderView::Stretch);
    transferTable_->horizontalHeader()->setSectionResizeMode(ActionColumn, QHeaderView::ResizeToContents);
    transferTable_->setColumnWidth(ProgressColumn, 150);
}

int MainWindow::rowForTransfer(const QString& transferId) {
    if (transferRows_.contains(transferId)) {
        return transferRows_.value(transferId);
    }

    const int row = transferTable_->rowCount();
    transferTable_->insertRow(row);
    transferRows_.insert(transferId, row);
    transferTable_->setRowHeight(row, 42);
    return row;
}

void MainWindow::setTextCell(int row, int column, const QString& text) {
    QTableWidgetItem* item = transferTable_->item(row, column);
    if (!item) {
        item = new QTableWidgetItem;
        transferTable_->setItem(row, column, item);
    }
    item->setText(text);
    item->setToolTip(text);
}

void MainWindow::setProgressCell(int row, int progress) {
    QProgressBar* bar = qobject_cast<QProgressBar*>(transferTable_->cellWidget(row, ProgressColumn));
    if (!bar) {
        bar = new QProgressBar(transferTable_);
        bar->setRange(0, 100);
        bar->setFormat(QStringLiteral("%p%"));
        transferTable_->setCellWidget(row, ProgressColumn, bar);
    }
    bar->setValue(qBound(0, progress, 100));
}

void MainWindow::setActionCell(int row, const TransferRecord& record) {
    QPushButton* button = qobject_cast<QPushButton*>(transferTable_->cellWidget(row, ActionColumn));
    if (!button) {
        button = new QPushButton(style()->standardIcon(QStyle::SP_DialogCancelButton),
                                 QStringLiteral("取消"),
                                 transferTable_);
        transferTable_->setCellWidget(row, ActionColumn, button);
    }

    button->setEnabled(record.cancellable);
    button->setToolTip(record.cancellable
                           ? QStringLiteral("取消该文件传输")
                           : QStringLiteral("该传输已结束"));
    disconnect(button, nullptr, this, nullptr);
    connect(button, &QPushButton::clicked, this, [this, record]() {
        controller_->cancelTransfer(record.transferId);
    });
}

QString MainWindow::formatSize(quint64 bytes) const {
    constexpr double kb = 1024.0;
    constexpr double mb = kb * 1024.0;
    constexpr double gb = mb * 1024.0;

    if (bytes >= static_cast<quint64>(gb)) {
        return QStringLiteral("%1 GB").arg(bytes / gb, 0, 'f', 2);
    }
    if (bytes >= static_cast<quint64>(mb)) {
        return QStringLiteral("%1 MB").arg(bytes / mb, 0, 'f', 2);
    }
    if (bytes >= static_cast<quint64>(kb)) {
        return QStringLiteral("%1 KB").arg(bytes / kb, 0, 'f', 1);
    }
    return QStringLiteral("%1 B").arg(bytes);
}
