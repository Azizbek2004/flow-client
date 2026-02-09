#include "ProcessMonitor.h"
#include <QDebug>
#include <QProcess>

#ifdef Q_OS_WIN
    #include <tlhelp32.h>
    #include <windows.h>
#elif defined(Q_OS_MAC)
    #include <libproc.h>
    #include <sys/sysctl.h>
#endif

ProcessMonitor *ProcessMonitor::s_instance = nullptr;

const QStringList ProcessMonitor::CS2_PROCESS_NAMES = { "cs2", "cs2.exe", "csgo_linux64", "csgo.exe", "Counter-Strike 2" };

ProcessMonitor::ProcessMonitor(QObject *parent) : QObject(parent), m_timer(new QTimer(this)), m_cs2Running(false)
{
    // Initialize with CS2 process names
    for (const QString &name : CS2_PROCESS_NAMES) {
        m_monitoredProcesses.insert(name.toLower());
    }

    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::pollProcesses);
    s_instance = this;
}

ProcessMonitor::~ProcessMonitor()
{
    stopMonitoring();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ProcessMonitor *ProcessMonitor::instance()
{
    if (!s_instance) {
        s_instance = new ProcessMonitor();
    }
    return s_instance;
}

void ProcessMonitor::startMonitoring(int intervalMs)
{
    if (m_timer->isActive()) {
        return;
    }

    qDebug() << "[ProcessMonitor] Starting monitoring with interval:" << intervalMs << "ms";
    qDebug() << "[ProcessMonitor] Watching for processes:" << m_monitoredProcesses;

    // Initial check
    pollProcesses();

    m_timer->start(intervalMs);
    emit monitoringChanged();
}

void ProcessMonitor::stopMonitoring()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "[ProcessMonitor] Stopped monitoring";
        emit monitoringChanged();
    }
}

bool ProcessMonitor::isMonitoring() const
{
    return m_timer->isActive();
}

bool ProcessMonitor::isCS2Running() const
{
    return m_cs2Running;
}

QStringList ProcessMonitor::monitoredProcesses() const
{
    return m_monitoredProcesses.values();
}

void ProcessMonitor::addMonitoredProcess(const QString &processName)
{
    m_monitoredProcesses.insert(processName.toLower());
}

void ProcessMonitor::removeMonitoredProcess(const QString &processName)
{
    m_monitoredProcesses.remove(processName.toLower());
}

void ProcessMonitor::pollProcesses()
{
    QSet<QString> runningProcesses = getRunningProcesses();
    QSet<QString> newRunning;

    // Check which monitored processes are running
    for (const QString &monitored : m_monitoredProcesses) {
        for (const QString &running : runningProcesses) {
            if (running.toLower().contains(monitored)) {
                newRunning.insert(monitored);
                break;
            }
        }
    }

    // Detect newly launched processes
    for (const QString &proc : newRunning) {
        if (!m_runningMonitoredProcesses.contains(proc)) {
            qDebug() << "[ProcessMonitor] Game process launched:" << proc;
            emit gameProcessDetected(proc);
        }
    }

    // Detect exited processes
    for (const QString &proc : m_runningMonitoredProcesses) {
        if (!newRunning.contains(proc)) {
            qDebug() << "[ProcessMonitor] Game process exited:" << proc;
            emit gameProcessExited(proc);
        }
    }

    m_runningMonitoredProcesses = newRunning;

    // Check CS2 state
    bool wasCS2Running = m_cs2Running;
    m_cs2Running = !newRunning.isEmpty();

    if (m_cs2Running && !wasCS2Running) {
        qDebug() << "[ProcessMonitor] CS2 launched - locking protocol";
        emit cs2Launched();
        emit cs2StateChanged();
    } else if (!m_cs2Running && wasCS2Running) {
        qDebug() << "[ProcessMonitor] CS2 exited - unlocking protocol";
        emit cs2Exited();
        emit cs2StateChanged();
    }
}

bool ProcessMonitor::checkProcessRunning(const QString &processName)
{
    QSet<QString> running = getRunningProcesses();
    for (const QString &proc : running) {
        if (proc.toLower().contains(processName.toLower())) {
            return true;
        }
    }
    return false;
}

QSet<QString> ProcessMonitor::getRunningProcesses()
{
#ifdef Q_OS_WIN
    return getRunningProcessesWindows();
#elif defined(Q_OS_MAC)
    return getRunningProcessesMacOS();
#else
    return getRunningProcessesLinux();
#endif
}

#ifdef Q_OS_WIN
QSet<QString> ProcessMonitor::getRunningProcessesWindows()
{
    QSet<QString> processes;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            QString name = QString::fromWCharArray(pe32.szExeFile);
            processes.insert(name.toLower());
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processes;
}
#endif

#ifdef Q_OS_MAC
QSet<QString> ProcessMonitor::getRunningProcessesMacOS()
{
    QSet<QString> processes;

    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    size_t size;

    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) {
        return processes;
    }

    struct kinfo_proc *procList = (struct kinfo_proc *)malloc(size);
    if (!procList) {
        return processes;
    }

    if (sysctl(mib, 4, procList, &size, NULL, 0) < 0) {
        free(procList);
        return processes;
    }

    int procCount = size / sizeof(struct kinfo_proc);

    for (int i = 0; i < procCount; ++i) {
        char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];
        pid_t pid = procList[i].kp_proc.p_pid;

        if (proc_pidpath(pid, pathBuffer, sizeof(pathBuffer)) > 0) {
            QString path = QString::fromUtf8(pathBuffer);
            QString name = path.section('/', -1);
            processes.insert(name.toLower());
        } else {
            // Fallback to process name from kinfo_proc
            QString name = QString::fromUtf8(procList[i].kp_proc.p_comm);
            processes.insert(name.toLower());
        }
    }

    free(procList);
    return processes;
}
#endif

#ifndef Q_OS_WIN
    #ifndef Q_OS_MAC
QSet<QString> ProcessMonitor::getRunningProcessesLinux()
{
    QSet<QString> processes;

    QProcess proc;
    proc.start("ps", QStringList() << "-e" << "-o" << "comm=");
    proc.waitForFinished(5000);

    QString output = proc.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        processes.insert(line.trimmed().toLower());
    }

    return processes;
}
    #endif
#endif
