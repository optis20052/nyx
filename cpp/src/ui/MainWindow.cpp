#include "MainWindow.h"
#include "core/ConfigManager.h"
#include "ui/dialogs/SettingsDialog.h"
#include "utils/Constants.h"

#include <QAction>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcMainWindow, "nyx.ui.mainwindow")

namespace nyx::ui {

using nyx::models::ServiceConfig;
using nyx::models::ServiceStatus;

MainWindow::MainWindow(nyx::core::ConfigManager *configManager, QWidget *parent)
    : QMainWindow(parent)
    , m_configManager(configManager)
{
    initUi();
}

void MainWindow::initUi()
{
    setWindowTitle(QStringLiteral("%1 - Service Manager").arg(nyx::utils::APP_NAME));
    setMinimumSize(800, 600);

    auto *central = new QWidget();
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    // Header
    auto *headerLayout = new QHBoxLayout();
    headerLayout->addWidget(new QLabel(QStringLiteral("<h2>Service Manager</h2>")));
    headerLayout->addStretch();

    auto *addBtn = new QPushButton(QStringLiteral("Add Service"));
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::serviceAddRequested);
    headerLayout->addWidget(addBtn);
    layout->addLayout(headerLayout);

    // Stacked widget: loading / table / empty
    m_tableStack = new QStackedWidget();

    // Index 0 — Loading
    auto *loadingWidget = new QWidget();
    auto *loadingLayout = new QVBoxLayout(loadingWidget);
    loadingLayout->addStretch();
    auto *loadingLabel = new QLabel(QStringLiteral("Loading services..."));
    loadingLabel->setStyleSheet(QStringLiteral("font-size: 14pt; color: gray;"));
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel);
    auto *loadingSubLabel = new QLabel(QStringLiteral("Please wait while service statuses are being retrieved"));
    loadingSubLabel->setStyleSheet(QStringLiteral("font-size: 10pt; color: gray;"));
    loadingSubLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingSubLabel);
    loadingLayout->addStretch();
    m_tableStack->addWidget(loadingWidget);

    // Index 1 — Table
    m_servicesTable = new QTableWidget();
    m_servicesTable->setColumnCount(6);
    m_servicesTable->setHorizontalHeaderLabels(
        {QStringLiteral("Service"), QStringLiteral("Display Name"), QStringLiteral("Type"),
         QStringLiteral("Status"), QStringLiteral("Auto-Start"), QStringLiteral("Actions")});

    auto *header = m_servicesTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_servicesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_servicesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_servicesTable->setAlternatingRowColors(true);
    m_tableStack->addWidget(m_servicesTable);

    // Index 2 — Empty state
    auto *emptyWidget = new QWidget();
    auto *emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->addStretch();
    auto *emptyLabel = new QLabel(QStringLiteral("No Services Configured"));
    emptyLabel->setStyleSheet(QStringLiteral("font-size: 14pt; color: gray;"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);
    auto *emptySubLabel = new QLabel(QStringLiteral("Click 'Add Service' to add your first service"));
    emptySubLabel->setStyleSheet(QStringLiteral("font-size: 10pt; color: gray;"));
    emptySubLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptySubLabel);
    emptyLayout->addStretch();
    m_tableStack->addWidget(emptyWidget);

    m_tableStack->setCurrentIndex(0);
    layout->addWidget(m_tableStack);

    // Bottom buttons
    auto *bottomLayout = new QHBoxLayout();

    auto *exitBtn = new QPushButton(QStringLiteral("Exit App"));
    exitBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e74c3c; color: white; font-weight: bold; "
        "padding: 5px 15px; border-radius: 3px; } "
        "QPushButton:hover { background-color: #c0392b; } "
        "QPushButton:pressed { background-color: #a93226; }"));
    exitBtn->setToolTip(QStringLiteral("Completely exit the application (closes all tray icons)"));
    connect(exitBtn, &QPushButton::clicked, this, &MainWindow::exitAppRequested);
    bottomLayout->addWidget(exitBtn);

    bottomLayout->addStretch();

    auto *settingsBtn = new QPushButton(QStringLiteral("Settings"));
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::showSettings);
    bottomLayout->addWidget(settingsBtn);

    auto *closeBtn = new QPushButton(QStringLiteral("Close"));
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    bottomLayout->addWidget(closeBtn);

    layout->addLayout(bottomLayout);

    qCInfo(lcMainWindow) << "Main window initialized";
}

