#ifndef CONNECTIONMANAGER_V2_H
#define CONNECTIONMANAGER_V2_H

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTimer>

// Forward declarations
class ISPDetector;
class BackendAPI;
class XRayHandler;
class HealthMonitor;
class ProcessMonitor;
class SystemProxy;
class ProtocolGuard;

/**
 * @brief Enhanced Connection Manager with advanced features
 *
 * Features:
 * - Smart server selection with parallel latency testing
 * - Auto-reconnect with exponential backoff
 * - Integration with HealthMonitor, ProcessMonitor, SystemProxy
 * - Metrics reporting to backend
 * - Protocol fallback chain
 */
class ConnectionManagerV2 : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConnectionState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool autoReconnectEnabled READ autoReconnectEnabled WRITE setAutoReconnectEnabled NOTIFY
                       autoReconnectEnabledChanged)
    Q_PROPERTY(int latency READ currentLatency NOTIFY latencyChanged)

public:
    enum ConnectionState {
        Disconnected,
        DetectingISP,
        FetchingServers,
        TestingServers, // New: parallel latency testing
        SelectingServer,
        FetchingConfig,
        Connecting,
        Connected,
        Reconnecting, // New: auto-reconnect state
        Disconnecting,
        Error
    };
    Q_ENUM(ConnectionState)

    explicit ConnectionManagerV2(QObject *parent = nullptr);
    ~ConnectionManagerV2();

    static ConnectionManagerV2 *instance();

    // State accessors
    ConnectionState state() const;
    QString statusMessage() const;
    int currentLatency() const;
    QString currentServerId() const;
    QString currentServerLocation() const;

    // Auto-reconnect settings
    bool autoReconnectEnabled() const;
    void setAutoReconnectEnabled(bool enabled);
    int maxReconnectAttempts() const;
    void setMaxReconnectAttempts(int max);

public slots:
    /**
     * @brief Start connection flow
     */
    void connectToFlow();

    /**
     * @brief Disconnect from FLOW network
     */
    void disconnectFromFlow();

    /**
     * @brief Force reconnection to a different server
     */
    void reconnectToNewServer();

    /**
     * @brief Cancel ongoing reconnection attempts
     */
    void cancelReconnect();

signals:
    void stateChanged();
    void statusMessageChanged();
    void latencyChanged();
    void autoReconnectEnabledChanged();

    // Advanced signals
    void serverLatencyMeasured(const QString &serverId, int latencyMs);
    void bestServerSelected(const QString &serverId, const QString &location, int latencyMs);
    void reconnectAttempt(int attempt, int maxAttempts);
    void reconnectFailed();
    void connectionHealthChanged(bool healthy);

private slots:
    // ISP detection
    void onISPDetected(const QString &isp, const QString &asn);
    void onISPError(const QString &error);

    // Server handling
    void onServersFetched(const QJsonArray &servers);
    void onServersFetchFailed(const QString &error);
    void onServerLatencyResult(const QString &serverId, int latencyMs);

    // Config handling
    void onConfigFetched(const QJsonObject &config);
    void onConfigFetchFailed(const QString &error);

    // XRay events
    void onXRayStarted();
    void onXRayStopped();
    void onXRayError(const QString &error);

    // Health monitoring
    void onConnectionDegraded(const QString &reason);
    void onConnectionRecovered();

    // Process monitoring
    void onCS2Launched();
    void onCS2Exited();

    // Auto-reconnect
    void attemptReconnect();

private:
    void setState(ConnectionState state);
    void setStatusMessage(const QString &message);

    // Server selection
    void startServerLatencyTests(const QJsonArray &servers);
    void selectBestServer();

    // Reconnection
    void scheduleReconnect();
    void resetReconnectState();
    int calculateBackoffDelay() const;

    // Metrics
    void reportConnectionSuccess();
    void reportConnectionFailure(const QString &reason);

    static ConnectionManagerV2 *s_instance;

    // Core components
    ISPDetector *m_ispDetector;
    BackendAPI *m_backendAPI;
    XRayHandler *m_xrayHandler;
    HealthMonitor *m_healthMonitor;
    ProcessMonitor *m_processMonitor;
    SystemProxy *m_systemProxy;
    ProtocolGuard *m_protocolGuard;

    // State
    ConnectionState m_state;
    QString m_statusMessage;
    QString m_currentISP;

    // Server selection
    QJsonArray m_availableServers;
    QMap<QString, int> m_serverLatencies; // serverId -> latency
    int m_pendingLatencyTests;
    QString m_selectedServerId;
    QString m_selectedServerLocation;
    int m_currentLatency;

    // Config
    QJsonObject m_currentConfig;

    // Auto-reconnect
    bool m_autoReconnectEnabled;
    int m_reconnectAttempts;
    int m_maxReconnectAttempts;
    QTimer *m_reconnectTimer;

    // Constants
    static const int INITIAL_BACKOFF_MS = 1000; // 1 second
    static const int MAX_BACKOFF_MS = 16000;    // 16 seconds
    static const int DEFAULT_MAX_RECONNECT_ATTEMPTS = 5;
    static const int LATENCY_TEST_TIMEOUT_MS = 5000;
};

#endif // CONNECTIONMANAGER_V2_H
