#include "ProtocolGuard.h"

static ProtocolGuard *s_instance = nullptr;

ProtocolGuard::ProtocolGuard(QObject *parent) : QObject(parent), m_isLocked(false), m_isMatchActive(false)
{
    s_instance = this;
}

ProtocolGuard *ProtocolGuard::instance()
{
    if (!s_instance) {
        s_instance = new ProtocolGuard();
    }
    return s_instance;
}

bool ProtocolGuard::isLocked() const
{
    return m_isLocked;
}

void ProtocolGuard::setLocked(bool locked)
{
    if (m_isLocked != locked) {
        m_isLocked = locked;
        emit lockedChanged();
    }
}

bool ProtocolGuard::isMatchActive() const
{
    return m_isMatchActive;
}

void ProtocolGuard::setMatchActive(bool active)
{
    if (m_isMatchActive != active) {
        m_isMatchActive = active;
        emit matchActiveChanged();
    }
}

bool ProtocolGuard::canSwitchProtocol() const
{
    return !m_isLocked;
}
