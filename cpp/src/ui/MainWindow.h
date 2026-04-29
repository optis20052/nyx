#pragma once

#include "models/ServiceConfig.h"
#include "models/ServiceStatus.h"

#include <QHash>
#include <QList>
#include <QMainWindow>

class QLabel;
class QStackedWidget;
class QTableWidget;

namespace nyx::core {
class ConfigManager;
}

namespace nyx::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(nyx::core::ConfigManager *configManager = nullptr,
                        QWidget *parent = nullptr);

    using ServiceKey = QPair<QString, QString>;
    void updateServices(const QList<nyx::models::ServiceConfig> &services,
                        const QHash<ServiceKey, nyx::models::ServiceStatus> &statuses);

signals:
    void serviceStartRequested(const QString &name, const QString &type);
    void serviceStopRequested(const QString &name, const QString &type);
    void serviceRestartRequested(const QString &name, const QString &type);
    void serviceAddRequested();
    void serviceEditRequested(const QString &name, const QString &type);
    void serviceRemoveRequested(const QString &name, const QString &type);
    void serviceLogsRequested(const QString &name, const QString &type);
    void settingsChanged();
    void exitAppRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void initUi();
    QWidget *createActionsWidget(const nyx::models::ServiceConfig &service,
                                 nyx::models::ServiceStatus status);
    void showServiceMenu(const nyx::models::ServiceConfig &service, QWidget *anchor);
    void showSettings();

    nyx::core::ConfigManager *m_configManager = nullptr;
    QTableWidget *m_servicesTable = nullptr;
    QStackedWidget *m_tableStack = nullptr;
};

} // namespace nyx::ui
