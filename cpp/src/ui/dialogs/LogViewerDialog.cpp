#include "LogViewerDialog.h"
#include "core/ServiceManager.h"
#include "utils/Constants.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcLogViewer, "nyx.ui.logviewer")

namespace nyx::ui {

LogViewerDialog::LogViewerDialog(const nyx::models::ServiceConfig &config,
                                 nyx::core::ServiceManager *serviceManager,
                                 QWidget *parent)
    : QDialog(parent)
    , m_config(config)
    , m_serviceManager(serviceManager)
{
    initUi();
    loadLogs();
    connect(&m_refreshTimer, &QTimer::timeout, this, &LogViewerDialog::loadLogs);
}

void LogViewerDialog::initUi()
{
    setWindowTitle(QStringLiteral("Logs - %1").arg(m_config.displayName));
    setMinimumSize(800, 600);

    auto *layout = new QVBoxLayout(this);

    // Controls
    auto *controls = new QHBoxLayout();

    controls->addWidget(new QLabel(QStringLiteral("Lines:")));

    m_linesSpinbox = new QSpinBox();
    m_linesSpinbox->setRange(10, nyx::utils::MAX_LOG_LINES);
    m_linesSpinbox->setValue(nyx::utils::DEFAULT_LOG_LINES);
    m_linesSpinbox->setSingleStep(100);
    controls->addWidget(m_linesSpinbox);

    controls->addStretch();

    auto *autoRefresh = new QCheckBox(QStringLiteral("Auto-refresh (5s)"));
    connect(autoRefresh, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        toggleAutoRefresh(state == Qt::Checked ? 1 : 0);
    });
    controls->addWidget(autoRefresh);

    auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"));
    connect(refreshBtn, &QPushButton::clicked, this, &LogViewerDialog::loadLogs);
    controls->addWidget(refreshBtn);

    auto *exportBtn = new QPushButton(QStringLiteral("Export..."));
    connect(exportBtn, &QPushButton::clicked, this, &LogViewerDialog::exportLogs);
    controls->addWidget(exportBtn);

    layout->addLayout(controls);

    // Log text
    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setLineWrapMode(QTextEdit::NoWrap);

    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(9);
    m_logText->setFont(monoFont);

    layout->addWidget(m_logText);

    // Bottom
    auto *bottom = new QHBoxLayout();
    bottom->addStretch();
    auto *closeBtn = new QPushButton(QStringLiteral("Close"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(closeBtn);
    layout->addLayout(bottom);
}

void LogViewerDialog::loadLogs()
{
    const int lines = m_linesSpinbox->value();
    qCDebug(lcLogViewer) << "Loading" << lines << "lines of logs for" << m_config.name;

    const QString logs = m_serviceManager->getServiceLogs(
        m_config.name, lines, m_config.isUserService());

    m_logText->setPlainText(logs);

    QTextCursor cursor = m_logText->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logText->setTextCursor(cursor);
}

void LogViewerDialog::exportLogs()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Logs"),
        QStringLiteral("%1_logs.txt").arg(m_config.name),
        QStringLiteral("Text Files (*.txt);;All Files (*)"));

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("Export Failed"),
                              QStringLiteral("Failed to export logs:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << m_logText->toPlainText();
    file.close();

    QMessageBox::information(this, QStringLiteral("Export Successful"),
                             QStringLiteral("Logs exported successfully to:\n%1").arg(filePath));
    qCInfo(lcLogViewer) << "Exported logs to" << filePath;
}

void LogViewerDialog::toggleAutoRefresh(int state)
{
    if (state) {
        m_refreshTimer.start(5000);
        qCDebug(lcLogViewer) << "Auto-refresh enabled";
    } else {
        m_refreshTimer.stop();
        qCDebug(lcLogViewer) << "Auto-refresh disabled";
    }
}

void LogViewerDialog::closeEvent(QCloseEvent *event)
{
    m_refreshTimer.stop();
    event->accept();
}

} // namespace nyx::ui
