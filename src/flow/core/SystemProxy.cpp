#include "SystemProxy.h"
#include <QDebug>
#include <QProcess>
#include <QSettings>

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <wininet.h>
    #pragma comment(lib, "wininet.lib")
#endif

SystemProxy *SystemProxy::s_instance = nullptr;

SystemProxy::SystemProxy(QObject *parent)
    : QObject(parent),
      m_enabled(false),
      m_host("127.0.0.1"),
      m_port(10808),
      m_bypassLocal(true),
      m_hadOriginalProxy(false),
      m_originalPort(0)
{
    // Default bypass list
    m_bypassList << "localhost" << "127.0.0.1" << "::1" << "*.local";
    s_instance = this;
}

SystemProxy::~SystemProxy()
{
    if (m_enabled) {
        disableProxy();
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

SystemProxy *SystemProxy::instance()
{
    if (!s_instance) {
        s_instance = new SystemProxy();
    }
    return s_instance;
}

bool SystemProxy::enableProxy(const QString &host, int port)
{
    if (m_enabled && m_host == host && m_port == port) {
        return true; // Already enabled with same settings
    }

    m_host = host;
    m_port = port;

    saveOriginalSettings();

    qDebug() << "[SystemProxy] Enabling SOCKS5 proxy:" << host << ":" << port;

    bool success = enableProxyPlatform();

    if (success) {
        m_enabled = true;
        emit proxyStateChanged(true);
        qDebug() << "[SystemProxy] Proxy enabled successfully";
    } else {
        emit proxyError("Failed to enable system proxy");
    }

    return success;
}

bool SystemProxy::disableProxy()
{
    if (!m_enabled) {
        return true;
    }

    qDebug() << "[SystemProxy] Disabling proxy";

    bool success = disableProxyPlatform();

    if (success) {
        m_enabled = false;
        emit proxyStateChanged(false);
        qDebug() << "[SystemProxy] Proxy disabled successfully";
    } else {
        emit proxyError("Failed to disable system proxy");
    }

    return success;
}

bool SystemProxy::isProxyEnabled() const
{
    return m_enabled;
}

QString SystemProxy::proxyHost() const
{
    return m_host;
}

int SystemProxy::proxyPort() const
{
    return m_port;
}

void SystemProxy::setBypassLocal(bool bypass)
{
    m_bypassLocal = bypass;
}

void SystemProxy::addBypassAddress(const QString &address)
{
    if (!m_bypassList.contains(address)) {
        m_bypassList.append(address);
    }
}

void SystemProxy::clearBypassList()
{
    m_bypassList.clear();
}

bool SystemProxy::enableProxyPlatform()
{
#ifdef Q_OS_WIN
    return enableProxyWindows();
#elif defined(Q_OS_MAC)
    return enableProxyMacOS();
#else
    return enableProxyLinux();
#endif
}

bool SystemProxy::disableProxyPlatform()
{
#ifdef Q_OS_WIN
    return disableProxyWindows();
#elif defined(Q_OS_MAC)
    return disableProxyMacOS();
#else
    return disableProxyLinux();
#endif
}

void SystemProxy::saveOriginalSettings()
{
    // TODO: Implement saving original proxy settings for restoration
    m_hadOriginalProxy = false;
}

void SystemProxy::restoreOriginalSettings()
{
    // TODO: Implement restoring original proxy settings
}

#ifdef Q_OS_WIN
bool SystemProxy::enableProxyWindows()
{
    // Set SOCKS proxy via registry
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                       QSettings::NativeFormat);

    QString proxyServer = QString("socks=%1:%2").arg(m_host).arg(m_port);
    settings.setValue("ProxyServer", proxyServer);
    settings.setValue("ProxyEnable", 1);

    if (m_bypassLocal) {
        settings.setValue("ProxyOverride", m_bypassList.join(";") + ";<local>");
    }

    settings.sync();

    // Notify system of settings change
    INTERNET_OPTION_SETTINGS_CHANGED;
    InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    return true;
}

bool SystemProxy::disableProxyWindows()
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                       QSettings::NativeFormat);

    settings.setValue("ProxyEnable", 0);
    settings.sync();

    InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    return true;
}
#endif

