#pragma once

#include "models/ServiceConfig.h"
#include "models/ServiceStatus.h"

#include <QAction>
#include <QSystemTrayIcon>
#include <functional>

namespace nyx::ui {

class ServiceTrayIcon : public QSystemTrayIcon {
    Q_OBJECT

public:
    using ThemeCallback = std::function<bool()>;

    explicit ServiceTrayIcon(const nyx::models::ServiceConfig &config,
                             ThemeCallback isDarkTheme = nullptr,
                             QObject *parent = nullptr);

    void updateStatus(nyx::models::ServiceStatus status);
    void updateConfig(const nyx::models::ServiceConfig &config);

    nyx::models::ServiceStatus currentStatus() const { return m_currentStatus; }
    const nyx::models::ServiceConfig &serviceConfig() const { return m_config; }

signals:
    void startRequested(const QString &name, const QString &type);
    void stopRequested(const QString &name, const QString &type);
    void restartRequested(const QString &name, const QString &type);
    void viewLogsRequested(const QString &name, const QString &type);
    void editRequested(const QString &name, const QString &type);
    void removeRequested(const QString &name, const QString &type);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onThemeChanged();

private:
    void createMenu();
    void updateIcon();
    void updateTooltip();
    void updateMenuActions();
    QIcon addStatusOverlay(const QIcon &baseIcon, const QColor &color);

    nyx::models::ServiceConfig m_config;
    nyx::models::ServiceStatus m_currentStatus = nyx::models::ServiceStatus::Unknown;
    ThemeCallback m_isDarkTheme;

    QAction *m_startAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_restartAction = nullptr;
};

} // namespace nyx::ui
