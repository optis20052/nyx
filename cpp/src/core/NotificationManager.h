#pragma once

#include <QHash>
#include <QString>

namespace nyx::core {

class NotificationManager {
public:
    NotificationManager();

    void notifyServiceStarted(const QString &serviceName, const QString &displayName);
    void notifyServiceStopped(const QString &serviceName, const QString &displayName);
    void notifyServiceFailed(const QString &serviceName, const QString &displayName,
                             const QString &error = {});
    void notifyError(const QString &title, const QString &message);
    void notifyInfo(const QString &title, const QString &message);

private:
    bool checkKdeNotifications();
    void sendNotification(const QString &title, const QString &message,
                          const QString &urgency = QStringLiteral("normal"));
    void sendKdeNotification(const QString &title, const QString &message, const QString &urgency);
    void sendQtNotification(const QString &title, const QString &message);

    bool shouldNotify(const QString &serviceName) const;
    void updateNotificationTime(const QString &serviceName);

    QHash<QString, qint64> m_lastNotification;
    bool m_kdeAvailable = false;
};

} // namespace nyx::core
