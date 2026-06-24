namespace {

int directionToInt(TransferDirection direction)
{
    return direction == TransferDirection::Upload ? 0 : 1;
}

TransferDirection directionFromInt(int value)
{
    return value == 1 ? TransferDirection::Download : TransferDirection::Upload;
}

int stateToInt(TransferState state)
{
    return static_cast<int>(state);
}

TransferState stateFromInt(int value)
{
    switch (value) {
    case 0: return TransferState::Waiting;
    case 1: return TransferState::Transferring;
    case 2: return TransferState::Finished;
    case 3: return TransferState::Rejected;
    case 4: return TransferState::Failed;
    case 5: return TransferState::Cancelled;
    default: return TransferState::Failed;
    }
}

bool isActiveState(TransferState state)
{
    return state == TransferState::Waiting || state == TransferState::Transferring;
}

QString formatTransferTime(qint64 timestampMs)
{
    if (timestampMs <= 0) {
        return QStringLiteral("-");
    }
    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QJsonObject transferToJson(const TransferView &view)
{
    QJsonObject object;
    object["transferId"] = view.transferId;
    object["peerId"] = view.peerId;
    object["peerName"] = view.peerName;
    object["fileName"] = view.fileName;
    object["localPath"] = view.localPath;
    object["fileSize"] = QString::number(view.fileSize);
    object["progress"] = view.progress;
    object["direction"] = directionToInt(view.direction);
    object["state"] = stateToInt(view.state);
    object["message"] = view.message;
    object["updatedAtMs"] = QString::number(view.updatedAtMs);
    return object;
}

TransferView transferFromJson(const QJsonObject &object)
{
    TransferView view;
    view.transferId = object.value("transferId").toString();
    view.peerId = object.value("peerId").toString();
    view.peerName = object.value("peerName").toString();
    view.fileName = object.value("fileName").toString();
    view.localPath = object.value("localPath").toString();
    view.fileSize = object.value("fileSize").toString().toLongLong();
    view.progress = object.value("progress").toInt();
    view.direction = directionFromInt(object.value("direction").toInt());
    view.state = stateFromInt(object.value("state").toInt());
    view.message = object.value("message").toString();
    view.updatedAtMs = object.value("updatedAtMs").toString().toLongLong();
    return view;
}

} // namespace

void MainWindow::onTransferCreated(const TransferView &transfer)
{
    TransferView view = transfer;
    if (view.updatedAtMs <= 0) {
        view.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    }

    m_transfers.insert(view.transferId, view);
    upsertTransferRow(view);
    saveTransferHistory();
}

void MainWindow::onTransferUpdated(const TransferView &transfer)
{
    TransferView view = transfer;
    if (view.updatedAtMs <= 0) {
        view.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    }

    m_transfers.insert(view.transferId, view);
    upsertTransferRow(view);
    saveTransferHistory();
}

QString MainWindow::transferHistoryFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("transfer_history.json"));
}

void MainWindow::loadTransferHistory()
{
    QFile file(transferHistoryFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonArray records = document.array();
    bool normalized = false;
    for (const QJsonValue &value : records) {
        if (!value.isObject()) {
            continue;
        }

        TransferView view = transferFromJson(value.toObject());
        if (view.transferId.isEmpty() || view.fileName.isEmpty()) {
            continue;
        }

        if (view.updatedAtMs <= 0) {
            view.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
            normalized = true;
        }

        if (isActiveState(view.state)) {
            view.state = TransferState::Cancelled;
            view.message = QStringLiteral("上次关闭时未完成");
            normalized = true;
        }

        m_transfers.insert(view.transferId, view);
        upsertTransferRow(view);
    }

    if (normalized) {
        saveTransferHistory();
    }
}

void MainWindow::saveTransferHistory() const
{
    QJsonArray records;
    for (const TransferView &view : m_transfers) {
        records.append(transferToJson(view));
    }

    QFile file(transferHistoryFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }

    file.write(QJsonDocument(records).toJson(QJsonDocument::Indented));
}

void MainWindow::onNetworkError(const QString &message)
{
    statusBar()->showMessage(message, 6000);
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("LanChat Qt — 无中心局域网聊天"));
    resize(1060, 680);

    auto *central = new QWidget(this);
    central->setObjectName("centralShell");
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    auto *left = new QWidget(central);
    left->setObjectName("leftPanel");
    left->setFixedWidth(270);
    buildLeftPanel(left);
    root->addWidget(left);

    auto *tabs = new QTabWidget(central);
    tabs->setObjectName("mainTabs");
    auto *chatTab = new QWidget(tabs);
    auto *transferTab = new QWidget(tabs);
    buildChatTab(chatTab);
    buildTransferTab(transferTab);
    tabs->addTab(chatTab, QStringLiteral("聊天"));
    tabs->addTab(transferTab, QStringLiteral("文件传输"));
    root->addWidget(tabs, 1);

    setCentralWidget(central);

}

