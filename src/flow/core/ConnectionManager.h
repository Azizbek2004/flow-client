#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include "../measurement/ISPDetector.h"
#include "../protocols/XRayHandler.h"
#include "BackendAPI.h"

class ConnectionManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConnectionState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum ConnectionState {
        Disconnected,
        DetectingISP,
        FetchingServers,
        SelectingServer,
        FetchingConfig,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };
    Q_ENUM(ConnectionState)

    explicit ConnectionManager(QObject *parent = nullptr);
    static ConnectionManager *instance();

    ConnectionState state() const;
    QString statusMessage() const;

    Q_INVOKABLE void connectToFlow();
    Q_INVOKABLE void disconnectFromFlow();

signals:
    void stateChanged();
    void statusMessageChanged();

private slots:
    void onISPDected(const QString &isp, const QString &asn);
    void onServersFetched(const QJsonArray &servers);
    void onConfigFetched(const QJsonObject &config);
    void onXRayStarted();
    void onXRayStopped();
    void onError(const QString &error);

private:
    void setState(ConnectionState state);
    void setStatusMessage(const QString &message);
    void processNextStep();

    ConnectionState m_state;
    QString m_statusMessage;
    QString m_currentISP;
    QJsonArray m_availableServers;
    QJsonObject m_currentConfig;

    // Dependencies
    ISPDetector *m_ispDetector;
    BackendAPI *m_backendAPI;
    XRayHandler *m_xrayHandler;
};

#endif // CONNECTIONMANAGER_H
