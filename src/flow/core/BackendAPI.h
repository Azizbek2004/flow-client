#ifndef BACKENDAPI_H
#define BACKENDAPI_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

class BackendAPI : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backendUrl READ backendUrl WRITE setBackendUrl NOTIFY backendUrlChanged)
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY authenticatedChanged)

public:
    explicit BackendAPI(QObject *parent = nullptr);
    static BackendAPI *instance();

    QString backendUrl() const;
    void setBackendUrl(const QString &url);

    bool isAuthenticated() const;

    Q_INVOKABLE void login(const QString &email);
    Q_INVOKABLE void fetchServers(const QString &isp);
    Q_INVOKABLE void fetchConfig(const QString &serverId);

    /**
     * @brief Report a metric event to the backend
     * @param event Event name (e.g., "connection_success", "connection_failure")
     * @param data Additional event data
     */
    Q_INVOKABLE void reportMetric(const QString &event, const QJsonObject &data);

signals:
    void backendUrlChanged();
    void authenticatedChanged();

    void loginSuccess(const QString &token);
    void loginFailed(const QString &error);

    void serversFetched(const QJsonArray &servers);
    void serversFetchFailed(const QString &error);

    void configFetched(const QJsonObject &config);
    void configFetchFailed(const QString &error);

private:
    QNetworkAccessManager *m_nam;
    QString m_backendUrl;
    QString m_authToken;
};

#endif // BACKENDAPI_H
