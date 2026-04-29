/**
 * Entry point for NyxApp — systemd service tray manager.
 *
 * Reproduces the logic from nyxapp/main.py:
 *   - Logging setup (file + console)
 *   - CLI argument parsing (--show-window, --no-tray, --startup)
 *   - Single-instance guard via QSharedMemory
 *   - IPC server via QLocalServer / QLocalSocket
 *   - NyxApp coordinator (equivalent to app.py)
 *   - Application event loop
 */

#include "NyxApp.h"
#include "ui/MainWindow.h"
#include "utils/Constants.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QSharedMemory>
#include <QTextStream>
#include <QThread>

Q_LOGGING_CATEGORY(lcMain, "nyx.main")

static const QString IPC_SERVER_NAME = QStringLiteral("nyxapp_ipc_server");

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

static QFile *s_logFile = nullptr;

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);

    QString level;
    switch (type) {
    case QtDebugMsg:    level = QStringLiteral("DEBUG");    break;
    case QtInfoMsg:     level = QStringLiteral("INFO");     break;
    case QtWarningMsg:  level = QStringLiteral("WARNING");  break;
    case QtCriticalMsg: level = QStringLiteral("CRITICAL"); break;
    case QtFatalMsg:    level = QStringLiteral("FATAL");    break;
    }

    const QString category = ctx.category ? QString::fromUtf8(ctx.category) : QStringLiteral("default");
    const QString line = QStringLiteral("%1 - %2 - %3 - %4\n").arg(timestamp, category, level, msg);

    QTextStream(stderr) << line;

    if (s_logFile && s_logFile->isOpen()) {
        QTextStream(s_logFile) << line;
        s_logFile->flush();
    }
}

static void cleanupLogging()
{
    qInstallMessageHandler(nullptr);
    delete s_logFile;
    s_logFile = nullptr;
}

static void setupLogging()
{
    const QString configDir = nyx::utils::configDir();
    QDir().mkpath(configDir);

    s_logFile = new QFile(nyx::utils::logFile());
    s_logFile->open(QIODevice::Append | QIODevice::Text);

    qInstallMessageHandler(messageHandler);
    std::atexit(cleanupLogging);

    qCInfo(lcMain) << "============================================================";
    qCInfo(lcMain) << "Starting" << nyx::utils::APP_NAME;
    qCInfo(lcMain) << "============================================================";
}

// ---------------------------------------------------------------------------
// Single-instance IPC
// ---------------------------------------------------------------------------

static bool sendShowWindowSignal()
{
    const int maxRetries = 5;
    const int retryDelays[] = {100, 200, 500, 1000, 2000};

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        QLocalSocket socket;
        socket.connectToServer(IPC_SERVER_NAME);

        if (socket.waitForConnected(1000)) {
            socket.write("SHOW_WINDOW");
            socket.flush();
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            qCInfo(lcMain) << "Sent show window signal (attempt" << attempt + 1 << ")";
            return true;
        }

        socket.abort();
        if (attempt < maxRetries - 1) {
            QThread::msleep(retryDelays[attempt]);
        }
    }

    qCWarning(lcMain) << "Could not connect to running instance after retries";
    return false;
}

