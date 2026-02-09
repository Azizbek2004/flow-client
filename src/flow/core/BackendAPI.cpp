#include "BackendAPI.h"
#include <QDateTime>
#include <QSettings>
#include <QUrlQuery>

static BackendAPI *s_instance = nullptr;

BackendAPI::BackendAPI(QObject *parent)
    : QObject(parent), m_backendUrl("https://flow-backend-flax.vercel.app/api") // Production URL
{
    m_nam = new QNetworkAccessManager(this);
    s_instance = this;
}

BackendAPI *BackendAPI::instance()
{
    if (!s_instance) {
        s_instance = new BackendAPI();
    }
    return s_instance;
}

QString BackendAPI::backendUrl() const
{
    return m_backendUrl;
}

void BackendAPI::setBackendUrl(const QString &url)
{
    if (m_backendUrl != url) {
        m_backendUrl = url;
        emit backendUrlChanged();
    }
}

bool BackendAPI::isAuthenticated() const
{
    return !m_authToken.isEmpty();
}

void BackendAPI::login(const QString &email)
{
    QUrl url(m_backendUrl + "/auth");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["email"] = email;
    QByteArray data = QJsonDocument(json).toJson();

    QNetworkReply *reply = m_nam->post(request, data);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonObject json = QJsonDocument::fromJson(responseData).object();
            m_authToken = json["token"].toString();
            emit loginSuccess(m_authToken);
            emit authenticatedChanged();
        } else {
            emit loginFailed(reply->errorString());
        }
        reply->deleteLater();
    });
}

void BackendAPI::fetchServers(const QString &isp)
{
    QUrl url(m_backendUrl + "/servers");
    QUrlQuery query;
    if (!isp.isEmpty()) {
        query.addQueryItem("isp", isp);
    }
    url.setQuery(query);

    QNetworkRequest request(url);
    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }

    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonObject json = QJsonDocument::fromJson(responseData).object();
            QJsonArray servers = json["servers"].toArray();
            emit serversFetched(servers);
        } else {
            emit serversFetchFailed(reply->errorString());
        }
        reply->deleteLater();
    });
}

void BackendAPI::fetchConfig(const QString &serverId)
{
    if (serverId.isEmpty()) {
        emit configFetchFailed("Server ID is required");
        return;
    }

    QUrl url(m_backendUrl + "/config");
    QUrlQuery query;
    query.addQueryItem("server", serverId);
    url.setQuery(query);

    QNetworkRequest request(url);
    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }

    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonObject json = QJsonDocument::fromJson(responseData).object();
            emit configFetched(json);
        } else {
            emit configFetchFailed(reply->errorString());
        }
        reply->deleteLater();
    });
}

void BackendAPI::reportMetric(const QString &event, const QJsonObject &data)
{
    QUrl url(m_backendUrl + "/metrics");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }

    QJsonObject payload;
    payload["event"] = event;
    payload["data"] = data;
    payload["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QByteArray postData = QJsonDocument(payload).toJson();

    QNetworkReply *reply = m_nam->post(request, postData);

    connect(reply, &QNetworkReply::finished, this, [reply]() {
        // Fire and forget - we don't block on metrics
        reply->deleteLater();
    });
}
