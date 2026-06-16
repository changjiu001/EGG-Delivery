#include "tcp_connection.h"

#include <utility>

TcpConnection::TcpConnection(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket)
{
    m_socket->setParent(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &TcpConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &TcpConnection::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpConnection::onDisconnected);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpConnection::onSocketError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &TcpConnection::onSocketError);
#endif
}

void TcpConnection::connectToHost(const QHostAddress &address, quint16 port)
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_socket->connectToHost(address, port);
    }
}

void TcpConnection::send(const WireMessage &message)
{
    const QByteArray frame = WireProtocol::encode(message);
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(frame);
    } else {
        m_pendingFrames.append(frame);
    }
}

void TcpConnection::close()
{
    m_socket->disconnectFromHost();
}

QAbstractSocket::SocketState TcpConnection::state() const
{
    return m_socket->state();
}

QString TcpConnection::peerId() const
{
    return m_peerId;
}

void TcpConnection::setPeerId(const QString &peerId)
{
    m_peerId = peerId;
}

void TcpConnection::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    QString error;
    const QList<WireMessage> messages = WireProtocol::takeMessages(m_buffer, &error);
    if (!error.isEmpty()) {
        emit errorOccurred(this, error);
    }

    for (const WireMessage &message : messages) {
        emit messageReceived(this, message);
    }
}

void TcpConnection::onConnected()
{
    flushPending();
}

void TcpConnection::onDisconnected()
{
    emit disconnected(this);
}

void TcpConnection::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(this, m_socket->errorString());
}

void TcpConnection::flushPending()
{
    for (const QByteArray &frame : std::as_const(m_pendingFrames)) {
        m_socket->write(frame);
    }
    m_pendingFrames.clear();
}
