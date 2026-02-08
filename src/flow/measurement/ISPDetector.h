#ifndef ISPDETECTOR_H
#define ISPDETECTOR_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

class ISPDetector : public QObject
{
    Q_OBJECT
public:
    explicit ISPDetector(QObject *parent = nullptr);
    static ISPDetector *instance();

    Q_INVOKABLE void measureLatency(const QString &host);
    Q_INVOKABLE void detectISP();
    Q_INVOKABLE QString recommendProtocol(const QString &isp);

signals:
    void latencyMeasured(const QString &host, int ms);
    void ispDetected(const QString &isp, const QString &asn);
    void errorOccurred(const QString &error);

private:
    QNetworkAccessManager *m_nam;
};

#endif // ISPDETECTOR_H
