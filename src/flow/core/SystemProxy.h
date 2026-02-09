#ifndef SYSTEMPROXY_H
#define SYSTEMPROXY_H

#include <QObject>

/**
 * @brief Manages system-level proxy configuration
 *
 * Configures the operating system to route traffic through the SOCKS5 proxy
 * created by XRay. Platform-specific implementations for Windows, macOS, and Linux.
 */
class SystemProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isProxyEnabled NOTIFY proxyStateChanged)
    Q_PROPERTY(QString host READ proxyHost NOTIFY proxyStateChanged)
    Q_PROPERTY(int port READ proxyPort NOTIFY proxyStateChanged)

public:
    explicit SystemProxy(QObject *parent = nullptr);
    ~SystemProxy();

    static SystemProxy *instance();

    /**
     * @brief Enable SOCKS5 proxy at specified host:port
     * @param host Proxy host (usually "127.0.0.1")
     * @param port Proxy port (usually 10808)
     * @return true if proxy was successfully enabled
     */
    bool enableProxy(const QString &host = "127.0.0.1", int port = 10808);

    /**
     * @brief Disable system proxy and restore original settings
     * @return true if proxy was successfully disabled
     */
    bool disableProxy();

    /**
     * @brief Check if proxy is currently enabled
     */
    bool isProxyEnabled() const;

    /**
     * @brief Get current proxy host
     */
    QString proxyHost() const;

    /**
     * @brief Get current proxy port
     */
    int proxyPort() const;

    /**
     * @brief Enable bypass for local addresses
     */
    void setBypassLocal(bool bypass);

    /**
     * @brief Add address to bypass list
     */
    void addBypassAddress(const QString &address);

    /**
     * @brief Clear bypass list
     */
    void clearBypassList();

signals:
    void proxyStateChanged(bool enabled);
    void proxyError(const QString &error);

private:
    // Platform-specific implementations
    bool enableProxyPlatform();
    bool disableProxyPlatform();
    void saveOriginalSettings();
    void restoreOriginalSettings();

#ifdef Q_OS_WIN
    bool enableProxyWindows();
    bool disableProxyWindows();
#elif defined(Q_OS_MAC)
    bool enableProxyMacOS();
    bool disableProxyMacOS();
    QStringList getNetworkServices();
#else
    bool enableProxyLinux();
    bool disableProxyLinux();
#endif

    static SystemProxy *s_instance;

    bool m_enabled;
    QString m_host;
    int m_port;
    bool m_bypassLocal;
    QStringList m_bypassList;

    // Original settings backup
    bool m_hadOriginalProxy;
    QString m_originalHost;
    int m_originalPort;
};

#endif // SYSTEMPROXY_H
