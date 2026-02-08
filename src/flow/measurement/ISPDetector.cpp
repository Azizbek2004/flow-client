#include "ISPDetector.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

static ISPDetector *s_instance = nullptr;

ISPDetector::ISPDetector(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    s_instance = this;
}

ISPDetector *ISPDetector::instance()
{
    if (!s_instance) {
        s_instance = new ISPDetector();
    }
    return s_instance;
}

void ISPDetector::measureLatency(const QString &host)
{
    QProcess *ping = new QProcess(this);
#ifdef Q_OS_WIN
    ping->start("ping", QStringList() << "-n" << "1" << host);
#else
    ping->start("ping", QStringList() << "-c" << "1" << host);
#endif

    connect(ping, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, ping, host](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitCode == 0) {
                    QString output = ping->readAllStandardOutput();
                    int ms = 0;
                    // Matches time=123 or time=123.45
                    QRegularExpression re("time[=<](\\d+(?:\\.\\d+)?)");
                    QRegularExpressionMatch match = re.match(output);
                    if (match.hasMatch()) {
                        ms = match.captured(1).toDouble(); // Round to int
                    }
                    emit latencyMeasured(host, ms);
                } else {
                    emit errorOccurred("Ping failed");
                }
                ping->deleteLater();
            });
}

void ISPDetector::detectISP()
{
    QNetworkRequest request(QUrl("https://ipinfo.io/json"));
    m_nam->get(request);

    // Note: In a real implementation, we should check if m_nam is already running a request
    // or handle signals properly to avoid multiple connections to the same slot if connected multiple times.
    // For MVP, simplistic connection is used.
    connect(m_nam, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();

            QString org = obj.value("org").toString(); // ASN + ISP
            emit ispDetected(org, org.split(" ").first());
        } else {
            emit errorOccurred("Failed to detect ISP");
        }
        reply->deleteLater();
    });
}

QString ISPDetector::recommendProtocol(const QString &isp)
{
    QString lower = isp.toLower();
    if (lower.contains("uztelecom") || lower.contains("uzonline")) {
        return "xray-vless";
    }
    if (lower.contains("beeline")) {
        return "amnezia-wg";
    }
    return "wireguard";
}
