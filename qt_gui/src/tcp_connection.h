#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include "wire_protocol.h"

#include <QObject>
#include <QPointer>
#include <QTcpSocket>

class TcpConnection : public QObject
{
    Q_OBJECT

public:
    explicit TcpConnection(QTcpSocket *socket, QObject *parent = nullptr);

    void connectToHost(const QHostAddress &address, quint16 port);
    void send(const WireMessage &message);
    void close();

    QAbstractSocket::SocketState state() const;
    QString peerId() const;
    void setPeerId(const QString &peerId);

signals:
    void messageReceived(TcpConnection *connection, const WireMessage &message);
    void disconnected(TcpConnection *connection);
    void errorOccurred(TcpConnection *connection, const QString &message);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void flushPending();

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QList<QByteAray> m_pendingFrames;
    QString m_peerId;
};

#endif // TCP_CONNECTION_H
