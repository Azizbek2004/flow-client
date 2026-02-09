#include "ConnectionManagerV2.h"
#include "../protocols/XRayHandler.h"
#include "BackendAPI.h"
#include "HealthMonitor.h"
#include "ISPDetector.h"
#include "ProcessMonitor.h"
#include "ProtocolGuard.h"
#include "SystemProxy.h"

#include <QDebug>
#include <QtMath>

ConnectionManagerV2 *ConnectionManagerV2::s_instance = nullptr;

ConnectionManagerV2::ConnectionManagerV2(QObject *parent)
    : QObject(parent),
      m_state(Disconnected),
      m_pendingLatencyTests(0),
      m_currentLatency(0),
      m_autoReconnectEnabled(true),
      m_reconnectAttempts(0),
      m_maxReconnectAttempts(DEFAULT_MAX_RECONNECT_ATTEMPTS),
      m_reconnectTimer(new QTimer(this))
{
    // Initialize core components
    m_ispDetector = ISPDetector::instance();
    m_backendAPI = BackendAPI::instance();
    m_xrayHandler = new XRayHandler(this);
    m_healthMonitor = HealthMonitor::instance();
    m_processMonitor = ProcessMonitor::instance();
    m_systemProxy = SystemProxy::instance();
    m_protocolGuard = ProtocolGuard::instance();

    // ISP detection signals
    connect(m_ispDetector, &ISPDetector::ispDetected, this, &ConnectionManagerV2::onISPDetected);
    connect(m_ispDetector, &ISPDetector::errorOccurred, this, &ConnectionManagerV2::onISPError);

    // Backend API signals
    connect(m_backendAPI, &BackendAPI::serversFetched, this, &ConnectionManagerV2::onServersFetched);
    connect(m_backendAPI, &BackendAPI::serversFetchFailed, this, &ConnectionManagerV2::onServersFetchFailed);
    connect(m_backendAPI, &BackendAPI::configFetched, this, &ConnectionManagerV2::onConfigFetched);
    connect(m_backendAPI, &BackendAPI::configFetchFailed, this, &ConnectionManagerV2::onConfigFetchFailed);

    // XRay signals
    connect(m_xrayHandler, &XRayHandler::started, this, &ConnectionManagerV2::onXRayStarted);
    connect(m_xrayHandler, &XRayHandler::stopped, this, &ConnectionManagerV2::onXRayStopped);
    connect(m_xrayHandler, &XRayHandler::errorOccurred, this, &ConnectionManagerV2::onXRayError);

    // Health monitoring signals
    connect(m_healthMonitor, &HealthMonitor::connectionDegraded, this, &ConnectionManagerV2::onConnectionDegraded);
    connect(m_healthMonitor, &HealthMonitor::connectionRecovered, this, &ConnectionManagerV2::onConnectionRecovered);

    // Process monitoring signals (CS2 detection)
    connect(m_processMonitor, &ProcessMonitor::cs2Launched, this, &ConnectionManagerV2::onCS2Launched);
    connect(m_processMonitor, &ProcessMonitor::cs2Exited, this, &ConnectionManagerV2::onCS2Exited);

    // Reconnect timer
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ConnectionManagerV2::attemptReconnect);

    // Start process monitoring
    m_processMonitor->startMonitoring();

    s_instance = this;
}

ConnectionManagerV2::~ConnectionManagerV2()
{
    disconnectFromFlow();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ConnectionManagerV2 *ConnectionManagerV2::instance()
{
    if (!s_instance) {
        s_instance = new ConnectionManagerV2();
    }
    return s_instance;
}

// ============== State Accessors ==============

ConnectionManagerV2::ConnectionState ConnectionManagerV2::state() const
{
    return m_state;
}

QString ConnectionManagerV2::statusMessage() const
{
    return m_statusMessage;
}

int ConnectionManagerV2::currentLatency() const
{
    return m_currentLatency;
}

QString ConnectionManagerV2::currentServerId() const
{
    return m_selectedServerId;
}

QString ConnectionManagerV2::currentServerLocation() const
{
    return m_selectedServerLocation;
}

bool ConnectionManagerV2::autoReconnectEnabled() const
{
    return m_autoReconnectEnabled;
}

void ConnectionManagerV2::setAutoReconnectEnabled(bool enabled)
{
    if (m_autoReconnectEnabled != enabled) {
        m_autoReconnectEnabled = enabled;
        emit autoReconnectEnabledChanged();
    }
}

int ConnectionManagerV2::maxReconnectAttempts() const
{
    return m_maxReconnectAttempts;
}

void ConnectionManagerV2::setMaxReconnectAttempts(int max)
{
    m_maxReconnectAttempts = qMax(1, max);
}

// ============== State Management ==============

void ConnectionManagerV2::setState(ConnectionState state)
{
    if (m_state != state) {
        qDebug() << "[ConnectionManagerV2] State:" << m_state << "->" << state;
        m_state = state;
        emit stateChanged();
    }
}

void ConnectionManagerV2::setStatusMessage(const QString &message)
{
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
        qDebug() << "[ConnectionManagerV2]" << message;
    }
}

