#pragma once
#include <voelund/mcp/ToolHost.hpp>

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <utility>

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
    // A new token ends every session; clients need the new connect command.
    void setToken(QByteArray token);
    // AI03: short-lived tokens of panel runs, accepted besides the main token. A revoked
    // token is refused from the next request on; requests run on this thread, so a call is
    // either complete before the revocation or refused.
    [[nodiscard]] QByteArray issueRunToken();
    void revokeRunToken(const QByteArray& token);
    [[nodiscard]] bool hasRunTokens() const { return !runTokens_.isEmpty(); }
    // The main token serves external clients; off while only panel runs use the server.
    void setMainTokenAccepted(bool accepted) { mainTokenAccepted_ = accepted; }
    // Runs after every request that passed host, origin and token checks.
    void setActivityHandler(std::function<void()> activity) { activity_ = std::move(activity); }

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
    std::function<void()> activity_;
    QSet<QByteArray> runTokens_;
    bool mainTokenAccepted_{true};
};

} // namespace voelund::mcp