#ifdef Q_OS_MAC
QStringList SystemProxy::getNetworkServices()
{
    QStringList services;
    QProcess proc;
    proc.start("networksetup", QStringList() << "-listallnetworkservices");
    proc.waitForFinished(5000);

    QString output = proc.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        // Skip the header line
        if (!line.startsWith("*") && !line.contains("An asterisk")) {
            services << line.trimmed();
        }
    }

    return services;
}

bool SystemProxy::enableProxyMacOS()
{
    QStringList services = getNetworkServices();
    bool success = true;

    for (const QString &service : services) {
        QProcess proc;

        // Enable SOCKS proxy
        proc.start("networksetup",
                   QStringList() << "-setsocksfirewallproxy" << service << m_host << QString::number(m_port));
        proc.waitForFinished(5000);

        if (proc.exitCode() != 0) {
            qWarning() << "[SystemProxy] Failed to set SOCKS proxy for" << service;
            continue;
        }

        // Turn on SOCKS proxy
        proc.start("networksetup", QStringList() << "-setsocksfirewallproxystate" << service << "on");
        proc.waitForFinished(5000);

        if (proc.exitCode() != 0) {
            qWarning() << "[SystemProxy] Failed to enable SOCKS proxy for" << service;
            success = false;
        } else {
            qDebug() << "[SystemProxy] Enabled SOCKS proxy for" << service;
        }

        // Set bypass domains
        if (!m_bypassList.isEmpty()) {
            QStringList args;
            args << "-setproxybypassdomains" << service;
            args << m_bypassList;
            proc.start("networksetup", args);
            proc.waitForFinished(5000);
        }
    }

    return success;
}

bool SystemProxy::disableProxyMacOS()
{
    QStringList services = getNetworkServices();
    bool success = true;

    for (const QString &service : services) {
        QProcess proc;
        proc.start("networksetup", QStringList() << "-setsocksfirewallproxystate" << service << "off");
        proc.waitForFinished(5000);

        if (proc.exitCode() != 0) {
            qWarning() << "[SystemProxy] Failed to disable SOCKS proxy for" << service;
            success = false;
        } else {
            qDebug() << "[SystemProxy] Disabled SOCKS proxy for" << service;
        }
    }

    return success;
}
#endif

#ifndef Q_OS_WIN
    #ifndef Q_OS_MAC
bool SystemProxy::enableProxyLinux()
{
    // For Linux, we set environment variables and try GNOME/KDE settings
    bool success = false;

    // Try gsettings for GNOME
    QProcess proc;
    proc.start("gsettings", QStringList() << "set" << "org.gnome.system.proxy" << "mode" << "'manual'");
    if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
        proc.start("gsettings", QStringList() << "set" << "org.gnome.system.proxy.socks" << "host" << m_host);
        proc.waitForFinished(5000);

        proc.start("gsettings",
                   QStringList() << "set" << "org.gnome.system.proxy.socks" << "port" << QString::number(m_port));
        proc.waitForFinished(5000);

        success = true;
        qDebug() << "[SystemProxy] Set GNOME proxy settings";
    }

    // Also export environment variables
    qputenv("ALL_PROXY", QString("socks5://%1:%2").arg(m_host).arg(m_port).toUtf8());
    qputenv("all_proxy", QString("socks5://%1:%2").arg(m_host).arg(m_port).toUtf8());

    return success;
}

bool SystemProxy::disableProxyLinux()
{
    QProcess proc;
    proc.start("gsettings", QStringList() << "set" << "org.gnome.system.proxy" << "mode" << "'none'");
    proc.waitForFinished(5000);

    // Clear environment variables
    qunsetenv("ALL_PROXY");
    qunsetenv("all_proxy");

    return true;
}
    #endif
#endif