// ============== Connection Flow ==============

void ConnectionManagerV2::connectToFlow()
{
    if (m_state != Disconnected && m_state != Error) {
        qDebug() << "[ConnectionManagerV2] Cannot connect: already in state" << m_state;
        return;
    }

    resetReconnectState();
    setState(DetectingISP);
    setStatusMessage("Detecting ISP...");
    m_ispDetector->detectISP();
}

void ConnectionManagerV2::disconnectFromFlow()
{
    cancelReconnect();

    if (m_state == Disconnected) {
        return;
    }

    setState(Disconnecting);
    setStatusMessage("Disconnecting...");

    // Stop health monitoring
    m_healthMonitor->stopMonitoring();

    // Disable system proxy
    m_systemProxy->disableProxy();

    // Stop XRay
    m_xrayHandler->stop();
}

void ConnectionManagerV2::reconnectToNewServer()
{
    if (m_availableServers.isEmpty()) {
        connectToFlow();
        return;
    }

    // Remove current server from selection
    QJsonArray remainingServers;
    for (const QJsonValue &server : m_availableServers) {
        if (server.toObject()["id"].toString() != m_selectedServerId) {
            remainingServers.append(server);
        }
    }

    if (remainingServers.isEmpty()) {
        setStatusMessage("No alternative servers available.");
        return;
    }

    m_availableServers = remainingServers;
    disconnectFromFlow();

    // Trigger new server selection after disconnect
    QTimer::singleShot(1000, this, [this]() {
        setState(TestingServers);
        startServerLatencyTests(m_availableServers);
    });
}

void ConnectionManagerV2::cancelReconnect()
{
    m_reconnectTimer->stop();
    resetReconnectState();
}

// ============== ISP Detection ==============

void ConnectionManagerV2::onISPDetected(const QString &isp, const QString &asn)
{
    if (m_state != DetectingISP)
        return;

    m_currentISP = isp;
    setStatusMessage(QString("Detected ISP: %1. Fetching servers...").arg(isp));
    setState(FetchingServers);
    m_backendAPI->fetchServers(isp);
}

void ConnectionManagerV2::onISPError(const QString &error)
{
    if (m_state != DetectingISP)
        return;

    setStatusMessage(QString("ISP detection failed: %1").arg(error));
    setState(Error);
    reportConnectionFailure("ISP detection failed");
}

// ============== Server Selection ==============

void ConnectionManagerV2::onServersFetched(const QJsonArray &servers)
{
    if (m_state != FetchingServers)
        return;

    if (servers.isEmpty()) {
        setStatusMessage("No servers available for your region.");
        setState(Error);
        reportConnectionFailure("No servers available");
        return;
    }

    m_availableServers = servers;
    int serverCount = servers.size();

    if (serverCount == 1) {
        // Only one server, skip latency testing
        setStatusMessage("Fetching configuration...");
        m_selectedServerId = servers.first().toObject()["id"].toString();
        m_selectedServerLocation = servers.first().toObject()["location"].toString();
        setState(FetchingConfig);
        m_backendAPI->fetchConfig(m_selectedServerId);
    } else {
        // Multiple servers, test latency to each
        setState(TestingServers);
        setStatusMessage(QString("Testing latency to %1 servers...").arg(serverCount));
        startServerLatencyTests(servers);
    }
}

void ConnectionManagerV2::onServersFetchFailed(const QString &error)
{
    if (m_state != FetchingServers)
        return;

    setStatusMessage(QString("Failed to fetch servers: %1").arg(error));
    setState(Error);
    reportConnectionFailure("Server fetch failed");
}

