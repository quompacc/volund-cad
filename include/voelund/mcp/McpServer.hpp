#pragma once
#include <voelund/mcp/ToolHost.hpp>

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

class QTcpServer;
class QTcpSocket;

// AI04b (docs/implementation-plan/AI04-plan.md): MCP over local "Streamable HTTP" with plain
// JSON responses. Runs on the thread that owns the session (the GUI thread), so every
// mutation is published there.
namespace voelund::mcp {

// JSON-RPC 2.0 messages of MCP, independent of the transport.
class McpProtocol final {
  public:
    explicit McpProtocol(ToolHost& host) : host_(host) {}
    [[nodiscard]] static const QStringList& supportedVersions();
    // A response for requests; nullopt for notifications and responses of the client.
    [[nodiscard]] std::optional<QJsonObject> handle(const QJsonObject& message);

  private:
    ToolHost& host_;
};

// No signals of its own (no moc in this project); sockets are connected with lambdas.
class McpHttpServer final : public QObject {
  public:
    static constexpr qsizetype maximumRequestBytes = 1024 * 1024;

    // Requests need "Authorization: Bearer <token>".
    McpHttpServer(ToolHost& host, QByteArray token, QObject* parent = nullptr);
    ~McpHttpServer() override;
    // Only 127.0.0.1; port 0 picks a free port.
    [[nodiscard]] bool listen(quint16 port = 0);
    void close();
    [[nodiscard]] bool isListening() const;
    [[nodiscard]] quint16 port() const;
    [[nodiscard]] QString url() const;

  private:
    struct Response final {
        int status;
        QByteArray body;
        QList<QPair<QByteArray, QByteArray>> headers;
    };
    void accept();
    void read(QTcpSocket* socket);
    [[nodiscard]] Response respond(const QByteArray& method, const QByteArray& path,
                                   const QHash<QByteArray, QByteArray>& headers,
                                   const QByteArray& body);
    [[nodiscard]] bool trustedHost(const QByteArray& host) const;
    [[nodiscard]] bool trustedOrigin(const QByteArray& origin) const;

    McpProtocol protocol_;
    QByteArray token_;
    QTcpServer* server_;
    QHash<QTcpSocket*, QByteArray> buffers_;
    QSet<QByteArray> sessions_;
};

} // namespace voelund::mcp
