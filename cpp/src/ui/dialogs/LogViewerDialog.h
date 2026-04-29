#pragma once

#include "models/ServiceConfig.h"

#include <QDialog>
#include <QTimer>

class QCheckBox;
class QPushButton;
class QSpinBox;
class QTextEdit;

namespace nyx::core {
class ServiceManager;
}

namespace nyx::ui {

class LogViewerDialog : public QDialog {
    Q_OBJECT

public:
    LogViewerDialog(const nyx::models::ServiceConfig &config,
                    nyx::core::ServiceManager *serviceManager,
                    QWidget *parent = nullptr);

private slots:
    void loadLogs();
    void exportLogs();
    void toggleAutoRefresh(int state);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void initUi();

    nyx::models::ServiceConfig m_config;
    nyx::core::ServiceManager *m_serviceManager;

    QTextEdit *m_logText = nullptr;
    QSpinBox *m_linesSpinbox = nullptr;
    QTimer m_refreshTimer;
};

} // namespace nyx::ui