void ConnectionManagerV2::startServerLatencyTests(const QJsonArray &servers)
{
    m_serverLatencies.clear();
    m_pendingLatencyTests = servers.size();

    for (const QJsonValue &serverVal : servers) {
        QJsonObject server = serverVal.toObject();
        QString serverId = server["id"].toString();
        QString address = server["address"].toString();

        // Use ISPDetector's ping functionality
        m_ispDetector->measureLatency(address);

        // Connect result handler (simplified - in production, track per-server)
        // For MVP, we'll use a single-shot connection pattern
        QMetaObject::Connection *conn = new QMetaObject::Connection();
        *conn = connect(m_ispDetector, &ISPDetector::latencyMeasured, this,
                        [this, serverId, conn](const QString &host, int ms) {
                            disconnect(*conn);
                            delete conn;
                            onServerLatencyResult(serverId, ms);
                        });
    }

    // Timeout for latency tests
    QTimer::singleShot(LATENCY_TEST_TIMEOUT_MS, this, [this]() {
        if (m_state == TestingServers && m_pendingLatencyTests > 0) {
            qDebug() << "[ConnectionManagerV2] Latency test timeout, proceeding with available results";
            m_pendingLatencyTests = 0;
            selectBestServer();
        }
    });
}

void ConnectionManagerV2::onServerLatencyResult(const QString &serverId, int latencyMs)
{
    if (m_state != TestingServers)
        return;

    m_serverLatencies[serverId] = latencyMs;
    emit serverLatencyMeasured(serverId, latencyMs);

    m_pendingLatencyTests--;
    setStatusMessage(QString("Testing servers... (%1 remaining)").arg(m_pendingLatencyTests));

    if (m_pendingLatencyTests <= 0) {
        selectBestServer();
    }
}

void ConnectionManagerV2::selectBestServer()
{
    if (m_serverLatencies.isEmpty()) {
        // Fallback: use first server
        if (!m_availableServers.isEmpty()) {
            m_selectedServerId = m_availableServers.first().toObject()["id"].toString();
            m_selectedServerLocation = m_availableServers.first().toObject()["location"].toString();
            m_currentLatency = 0;
        } else {
            setState(Error);
            setStatusMessage("No servers to select from.");
            return;
        }
    } else {
        // Find server with lowest latency
        QString bestId;
        int bestLatency = INT_MAX;

        for (auto it = m_serverLatencies.constBegin(); it != m_serverLatencies.constEnd(); ++it) {
            if (it.value() < bestLatency) {
                bestLatency = it.value();
                bestId = it.key();
            }
        }

        m_selectedServerId = bestId;
        m_currentLatency = bestLatency;

        // Find location from server list
        for (const QJsonValue &server : m_availableServers) {
            if (server.toObject()["id"].toString() == bestId) {
                m_selectedServerLocation = server.toObject()["location"].toString();
                break;
            }
        }
    }

    qDebug() << "[ConnectionManagerV2] Selected server:" << m_selectedServerId << "(" << m_selectedServerLocation
             << ") with latency" << m_currentLatency << "ms";

    emit bestServerSelected(m_selectedServerId, m_selectedServerLocation, m_currentLatency);
    emit latencyChanged();

    setState(FetchingConfig);
    setStatusMessage(
            QString("Selected %1 (%2ms). Fetching config...").arg(m_selectedServerLocation).arg(m_currentLatency));

    m_backendAPI->fetchConfig(m_selectedServerId);
}

// ============== Configuration ==============

void ConnectionManagerV2::onConfigFetched(const QJsonObject &config)
{
    if (m_state != FetchingConfig)
        return;

    m_currentConfig = config;
    setState(Connecting);
    setStatusMessage("Starting secure connection...");
    m_xrayHandler->start(config);
}

void ConnectionManagerV2::onConfigFetchFailed(const QString &error)
{
    if (m_state != FetchingConfig)
        return;

    setStatusMessage(QString("Config fetch failed: %1").arg(error));
    setState(Error);
    reportConnectionFailure("Config fetch failed");
}

// ============== XRay Events ==============

void ConnectionManagerV2::onXRayStarted()
{
    setState(Connected);
    setStatusMessage(QString("Connected to %1").arg(m_selectedServerLocation));

    // Enable system proxy
    m_systemProxy->enableProxy("127.0.0.1", 10808);

    // Start health monitoring
    m_healthMonitor->startMonitoring(m_currentConfig["server"].toObject()["address"].toString(),
                                     5000, // 5 second interval
                                     5     // 5 pings per check
    );

    // Report success
    reportConnectionSuccess();
    resetReconnectState();
}

void ConnectionManagerV2::onXRayStopped()
{
    bool wasConnected = (m_state == Connected || m_state == Reconnecting);

    m_healthMonitor->stopMonitoring();
    m_systemProxy->disableProxy();

    if (m_state == Disconnecting) {
        setState(Disconnected);
        setStatusMessage("Disconnected.");
    } else if (wasConnected && m_autoReconnectEnabled) {
        // Unexpected disconnect, try to reconnect
        scheduleReconnect();
    } else {
        setState(Disconnected);
        setStatusMessage("Connection lost.");
    }
}

