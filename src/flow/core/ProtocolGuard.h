#ifndef PROTOCOLGUARD_H
#define PROTOCOLGUARD_H

#include <QObject>
#include <QString>

class ProtocolGuard : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLocked READ isLocked WRITE setLocked NOTIFY lockedChanged)
    Q_PROPERTY(bool isMatchActive READ isMatchActive NOTIFY matchActiveChanged)

public:
    explicit ProtocolGuard(QObject *parent = nullptr);
    static ProtocolGuard* instance();

    bool isLocked() const;
    void setLocked(bool locked);
    
    bool isMatchActive() const;
    void setMatchActive(bool active);

    Q_INVOKABLE bool canSwitchProtocol() const;

signals:
    void lockedChanged();
    void matchActiveChanged();

private:
    bool m_isLocked;
    bool m_isMatchActive;
};

#endif // PROTOCOLGUARD_H
