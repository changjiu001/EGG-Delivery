#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "file_transfer_controller.h"

#include <QHash>
#include <QMainWindow>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QTableWidget;
class QTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void chooseDownloadDirectory();
    void chooseFilesToSend();
    void openDownloadDirectory();
    void clearLog();
    void appendLog(const QString& message);
    void upsertTransfer(const TransferRecord& record);
    void markTransferCompleted(const TransferRecord& record);
    void markTransferFailed(const QString& transferId, const QString& reason);

private:
    enum Column {
        FileNameColumn = 0,
        DirectionColumn,
        PeerColumn,
        HashColumn,
        ProgressColumn,
        StatusColumn,
        ActionColumn,
        ColumnCount
    };

    void buildUi();
    QWidget* buildHeader();
    QWidget* buildToolbar();
    void configureTable();
    int rowForTransfer(const QString& transferId);
    void setTextCell(int row, int column, const QString& text);
    void setProgressCell(int row, int progress);
    void setActionCell(int row, const TransferRecord& record);
    QString formatSize(quint64 bytes) const;

    FileTransferController* controller_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLineEdit* downloadDirEdit_ = nullptr;
    QCheckBox* loopbackCheck_ = nullptr;
    QPushButton* sendButton_ = nullptr;
    QPushButton* chooseDirButton_ = nullptr;
    QPushButton* openDirButton_ = nullptr;
    QTableWidget* transferTable_ = nullptr;
    QTextEdit* logEdit_ = nullptr;

    QHash<QString, int> transferRows_;
};

#endif // MAIN_WINDOW_H