void MainWindow::buildLeftPanel(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("EGG Delivery ✦"), parent);
    title->setObjectName("titleLabel");
    layout->addWidget(title);

    layout->addWidget(new QLabel(QStringLiteral("昵称"), parent));
    m_nameEdit = new QLineEdit(parent);
    m_nameEdit->setPlaceholderText(QStringLiteral("输入你的昵称"));
    m_nameEdit->setText(qEnvironmentVariable("USERNAME", QStringLiteral("QtUser")));
    layout->addWidget(m_nameEdit);

    layout->addWidget(new QLabel(QStringLiteral("TCP 端口"), parent));
    m_portSpin = new QSpinBox(parent);
    m_portSpin->setRange(10000, 65535);
    m_portSpin->setValue(50001);
    layout->addWidget(m_portSpin);

    m_startButton = new QPushButton(QStringLiteral("启动"), parent);
    m_startButton->setObjectName(QStringLiteral("startButton"));
    layout->addWidget(m_startButton);

    m_statusLabel = new QLabel(QStringLiteral("状态：未启动"), parent);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *manualTitle = new QLabel(QStringLiteral("手动连接"), parent);
    manualTitle->setObjectName("sectionLabel");
    layout->addWidget(manualTitle);

    m_manualIpEdit = new QLineEdit(parent);
    m_manualIpEdit->setPlaceholderText(QStringLiteral("对方 IP，例如 192.168.137.45"));
    layout->addWidget(m_manualIpEdit);

    auto *manualRow = new QHBoxLayout;
    m_manualPortSpin = new QSpinBox(parent);
    m_manualPortSpin->setRange(10000, 65535);
    m_manualPortSpin->setValue(50001);
    m_manualAddButton = new QPushButton(QStringLiteral("添加"), parent);
    manualRow->addWidget(m_manualPortSpin);
    manualRow->addWidget(m_manualAddButton);
    layout->addLayout(manualRow);
    m_manualIpEdit->setEnabled(false);
    m_manualPortSpin->setEnabled(false);
    m_manualAddButton->setEnabled(false);

    auto *peerTitle = new QLabel(QStringLiteral("在线用户"), parent);
    peerTitle->setObjectName("sectionLabel");
    layout->addWidget(peerTitle);

    m_peerList = new QListWidget(parent);
    m_peerList->setAlternatingRowColors(true);
    layout->addWidget(m_peerList, 1);

    auto *dirTitle = new QLabel(QStringLiteral("接收目录"), parent);
    dirTitle->setObjectName("sectionLabel");
    layout->addWidget(dirTitle);

    m_downloadDirLabel = new QLabel(parent);
    m_downloadDirLabel->setWordWrap(true);
    m_downloadDirLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_downloadDirLabel);
}

void MainWindow::buildChatTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    m_chatView = new QTextBrowser(tab);
    m_chatView->setObjectName("chatView");
    m_chatView->setOpenExternalLinks(false);
    layout->addWidget(m_chatView, 1);

    m_stickerPanel = new QWidget(tab);
    m_stickerPanel->setObjectName("stickerPanel");
    auto *panelLayout = new QVBoxLayout(m_stickerPanel);
    panelLayout->setContentsMargins(12, 10, 12, 12);
    panelLayout->setSpacing(10);

    auto *panelTitle = new QLabel(QStringLiteral("表情包"), m_stickerPanel);
    panelTitle->setObjectName("stickerPanelTitle");
    panelLayout->addWidget(panelTitle);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    int index = 0;
    constexpr int Columns = 4;
    for (const auto &sticker : allStickers()) {
        auto *button = new QToolButton(m_stickerPanel);
        button->setObjectName("stickerButton");
        button->setToolTip(QString());
        button->setIcon(QIcon(sticker.resourcePath));
        const QPixmap stickerPixmap(sticker.resourcePath);
        button->setIconSize(stickerPixmap.isNull()
            ? QSize(88, 88)
            : stickerPixmap.size().boundedTo(QSize(88, 88)));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(104, 104);
        connect(button, &QToolButton::clicked, this, [this, sticker]() {
            sendStickerById(sticker.id);
        });
        grid->addWidget(button, index / Columns, index % Columns);
        ++index;
    }

    grid->setColumnStretch(Columns, 1);
    panelLayout->addLayout(grid);
    m_stickerPanel->setVisible(false);
    layout->addWidget(m_stickerPanel);

    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(10);
    m_messageEdit = new QLineEdit(tab);
