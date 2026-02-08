#ifndef XRAYHANDLER_H
#define XRAYHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryFile>

class XRayHandler : public QObject
{
    Q_OBJECT
public:
    explicit XRayHandler(QObject *parent = nullptr);
    ~XRayHandler();

    bool start(const QJsonObject &config);
    void stop();
    bool isRunning() const;

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &error);

private:
    QString generateConfig(const QJsonObject &config);
    
    QProcess *m_process;
    QTemporaryFile *m_configFile;
};

#endif // XRAYHANDLER_H
