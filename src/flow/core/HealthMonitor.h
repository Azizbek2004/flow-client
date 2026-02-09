#ifndef HEALTHMONITOR_H
#define HEALTHMONITOR_H

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVector>

/**
 * @brief Network quality metrics structure
 */
struct NetworkMetrics
{
    double minLatency = 0.0;
    double maxLatency = 0.0;
    double avgLatency = 0.0;
    double jitter = 0.0;     // Standard deviation of latencies
    double packetLoss = 0.0; // Percentage 0-100
    QString targetHost;
    QDateTime timestamp;

    bool isHealthy() const
    {
        return packetLoss < 5.0 && jitter < 20.0 && avgLatency < 150.0;
    }

    QString healthStatus() const
    {
        if (packetLoss >= 10.0)
            return "Critical";
        if (packetLoss >= 5.0 || jitter >= 30.0)
            return "Degraded";
        if (jitter >= 20.0 || avgLatency >= 100.0)
            return "Fair";
        return "Good";
    }
};

/**
 * @brief Monitors connection health continuously while connected
 *
 * Sends periodic pings to measure latency, jitter, and packet loss.
 * Emits signals when connection quality degrades or recovers.
 */
class HealthMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool monitoring READ isMonitoring NOTIFY monitoringChanged)
    Q_PROPERTY(bool healthy READ isConnectionHealthy NOTIFY healthChanged)
    Q_PROPERTY(QString status READ statusText NOTIFY metricsUpdated)

public:
    explicit HealthMonitor(QObject *parent = nullptr);
    ~HealthMonitor();

    static HealthMonitor *instance();

    /**
     * @brief Start continuous health monitoring
     * @param host Target host to ping (usually the VPN server)
     * @param intervalMs Interval between checks in milliseconds (default 5000)
     * @param pingCount Number of pings per check (default 5)
     */
    void startMonitoring(const QString &host, int intervalMs = 5000, int pingCount = 5);

    /**
     * @brief Stop health monitoring
     */
    void stopMonitoring();

    /**
     * @brief Check if currently monitoring
     */
    bool isMonitoring() const;

    /**
     * @brief Get the most recent metrics
     */
    NetworkMetrics currentMetrics() const;

    /**
     * @brief Check if connection is currently healthy
     */
    bool isConnectionHealthy() const;

    /**
     * @brief Get human-readable status text
     */
    QString statusText() const;

    /**
     * @brief Get historical metrics (last N measurements)
     */
    QVector<NetworkMetrics> metricsHistory(int count = 10) const;

signals:
    void monitoringChanged();
    void healthChanged();
    void metricsUpdated(const NetworkMetrics &metrics);
    void connectionDegraded(const QString &reason);
    void connectionRecovered();
    void pingCompleted(int latencyMs);

private slots:
    void performHealthCheck();
    void onPingFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void calculateMetrics();
    int parsePingOutput(const QString &output);

    static HealthMonitor *s_instance;

    QTimer *m_timer;
    QString m_targetHost;
    int m_pingCount;
    int m_intervalMs;

    // Current check state
    QVector<int> m_currentPings;
    int m_pendingPings;
    int m_failedPings;

    // Metrics
    NetworkMetrics m_currentMetrics;
    QVector<NetworkMetrics> m_history;
    static const int MAX_HISTORY_SIZE = 100;

    // Degradation tracking
    int m_degradedCount;
    bool m_wasHealthy;
    static const int DEGRADATION_THRESHOLD = 3;
};

#endif // HEALTHMONITOR_H
