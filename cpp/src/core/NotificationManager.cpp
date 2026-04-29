#include "NotificationManager.h"
#include "utils/Constants.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDateTime>
#include <QLoggingCategory>
#include <QVariantMap>

Q_LOGGING_CATEGORY(lcNotificationManager, "nyx.core.notificationmanager")

namespace nyx::core {

NotificationManager::NotificationManager()
    : m_kdeAvailable(checkKdeNotifications())
{
}

bool NotificationManager::checkKdeNotifications()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcNotificationManager) << "D-Bus session bus not connected";
        return false;
    }

    QDBusInterface interface(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        bus);

    if (!interface.isValid()) {
        qCWarning(lcNotificationManager) << "Freedesktop Notifications interface not available";
        return false;
    }

    qCInfo(lcNotificationManager) << "Freedesktop Notifications available via D-Bus";
    return true;
}

void NotificationManager::notifyServiceStarted(const QString &serviceName, const QString &displayName)
{
    if (shouldNotify(serviceName)) {
        sendNotification(
            QStringLiteral("%1 Started").arg(displayName),
            QStringLiteral("Service %1 has been started successfully.").arg(displayName),
            QStringLiteral("normal"));
        updateNotificationTime(serviceName);
    }
}

void NotificationManager::notifyServiceStopped(const QString &serviceName, const QString &displayName)
{
    if (shouldNotify(serviceName)) {
        sendNotification(
            QStringLiteral("%1 Stopped").arg(displayName),
            QStringLiteral("Service %1 has been stopped.").arg(displayName),
            QStringLiteral("normal"));
        updateNotificationTime(serviceName);
    }
}

void NotificationManager::notifyServiceFailed(const QString &serviceName, const QString &displayName,
                                               const QString &error)
{
    if (shouldNotify(serviceName)) {
        QString message = QStringLiteral("Service %1 has failed.").arg(displayName);
        if (!error.isEmpty()) {
            message += QLatin1Char('\n') + error;
        }

        sendNotification(
            QStringLiteral("%1 Failed").arg(displayName),
            message,
            QStringLiteral("critical"));
        updateNotificationTime(serviceName);
    }
}

void NotificationManager::notifyError(const QString &title, const QString &message)
{
    sendNotification(title, message, QStringLiteral("critical"));
}

void NotificationManager::notifyInfo(const QString &title, const QString &message)
{
    sendNotification(title, message, QStringLiteral("normal"));
}

void NotificationManager::sendNotification(const QString &title, const QString &message,
                                            const QString &urgency)
{
    if (m_kdeAvailable) {
        sendKdeNotification(title, message, urgency);
    } else {
        sendQtNotification(title, message);
    }
}

void NotificationManager::sendKdeNotification(const QString &title, const QString &message,
                                               const QString &urgency)
{
    auto bus = QDBusConnection::sessionBus();
    QDBusInterface interface(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        bus);

    if (!interface.isValid()) {
        qCWarning(lcNotificationManager) << "D-Bus interface invalid, falling back to Qt notifications";
        sendQtNotification(title, message);
        return;
    }

    int urgencyLevel = 1; // normal
    if (urgency == QLatin1String("low"))       urgencyLevel = 0;
    else if (urgency == QLatin1String("critical")) urgencyLevel = 2;

    QVariantMap hints;
    hints[QStringLiteral("urgency")] = urgencyLevel;

    QDBusMessage reply = interface.call(
        QStringLiteral("Notify"),
        nyx::utils::APP_NAME,                        // app_name
        static_cast<uint>(0),                         // replaces_id
        nyx::utils::APP_ICON,                         // app_icon
        title,                                        // summary
        message,                                      // body
        QStringList(),                                // actions
        hints,                                        // hints
        nyx::utils::NOTIFICATION_TIMEOUT              // timeout
    );

    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(lcNotificationManager) << "D-Bus notification error:" << reply.errorMessage();
        sendQtNotification(title, message);
    }
}

void NotificationManager::sendQtNotification(const QString &title, const QString &message)
{
    // Fallback: log the notification. The actual tray icon showMessage will be
    // connected when the UI layer is implemented.
    qCInfo(lcNotificationManager) << "Qt Notification:" << title << "-" << message;
}

bool NotificationManager::shouldNotify(const QString &serviceName) const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastTime = m_lastNotification.value(serviceName, 0);

    if (now - lastTime < static_cast<qint64>(nyx::utils::NOTIFICATION_RATE_LIMIT) * 1000) {
        qCDebug(lcNotificationManager) << "Rate limiting notification for" << serviceName;
        return false;
    }

    return true;
}

void NotificationManager::updateNotificationTime(const QString &serviceName)
{
    m_lastNotification[serviceName] = QDateTime::currentMSecsSinceEpoch();
}

} // namespace nyx::core