void MainWindow::updateServices(const QList<ServiceConfig> &services,
                                const QHash<ServiceKey, ServiceStatus> &statuses)
{
    if (services.isEmpty()) {
        m_tableStack->setCurrentIndex(2);
        return;
    }

    m_tableStack->setCurrentIndex(1);
    m_servicesTable->setRowCount(services.size());

    for (int row = 0; row < services.size(); ++row) {
        const ServiceConfig &service = services[row];
        const ServiceKey key(service.name, service.serviceType);
        const ServiceStatus status = statuses.value(key, ServiceStatus::Unknown);

        auto makeItem = [](const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            return item;
        };

        m_servicesTable->setItem(row, 0, makeItem(service.name));
        m_servicesTable->setItem(row, 1, makeItem(service.displayName));
        m_servicesTable->setItem(row, 2, makeItem(service.serviceType));

        // Status with color
        const QString statusStr = nyx::models::serviceStatusToString(status);
        auto *statusItem = makeItem(statusStr.at(0).toUpper() + statusStr.mid(1));
        if (status == ServiceStatus::Active)
            statusItem->setForeground(Qt::darkGreen);
        else if (status == ServiceStatus::Failed)
            statusItem->setForeground(Qt::red);
        else if (status == ServiceStatus::Inactive)
            statusItem->setForeground(Qt::gray);
        m_servicesTable->setItem(row, 3, statusItem);

        m_servicesTable->setItem(row, 4, makeItem(service.autoStart ? QStringLiteral("Yes") : QStringLiteral("No")));
        m_servicesTable->setCellWidget(row, 5, createActionsWidget(service, status));
    }
}

QWidget *MainWindow::createActionsWidget(const ServiceConfig &service, ServiceStatus status)
{
    auto *widget = new QWidget();
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *startBtn = new QPushButton(QStringLiteral("Start"));
    startBtn->setMaximumWidth(60);
    startBtn->setEnabled(status != ServiceStatus::Active);
    connect(startBtn, &QPushButton::clicked, this, [this, name = service.name, type = service.serviceType]() {
        emit serviceStartRequested(name, type);
    });
    layout->addWidget(startBtn);

    auto *stopBtn = new QPushButton(QStringLiteral("Stop"));
    stopBtn->setMaximumWidth(60);
    stopBtn->setEnabled(status == ServiceStatus::Active);
    connect(stopBtn, &QPushButton::clicked, this, [this, name = service.name, type = service.serviceType]() {
        emit serviceStopRequested(name, type);
    });
    layout->addWidget(stopBtn);

    auto *moreBtn = new QPushButton(QStringLiteral("\xe2\x8b\xae")); // Unicode vertical ellipsis
    moreBtn->setMaximumWidth(30);
    connect(moreBtn, &QPushButton::clicked, this, [this, service, moreBtn]() {
        showServiceMenu(service, moreBtn);
    });
    layout->addWidget(moreBtn);

    return widget;
}

void MainWindow::showServiceMenu(const ServiceConfig &service, QWidget *anchor)
{
    QMenu menu(this);

    menu.addAction(QStringLiteral("Restart"), this, [this, name = service.name, type = service.serviceType]() {
        emit serviceRestartRequested(name, type);
    });

    menu.addAction(QStringLiteral("View Logs"), this, [this, name = service.name, type = service.serviceType]() {
        emit serviceLogsRequested(name, type);
    });

    menu.addSeparator();

    menu.addAction(QStringLiteral("Edit"), this, [this, name = service.name, type = service.serviceType]() {
        emit serviceEditRequested(name, type);
    });

    menu.addAction(QStringLiteral("Remove"), this, [this, name = service.name, type = service.serviceType]() {
        emit serviceRemoveRequested(name, type);
    });

    menu.exec(anchor->mapToGlobal(anchor->rect().bottomLeft()));
}

void MainWindow::showSettings()
{
    if (!m_configManager) {
        QMessageBox::warning(this, QStringLiteral("Settings"),
                             QStringLiteral("Settings are not available at this time."));
        return;
    }

    SettingsDialog dialog(m_configManager, this);
    if (dialog.exec()) {
        if (dialog.hasChanges()) {
            emit settingsChanged();
            QMessageBox::information(this, QStringLiteral("Settings Saved"),
                                     QStringLiteral("Settings have been saved.\n\n"
                                                    "Some changes may require restarting the application."));
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
    qCInfo(lcMainWindow) << "Main window hidden";
}

} // namespace nyx::ui
