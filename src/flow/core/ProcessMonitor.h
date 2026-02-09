#ifndef PROCESSMONITOR_H
#define PROCESSMONITOR_H

#include <QObject>
#include <QSet>
#include <QTimer>

/**
 * @brief Monitors system processes to detect CS2/game launches
 *
 * Platform-specific implementation to detect when CS2 starts or exits,
 * enabling automatic protocol locking during matches.
 */
class ProcessMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool cs2Running READ isCS2Running NOTIFY cs2StateChanged)
    Q_PROPERTY(bool monitoring READ isMonitoring NOTIFY monitoringChanged)

public:
    explicit ProcessMonitor(QObject *parent = nullptr);
    ~ProcessMonitor();

    static ProcessMonitor *instance();

    /**
     * @brief Start monitoring for game processes
     * @param intervalMs Polling interval in milliseconds (default 2000)
     */
    void startMonitoring(int intervalMs = 2000);

    /**
     * @brief Stop monitoring
     */
    void stopMonitoring();

    /**
     * @brief Check if currently monitoring
     */
    bool isMonitoring() const;

    /**
     * @brief Check if CS2 is currently running
     */
    bool isCS2Running() const;

    /**
     * @brief Get list of monitored process names
     */
    QStringList monitoredProcesses() const;

    /**
     * @brief Add a process name to monitor
     */
    void addMonitoredProcess(const QString &processName);

    /**
     * @brief Remove a process name from monitoring
     */
    void removeMonitoredProcess(const QString &processName);

signals:
    void monitoringChanged();
    void cs2StateChanged();
    void cs2Launched();
    void cs2Exited();
    void gameProcessDetected(const QString &processName);
    void gameProcessExited(const QString &processName);

private slots:
    void pollProcesses();

private:
    bool checkProcessRunning(const QString &processName);
    QSet<QString> getRunningProcesses();

    // Platform-specific implementations
#ifdef Q_OS_WIN
    QSet<QString> getRunningProcessesWindows();
#elif defined(Q_OS_MAC)
    QSet<QString> getRunningProcessesMacOS();
#else
    QSet<QString> getRunningProcessesLinux();
#endif

    static ProcessMonitor *s_instance;

    QTimer *m_timer;
    QSet<QString> m_monitoredProcesses;
    QSet<QString> m_runningMonitoredProcesses;
    bool m_cs2Running;

    // Default CS2 process names across platforms
    static const QStringList CS2_PROCESS_NAMES;
};

#endif // PROCESSMONITOR_H
