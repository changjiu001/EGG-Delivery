#include <QFile>
    m_messageEdit->setPlaceholderText(QStringLiteral("输入消息，Enter 发送"));
    m_stickerToggleButton = new QPushButton(QStringLiteral("表情包"), tab);
    m_sendButton = new QPushButton(QStringLiteral("发送"), tab);
    m_fileButton = new QPushButton(QStringLiteral("发送文件"), tab);
    bottom->addWidget(m_messageEdit, 1);
    bottom->addWidget(m_stickerToggleButton);
    bottom->addWidget(m_sendButton);
    bottom->addWidget(m_fileButton);
    layout->addLayout(bottom);
}

void MainWindow::buildTransferTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    m_dirButton = new QPushButton(QStringLiteral("设置接收目录"), tab);
    m_openDirButton = new QPushButton(QStringLiteral("打开接收目录"), tab);
    m_clearButton = new QPushButton(QStringLiteral("清理已结束记录"), tab);
    m_deleteButton = new QPushButton(QStringLiteral("删除选中记录"), tab);
    top->addWidget(m_dirButton);
    top->addWidget(m_openDirButton);
    top->addStretch(1);
    top->addWidget(m_deleteButton);
    top->addWidget(m_clearButton);
    layout->addLayout(top);

    m_transferTable = new QTableWidget(tab);
    m_transferTable->setColumnCount(9);
    m_transferTable->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("文件"),
        QStringLiteral("对象"),
        QStringLiteral("方向"),
        QStringLiteral("大小"),
        QStringLiteral("时间"),
        QStringLiteral("位置"),
        QStringLiteral("进度"),
        QStringLiteral("状态"),
        QStringLiteral("操作")
    });
    m_transferTable->horizontalHeader()->setStretchLastSection(false);
    m_transferTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_transferTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_transferTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_transferTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_transferTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_transferTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_transferTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_transferTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_transferTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    m_transferTable->verticalHeader()->setVisible(false);
    m_transferTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transferTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_transferTable->setTextElideMode(Qt::ElideRight);
    m_transferTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_transferTable->viewport()->installEventFilter(this);
    layout->addWidget(m_transferTable, 1);
}

void MainWindow::connectSignals()
{
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::toggleNetwork);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::sendCurrentText);
    connect(m_messageEdit, &QLineEdit::returnPressed, this, &MainWindow::sendCurrentText);
    connect(m_stickerToggleButton, &QPushButton::clicked, this, [this]() {
        if (m_stickerPanel) {
            m_stickerPanel->setVisible(!m_stickerPanel->isVisible());
        }
    });
    connect(m_fileButton, &QPushButton::clicked, this, &MainWindow::chooseAndSendFile);
    connect(m_dirButton, &QPushButton::clicked, this, &MainWindow::chooseDownloadDirectory);
    connect(m_openDirButton, &QPushButton::clicked, this, &MainWindow::openDownloadDirectory);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearFinishedTransfers);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTransfer);
    connect(m_transferTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::showTransferContextMenu);
    connect(m_manualAddButton, &QPushButton::clicked, this, &MainWindow::addManualPeer);

    connect(&m_network, &LanNetworkClient::started, this, &MainWindow::onStarted);
    connect(&m_network, &LanNetworkClient::stopped, this, &MainWindow::onStopped);
    connect(&m_network, &LanNetworkClient::peerOnline, this, &MainWindow::onPeerOnline);
    connect(&m_network, &LanNetworkClient::peerOffline, this, &MainWindow::onPeerOffline);
    connect(&m_network, &LanNetworkClient::textReceived, this, &MainWindow::onTextReceived);
    connect(&m_network, &LanNetworkClient::stickerReceived, this, &MainWindow::onStickerReceived);
    connect(&m_network, &LanNetworkClient::textSendFailed, this, &MainWindow::onTextSendFailed);
    connect(&m_network, &LanNetworkClient::fileOfferReceived, this, &MainWindow::onFileOfferReceived);
    connect(&m_network, &LanNetworkClient::transferCreated, this, &MainWindow::onTransferCreated);
    connect(&m_network, &LanNetworkClient::transferUpdated, this, &MainWindow::onTransferUpdated);
    connect(&m_network, &LanNetworkClient::errorOccurred, this, &MainWindow::onNetworkError);
}

void MainWindow::applyStyle()
{
    qApp->setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(15, 23, 42));
    palette.setColor(QPalette::WindowText, QColor(248, 250, 252));
    palette.setColor(QPalette::Base, QColor(15, 23, 42));
    palette.setColor(QPalette::AlternateBase, QColor(30, 41, 59));
    palette.setColor(QPalette::Text, QColor(248, 250, 252));
    palette.setColor(QPalette::Button, QColor(30, 41, 59));
    palette.setColor(QPalette::ButtonText, QColor(248, 250, 252));
    palette.setColor(QPalette::BrightText, QColor(248, 113, 113));
    palette.setColor(QPalette::Highlight, QColor(37, 99, 235));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));
    palette.setColor(QPalette::ToolTipText, QColor(248, 250, 252));
    qApp->setPalette(palette);

    QFile styleFile(QStringLiteral(":/assets/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
}

