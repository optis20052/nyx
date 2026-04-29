#pragma once

#include <QString>
#include <QJsonObject>

namespace nyx::models {

struct ServiceConfig {
    QString name;
    QString displayName;
    QString icon = QStringLiteral("application-x-executable");
    QString iconLight;
    QString iconDark;
    QString serviceType = QStringLiteral("user");
    bool autoStart = false;
    bool enabled = true;

    bool validate();
    bool isUserService() const;
    QString getIconForTheme(bool isDarkTheme) const;

    QJsonObject toJson() const;
    static ServiceConfig fromJson(const QJsonObject &obj);
};

} // namespace nyx::models
