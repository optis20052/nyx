#include "ServiceTrayIcon.h"
#include "utils/Constants.h"

#include <QApplication>
#include <QEvent>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMenu>
#include <QPainter>

Q_LOGGING_CATEGORY(lcServiceTrayIcon, "nyx.ui.servicetrayicon")

namespace nyx::ui {

using nyx::models::ServiceStatus;

ServiceTrayIcon::ServiceTrayIcon(const nyx::models::ServiceConfig &config,
                                 ThemeCallback isDarkTheme,
                                 QObject *parent)
    : QSystemTrayIcon(parent)
    , m_config(config)
    , m_isDarkTheme(std::move(isDarkTheme))
{
    if (m_isDarkTheme) {
        qApp->installEventFilter(this);
    }

    updateIcon();
    createMenu();
    updateTooltip();
    show();

    qCDebug(lcServiceTrayIcon) << "Created tray icon for" << config.displayName;
}

void ServiceTrayIcon::updateStatus(ServiceStatus status)
{
    if (m_currentStatus != status) {
        qCDebug(lcServiceTrayIcon) << m_config.displayName << "status changed:"
                                   << nyx::models::serviceStatusToString(m_currentStatus)
                                   << "->" << nyx::models::serviceStatusToString(status);
        m_currentStatus = status;
        updateIcon();
        updateTooltip();
        updateMenuActions();
    }
}

void ServiceTrayIcon::updateConfig(const nyx::models::ServiceConfig &config)
{
    qCDebug(lcServiceTrayIcon) << "Updating config for" << m_config.displayName;
    m_config = config;
    updateIcon();
    updateTooltip();
    createMenu();
}

bool ServiceTrayIcon::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        onThemeChanged();
    }
    return QSystemTrayIcon::eventFilter(obj, event);
}

void ServiceTrayIcon::onThemeChanged()
{
    qCDebug(lcServiceTrayIcon) << "Theme changed for" << m_config.displayName;
    updateIcon();
}

void ServiceTrayIcon::createMenu()
{
    auto *menu = new QMenu();

    // Service name header (disabled)
    auto *headerAction = menu->addAction(m_config.displayName);
    headerAction->setEnabled(false);

    menu->addSeparator();

    m_startAction = menu->addAction(QStringLiteral("Start"), this, [this]() {
        qCInfo(lcServiceTrayIcon) << "Start requested for" << m_config.name;
        emit startRequested(m_config.name, m_config.serviceType);
    });

    m_stopAction = menu->addAction(QStringLiteral("Stop"), this, [this]() {
        qCInfo(lcServiceTrayIcon) << "Stop requested for" << m_config.name;
        emit stopRequested(m_config.name, m_config.serviceType);
    });

    m_restartAction = menu->addAction(QStringLiteral("Restart"), this, [this]() {
        qCInfo(lcServiceTrayIcon) << "Restart requested for" << m_config.name;
        emit restartRequested(m_config.name, m_config.serviceType);
    });

    menu->addSeparator();

    menu->addAction(QStringLiteral("View Logs..."), this, [this]() {
        qCInfo(lcServiceTrayIcon) << "View logs requested for" << m_config.name;
        emit viewLogsRequested(m_config.name, m_config.serviceType);
    });

    menu->addSeparator();

    menu->addAction(QStringLiteral("Edit Service..."), this, [this]() {
        qCInfo(lcServiceTrayIcon) << "Edit requested for" << m_config.name;
        emit editRequested(m_config.name, m_config.serviceType);
    });

    menu->addAction(QStringLiteral("Remove from Tray"), this, [this]() {
        qCInfo(lcServiceTrayIcon) << "Remove requested for" << m_config.name;
        emit removeRequested(m_config.name, m_config.serviceType);
    });

    // Delete old menu
    delete contextMenu();
    setContextMenu(menu);
    updateMenuActions();
}

void ServiceTrayIcon::updateMenuActions()
{
    const bool isActive = m_currentStatus == ServiceStatus::Active;
    const bool isTransitioning = m_currentStatus == ServiceStatus::Activating
                                 || m_currentStatus == ServiceStatus::Deactivating;

    if (isTransitioning) {
        m_startAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_restartAction->setEnabled(false);
    } else {
        m_startAction->setEnabled(!isActive);
        m_stopAction->setEnabled(isActive);
        m_restartAction->setEnabled(isActive);
    }
}

void ServiceTrayIcon::updateIcon()
{
    const bool isDark = m_isDarkTheme ? m_isDarkTheme() : false;
    const QString iconPath = m_config.getIconForTheme(isDark);

    QIcon baseIcon;
    if (!iconPath.isEmpty() && (iconPath.startsWith(QLatin1Char('/')) || iconPath.startsWith(QLatin1String("./")))) {
        if (QFileInfo::exists(iconPath)) {
            baseIcon = QIcon(iconPath);
        } else {
            qCWarning(lcServiceTrayIcon) << "Icon file not found:" << iconPath << "- using default";
            baseIcon = QIcon::fromTheme(nyx::utils::DEFAULT_SERVICE_ICON);
        }
    } else {
        baseIcon = QIcon::fromTheme(
            iconPath.isEmpty() ? nyx::utils::DEFAULT_SERVICE_ICON : iconPath,
            QIcon::fromTheme(nyx::utils::DEFAULT_SERVICE_ICON));
    }

    const QColor color = nyx::utils::statusColor(m_currentStatus);
    setIcon(addStatusOverlay(baseIcon, color));
}

QIcon ServiceTrayIcon::addStatusOverlay(const QIcon &baseIcon, const QColor &color)
{
    const QSize size(48, 48);
    QPixmap pixmap = baseIcon.pixmap(size);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const int indicatorSize = 16;
    const int x = size.width() - indicatorSize - 2;
    const int y = size.height() - indicatorSize - 2;

    // White border
    painter.setBrush(Qt::white);
    painter.setPen(Qt::white);
    painter.drawEllipse(x, y, indicatorSize, indicatorSize);

    // Colored indicator
    painter.setBrush(color);
    painter.setPen(color);
    painter.drawEllipse(x + 2, y + 2, indicatorSize - 4, indicatorSize - 4);

    painter.end();
    return QIcon(pixmap);
}

void ServiceTrayIcon::updateTooltip()
{
    const QString statusText = nyx::models::serviceStatusToString(m_currentStatus);
    setToolTip(QStringLiteral("%1\nStatus: %2\nType: %3 service")
                   .arg(m_config.displayName,
                        statusText.at(0).toUpper() + statusText.mid(1),
                        m_config.serviceType));
}

} // namespace nyx::ui