static QSharedMemory *checkSingleInstance()
{
    auto *shm = new QSharedMemory(QStringLiteral("NyxAppUniqueKey"));

    if (!shm->create(1)) {
        qCInfo(lcMain) << "Another instance is already running – sending show window signal";
        sendShowWindowSignal();
        delete shm;
        return nullptr;
    }

    return shm;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    setupLogging();
    Q_INIT_RESOURCE(resources);

    QApplication app(argc, argv);
    app.setApplicationName(nyx::utils::APP_NAME);
    app.setApplicationDisplayName(nyx::utils::APP_NAME);
    app.setApplicationVersion(nyx::utils::APP_VERSION);
    app.setOrganizationName(nyx::utils::ORGANIZATION_NAME);
    app.setDesktopFileName(QStringLiteral("nyxapp"));

    // --- CLI arguments -------------------------------------------------------
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("%1 - Manage systemd services from system tray").arg(nyx::utils::APP_NAME));

    QCommandLineOption helpOpt(
        {QStringLiteral("h"), QStringLiteral("help")},
        QStringLiteral("Displays help on commandline options."));
    QCommandLineOption versionOpt(
        {QStringLiteral("v"), QStringLiteral("version")},
        QStringLiteral("Displays version information."));
    QCommandLineOption showWindowOpt(
        QStringLiteral("show-window"),
        QStringLiteral("Show the management window on startup"));
    QCommandLineOption noTrayOpt(
        QStringLiteral("no-tray"),
        QStringLiteral("Hide the main tray icon (only show service icons)"));
    QCommandLineOption startupOpt(
        QStringLiteral("startup"),
        QStringLiteral("Started from autostart (hides window, internal use)"));

    parser.addOption(helpOpt);
    parser.addOption(versionOpt);
    parser.addOption(showWindowOpt);
    parser.addOption(noTrayOpt);
    parser.addOption(startupOpt);
    parser.process(app);

    if (parser.isSet(helpOpt)) {
        QTextStream(stdout) << parser.helpText();
        return 0;
    }
    if (parser.isSet(versionOpt)) {
        QTextStream(stdout) << QCoreApplication::applicationName()
                            << ' ' << QCoreApplication::applicationVersion() << '\n';
        return 0;
    }

    const bool argShowWindow = parser.isSet(showWindowOpt);
    const bool argNoTray     = parser.isSet(noTrayOpt);
    const bool argStartup    = parser.isSet(startupOpt);

    // --- Single instance check -----------------------------------------------
    QSharedMemory *sharedMemory = checkSingleInstance();
    if (!sharedMemory) {
        return 0;
    }

    app.setQuitOnLastWindowClosed(argNoTray);

    // --- Application icon ----------------------------------------------------
    app.setWindowIcon(QIcon(nyx::utils::appIconPath()));

    // --- Create NyxApp -------------------------------------------------------
    nyx::NyxApp nyxApp;

    // Hide main tray icon if requested
    if (argNoTray && nyxApp.mainTray()) {
        nyxApp.mainTray()->hide();
        qCInfo(lcMain) << "Main tray icon hidden as requested";
    }

    // --- IPC server ----------------------------------------------------------
    QLocalServer::removeServer(IPC_SERVER_NAME);
    QLocalServer ipcServer;

    QObject::connect(&ipcServer, &QLocalServer::newConnection, [&]() {
        QLocalSocket *client = ipcServer.nextPendingConnection();
        if (!client) return;

        client->waitForReadyRead(1000);
        const QByteArray data = client->readAll();

        if (data == "SHOW_WINDOW") {
            qCInfo(lcMain) << "Received show window command";
            nyxApp.mainWindow()->show();
            nyxApp.mainWindow()->raise();
            nyxApp.mainWindow()->activateWindow();
        }

        client->disconnectFromServer();
        client->deleteLater();
    });

    if (!ipcServer.listen(IPC_SERVER_NAME)) {
        qCWarning(lcMain) << "Failed to start IPC server:" << ipcServer.errorString();
    } else {
        qCInfo(lcMain) << "IPC server started successfully";
    }

    // --- Window visibility logic ---------------------------------------------
    if (argShowWindow) {
        nyxApp.mainWindow()->show();
        qCInfo(lcMain) << "Main window shown as requested (--show-window)";
    } else if (argStartup) {
        qCInfo(lcMain) << "Main window hidden (launched from autostart)";
    } else {
        const bool minimizeToTray = nyxApp.configManager()->getSetting(
            QStringLiteral("minimize_to_tray"), false).toBool();
        if (minimizeToTray) {
            qCInfo(lcMain) << "Main window hidden (minimize_to_tray is enabled)";
        } else {
            nyxApp.mainWindow()->show();
            qCInfo(lcMain) << "Main window shown (manual launch)";
        }
    }

    qCInfo(lcMain) << "Application started successfully";

    // --- Event loop ----------------------------------------------------------
    const int exitCode = app.exec();

    // --- Cleanup -------------------------------------------------------------
    qCInfo(lcMain) << "Application exited with code" << exitCode;

    nyxApp.cleanup();
    ipcServer.close();

    if (sharedMemory) {
        sharedMemory->detach();
        delete sharedMemory;
    }

    return exitCode;
}