void ConnectionManagerV2::onXRayError(const QString &error)
{
    setStatusMessage(QString("Connection error: %1").arg(error));

    m_healthMonitor->stopMonitoring();
    m_systemProxy->disableProxy();

    if (m_xrayHandler->isRunning()) {
        m_xrayHandler->stop();
    }

    if (m_autoReconnectEnabled && m_reconnectAttempts < m_maxReconnectAttempts) {
        scheduleReconnect();
    } else {
        setState(Error);
        reportConnectionFailure(error);
    }
}

// ============== Health Monitoring ==============

void ConnectionManagerV2::onConnectionDegraded(const QString &reason)
{
    qDebug() << "[ConnectionManagerV2] Connection degraded:" << reason;
    setStatusMessage(QString("Connection unstable: %1").arg(reason));
    emit connectionHealthChanged(false);
}

void ConnectionManagerV2::onConnectionRecovered()
{
    qDebug() << "[ConnectionManagerV2] Connection recovered";
    setStatusMessage(QString("Connected to %1").arg(m_selectedServerLocation));
    emit connectionHealthChanged(true);
}

// ============== CS2 Process Monitoring ==============

void ConnectionManagerV2::onCS2Launched()
{
    qDebug() << "[ConnectionManagerV2] CS2 launched, locking protocol";
    m_protocolGuard->setLocked(true);
    m_protocolGuard->setMatchActive(true);

    if (m_state == Disconnected || m_state == Error) {
        // Auto-connect when CS2 launches
        setStatusMessage("CS2 detected, connecting...");
        connectToFlow();
    }
}

void ConnectionManagerV2::onCS2Exited()
{
    qDebug() << "[ConnectionManagerV2] CS2 exited, unlocking protocol";
    m_protocolGuard->setMatchActive(false);
    m_protocolGuard->setLocked(false);
}

// ============== Auto-Reconnect ==============

void ConnectionManagerV2::scheduleReconnect()
{
    if (!m_autoReconnectEnabled || m_reconnectAttempts >= m_maxReconnectAttempts) {
        setState(Error);
        setStatusMessage("Connection lost. Max reconnect attempts reached.");
        emit reconnectFailed();
        reportConnectionFailure("Max reconnect attempts reached");
        return;
    }

    setState(Reconnecting);
    m_reconnectAttempts++;

    int delay = calculateBackoffDelay();

    setStatusMessage(QString("Reconnecting in %1s... (attempt %2/%3)")
                             .arg(delay / 1000)
                             .arg(m_reconnectAttempts)
                             .arg(m_maxReconnectAttempts));

    emit reconnectAttempt(m_reconnectAttempts, m_maxReconnectAttempts);

    m_reconnectTimer->start(delay);
}

void ConnectionManagerV2::attemptReconnect()
{
    if (m_state != Reconnecting) {
        return;
    }

    qDebug() << "[ConnectionManagerV2] Attempting reconnect" << m_reconnectAttempts << "/" << m_maxReconnectAttempts;

    // Try to reconnect using last known config
    if (!m_currentConfig.isEmpty()) {
        setState(Connecting);
        setStatusMessage("Reconnecting...");
        m_xrayHandler->start(m_currentConfig);
    } else {
        // No cached config, start fresh
        setState(DetectingISP);
        setStatusMessage("Reconnecting (fresh)...");
        m_ispDetector->detectISP();
    }
}

void ConnectionManagerV2::resetReconnectState()
{
    m_reconnectAttempts = 0;
    m_reconnectTimer->stop();
}

int ConnectionManagerV2::calculateBackoffDelay() const
{
    // Exponential backoff: 1s, 2s, 4s, 8s, 16s
    int delay = INITIAL_BACKOFF_MS * qPow(2, m_reconnectAttempts - 1);
    return qMin(delay, MAX_BACKOFF_MS);
}

// ============== Metrics ==============

void ConnectionManagerV2::reportConnectionSuccess()
{
    QJsonObject data;
    data["serverId"] = m_selectedServerId;
    data["latency"] = m_currentLatency;
    data["isp"] = m_currentISP;

    m_backendAPI->reportMetric("connection_success", data);
}

void ConnectionManagerV2::reportConnectionFailure(const QString &reason)
{
    QJsonObject data;
    data["serverId"] = m_selectedServerId;
    data["reason"] = reason;
    data["isp"] = m_currentISP;

    m_backendAPI->reportMetric("connection_failure", data);
}
