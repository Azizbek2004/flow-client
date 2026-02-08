#include "ConnectionManager.h"
#include <QDebug>

static ConnectionManager *s_instance = nullptr;

ConnectionManager::ConnectionManager(QObject *parent) : QObject(parent), m_state(Disconnected)
{
    m_ispDetector = ISPDetector::instance();
    m_backendAPI = BackendAPI::instance();
    m_xrayHandler = new XRayHandler(this);

    connect(m_ispDetector, &ISPDetector::ispDetected, this, &ConnectionManager::onISPDected);
    connect(m_ispDetector, &ISPDetector::errorOccurred, this, &ConnectionManager::onError);

    connect(m_backendAPI, &BackendAPI::serversFetched, this, &ConnectionManager::onServersFetched);
    connect(m_backendAPI, &BackendAPI::serversFetchFailed, this, &ConnectionManager::onError);
    connect(m_backendAPI, &BackendAPI::configFetched, this, &ConnectionManager::onConfigFetched);
    connect(m_backendAPI, &BackendAPI::configFetchFailed, this, &ConnectionManager::onError);

    connect(m_xrayHandler, &XRayHandler::started, this, &ConnectionManager::onXRayStarted);
    connect(m_xrayHandler, &XRayHandler::stopped, this, &ConnectionManager::onXRayStopped);
    connect(m_xrayHandler, &XRayHandler::errorOccurred, this, &ConnectionManager::onError);

    s_instance = this;
}

ConnectionManager *ConnectionManager::instance()
{
    if (!s_instance) {
        s_instance = new ConnectionManager();
    }
    return s_instance;
}

ConnectionManager::ConnectionState ConnectionManager::state() const
{
    return m_state;
}

QString ConnectionManager::statusMessage() const
{
    return m_statusMessage;
}

void ConnectionManager::setState(ConnectionState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged();
    }
}

void ConnectionManager::setStatusMessage(const QString &message)
{
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
    }
}

void ConnectionManager::connectToFlow()
{
    if (m_state != Disconnected && m_state != Error) {
        return;
    }

    setState(DetectingISP);
    setStatusMessage("Detecting ISP...");
    m_ispDetector->detectISP();
}

void ConnectionManager::disconnectFromFlow()
{
    if (m_state == Disconnected) {
        return;
    }

    setState(Disconnecting);
    setStatusMessage("Disconnecting...");
    m_xrayHandler->stop();
}

void ConnectionManager::onISPDected(const QString &isp, const QString &asn)
{
    if (m_state != DetectingISP)
        return;

    m_currentISP = isp;
    setStatusMessage(QString("Detected ISP: %1. Fetching servers...").arg(isp));
    setState(FetchingServers);
    m_backendAPI->fetchServers(isp);
}

void ConnectionManager::onServersFetched(const QJsonArray &servers)
{
    if (m_state != FetchingServers)
        return;

    if (servers.isEmpty()) {
        onError("No servers available for your region.");
        return;
    }

    m_availableServers = servers;
    setState(SelectingServer);
    setStatusMessage("Selecting best server...");

    // Simple logic: pick the first one for now.
    // Ideally we'd ping them or use the latency estimate from backend.

    QJsonObject selectedServer = servers.first().toObject();
    QString serverId = selectedServer["id"].toString();

    setStatusMessage(QString("Selected server: %1. Fetching config...").arg(selectedServer["location"].toString()));
    setState(FetchingConfig);
    m_backendAPI->fetchConfig(serverId);
}

void ConnectionManager::onConfigFetched(const QJsonObject &config)
{
    if (m_state != FetchingConfig)
        return;

    m_currentConfig = config;
    setState(Connecting);
    setStatusMessage("Starting connection...");
    m_xrayHandler->start(config);
}

void ConnectionManager::onXRayStarted()
{
    setState(Connected);
    setStatusMessage("Connected to FLOW network.");
}

void ConnectionManager::onXRayStopped()
{
    setState(Disconnected);
    setStatusMessage("Disconnected.");
    m_currentConfig = QJsonObject();
    m_availableServers = QJsonArray();
}

void ConnectionManager::onError(const QString &error)
{
    setState(Error);
    setStatusMessage(QString("Error: %1").arg(error));
    // Depending on severity, we might want to stop XRay if it was starting
    if (m_xrayHandler->isRunning()) {
        m_xrayHandler->stop();
    }
}
