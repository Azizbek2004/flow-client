#include "XRayHandler.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

XRayHandler::XRayHandler(QObject *parent) : QObject(parent), m_process(new QProcess(this)), m_configFile(nullptr)
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::CrashExit) {
                    emit errorOccurred("XRay process crashed");
                }
                emit stopped();
            });
}

XRayHandler::~XRayHandler()
{
    stop();
}

bool XRayHandler::start(const QJsonObject &config)
{
    stop(); // Ensure stopped before starting

    QString configJson = generateConfig(config);
    if (configJson.isEmpty()) {
        emit errorOccurred("Failed to generate XRay config");
        return false;
    }

    m_configFile = new QTemporaryFile();
    if (!m_configFile->open()) {
        emit errorOccurred("Failed to create temporary config file");
        delete m_configFile;
        m_configFile = nullptr;
        return false;
    }

    m_configFile->write(configJson.toUtf8());
    m_configFile->flush();
    m_configFile->close();

    QString program = "xray"; // Assumed to be in PATH or bundled
#ifdef Q_OS_WIN
    program = QCoreApplication::applicationDirPath() + "/xray.exe";
#elif defined(Q_OS_MAC)
    program = QCoreApplication::applicationDirPath() + "/xray";
#endif

    // Fallback if not found in app dir, check system path
    if (!QFile::exists(program)) {
        program = "xray";
    }

    m_process->start(program, QStringList() << "-c" << m_configFile->fileName());

    if (!m_process->waitForStarted()) {
        emit errorOccurred("Failed to start XRay process: " + m_process->errorString());
        return false;
    }

    emit started();
    return true;
}

void XRayHandler::stop()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }

    if (m_configFile) {
        delete m_configFile;
        m_configFile = nullptr;
    }
}

bool XRayHandler::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

QString XRayHandler::generateConfig(const QJsonObject &config)
{
    // Load template
    QFile templateFile(":/config/xray-template.json");
    // In a real app we might use a resource file or look in a config dir.
    // For now assuming it's in a known location relative to app or embedded.
    // Let's assume it's next to the executable for this MVP if not in resources.

    QString templateContent;
    QString templatePath = QCoreApplication::applicationDirPath() + "/config/xray-template.json";

    QFile file(templatePath);
    if (file.open(QIODevice::ReadOnly)) {
        templateContent = file.readAll();
    } else {
        // Fallback to simpler hardcoded template if file missing
        templateContent = R"({
          "inbounds": [{
            "port": 10808,
            "protocol": "socks",
            "settings": {
              "udp": true
            }
          }],
          "outbounds": [{
            "protocol": "vless",
            "settings": {
              "vnext": [{
                "address": "{{SERVER_ADDRESS}}",
                "port": {{SERVER_PORT}},
                "users": [{
                  "id": "{{USER_ID}}",
                  "encryption": "none",
                  "flow": "xtls-rprx-vision"
                }]
              }]
            },
            "streamSettings": {
              "network": "tcp",
              "security": "reality",
              "realitySettings": {
                "serverName": "{{SNI}}",
                "publicKey": "{{PUBLIC_KEY}}",
                "shortId": "{{SHORT_ID}}"
              }
            }
          }]
        })";
    }

    // Replace placeholders
    // Config object structure expected from backend:
    // {
    //   "server": { "address": "...", "port": ... },
    //   "user": { "id": "..." },
    //   "reality": { "sni": "...", "publicKey": "...", "shortId": "..." }
    // }

    QString result = templateContent;
    result.replace("{{SERVER_ADDRESS}}", config["server"].toObject()["address"].toString());
    result.replace("{{SERVER_PORT}}", QString::number(config["server"].toObject()["port"].toInt()));
    result.replace("{{USER_ID}}", config["user"].toObject()["id"].toString());
    result.replace("{{SNI}}", config["reality"].toObject()["sni"].toString());
    result.replace("{{PUBLIC_KEY}}", config["reality"].toObject()["publicKey"].toString());
    result.replace("{{SHORT_ID}}", config["reality"].toObject()["shortId"].toString());

    return result;
}
