#include "HealthMonitor.h"
#include <QDebug>
#include <QRegularExpression>
#include <cmath>

HealthMonitor *HealthMonitor::s_instance = nullptr;

HealthMonitor::HealthMonitor(QObject *parent)
    : QObject(parent),
      m_timer(new QTimer(this)),
      m_pingCount(5),
      m_intervalMs(5000),
      m_pendingPings(0),
      m_failedPings(0),
      m_degradedCount(0),
      m_wasHealthy(true)
{
    connect(m_timer, &QTimer::timeout, this, &HealthMonitor::performHealthCheck);
    s_instance = this;
}

HealthMonitor::~HealthMonitor()
{
    stopMonitoring();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

HealthMonitor *HealthMonitor::instance()
{
    if (!s_instance) {
        s_instance = new HealthMonitor();
    }
    return s_instance;
}

void HealthMonitor::startMonitoring(const QString &host, int intervalMs, int pingCount)
{
    stopMonitoring();

    m_targetHost = host;
    m_intervalMs = intervalMs;
    m_pingCount = pingCount;
    m_degradedCount = 0;
    m_wasHealthy = true;

    qDebug() << "[HealthMonitor] Starting monitoring for" << host << "every" << intervalMs << "ms with" << pingCount
             << "pings";

    // Perform initial check immediately
    performHealthCheck();

    // Start periodic checks
    m_timer->start(m_intervalMs);
    emit monitoringChanged();
}

void HealthMonitor::stopMonitoring()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "[HealthMonitor] Stopped monitoring";
        emit monitoringChanged();
    }
}

bool HealthMonitor::isMonitoring() const
{
    return m_timer->isActive();
}

NetworkMetrics HealthMonitor::currentMetrics() const
{
    return m_currentMetrics;
}

bool HealthMonitor::isConnectionHealthy() const
{
    return m_currentMetrics.isHealthy();
}

QString HealthMonitor::statusText() const
{
    if (!isMonitoring()) {
        return "Not monitoring";
    }

    return QString("%1 | Latency: %2ms | Jitter: %3ms | Loss: %4%")
            .arg(m_currentMetrics.healthStatus())
            .arg(m_currentMetrics.avgLatency, 0, 'f', 1)
            .arg(m_currentMetrics.jitter, 0, 'f', 1)
            .arg(m_currentMetrics.packetLoss, 0, 'f', 1);
}

QVector<NetworkMetrics> HealthMonitor::metricsHistory(int count) const
{
    int start = qMax(0, m_history.size() - count);
    return m_history.mid(start);
}

void HealthMonitor::performHealthCheck()
{
    if (m_targetHost.isEmpty()) {
        return;
    }

    // Reset state for new check
    m_currentPings.clear();
    m_pendingPings = m_pingCount;
    m_failedPings = 0;

    // Launch all pings
    for (int i = 0; i < m_pingCount; ++i) {
        QProcess *ping = new QProcess(this);

        connect(ping, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                &HealthMonitor::onPingFinished);

#ifdef Q_OS_WIN
        ping->start("ping", QStringList() << "-n" << "1" << "-w" << "2000" << m_targetHost);
#else
        ping->start("ping", QStringList() << "-c" << "1" << "-W" << "2" << m_targetHost);
#endif
    }
}

void HealthMonitor::onPingFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *ping = qobject_cast<QProcess *>(sender());
    if (!ping)
        return;

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        QString output = ping->readAllStandardOutput();
        int latency = parsePingOutput(output);
        if (latency > 0) {
            m_currentPings.append(latency);
            emit pingCompleted(latency);
        } else {
            m_failedPings++;
        }
    } else {
        m_failedPings++;
    }

    ping->deleteLater();
    m_pendingPings--;

    // All pings completed
    if (m_pendingPings == 0) {
        calculateMetrics();
    }
}

int HealthMonitor::parsePingOutput(const QString &output)
{
    // Match patterns like "time=23.5" or "time<1ms" or "time=23 ms"
    QRegularExpression re("time[=<](\\d+(?:\\.\\d+)?)\\s*m?s?");
    QRegularExpressionMatch match = re.match(output);

    if (match.hasMatch()) {
        return static_cast<int>(match.captured(1).toDouble());
    }
    return -1;
}

void HealthMonitor::calculateMetrics()
{
    NetworkMetrics metrics;
    metrics.targetHost = m_targetHost;
    metrics.timestamp = QDateTime::currentDateTime();

    int totalPings = m_pingCount;
    int successfulPings = m_currentPings.size();

    // Calculate packet loss
    metrics.packetLoss = ((totalPings - successfulPings) / static_cast<double>(totalPings)) * 100.0;

    if (successfulPings > 0) {
        // Calculate min, max, avg
        double sum = 0;
        metrics.minLatency = m_currentPings.first();
        metrics.maxLatency = m_currentPings.first();

        for (int ping : m_currentPings) {
            sum += ping;
            metrics.minLatency = qMin(metrics.minLatency, static_cast<double>(ping));
            metrics.maxLatency = qMax(metrics.maxLatency, static_cast<double>(ping));
        }

        metrics.avgLatency = sum / successfulPings;

        // Calculate jitter (standard deviation)
        if (successfulPings > 1) {
            double variance = 0;
            for (int ping : m_currentPings) {
                variance += std::pow(ping - metrics.avgLatency, 2);
            }
            metrics.jitter = std::sqrt(variance / (successfulPings - 1));
        }
    } else {
        // All pings failed
        metrics.minLatency = 0;
        metrics.maxLatency = 0;
        metrics.avgLatency = 0;
        metrics.jitter = 0;
    }

    // Store metrics
    m_currentMetrics = metrics;
    m_history.append(metrics);

    // Trim history
    while (m_history.size() > MAX_HISTORY_SIZE) {
        m_history.removeFirst();
    }

    emit metricsUpdated(metrics);

    // Check for degradation/recovery
    bool isHealthy = metrics.isHealthy();

    if (!isHealthy) {
        m_degradedCount++;
        if (m_degradedCount >= DEGRADATION_THRESHOLD && m_wasHealthy) {
            m_wasHealthy = false;
            QString reason;
            if (metrics.packetLoss >= 10.0) {
                reason = QString("High packet loss: %1%").arg(metrics.packetLoss, 0, 'f', 1);
            } else if (metrics.jitter >= 30.0) {
                reason = QString("High jitter: %1ms").arg(metrics.jitter, 0, 'f', 1);
            } else {
                reason = QString("Connection quality degraded");
            }
            emit connectionDegraded(reason);
            emit healthChanged();
        }
    } else {
        if (!m_wasHealthy) {
            m_wasHealthy = true;
            m_degradedCount = 0;
            emit connectionRecovered();
            emit healthChanged();
        } else {
            m_degradedCount = 0;
        }
    }

    qDebug() << "[HealthMonitor]" << metrics.healthStatus() << "| Latency:" << metrics.avgLatency << "ms"
             << "| Jitter:" << metrics.jitter << "ms"
             << "| Loss:" << metrics.packetLoss << "%";
}
