#include "PolkitHelper.h"
#include "core/ConfigManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

Q_LOGGING_CATEGORY(lcPolkitHelper, "nyx.utils.polkithelper")

namespace nyx::utils {

static const QString POLKIT_RULE_TEMPLATE = QStringLiteral(
    "/* Allow %1 to manage systemd services without password */\n"
    "polkit.addRule(function(action, subject) {\n"
    "    if ((action.id == \"org.freedesktop.systemd1.manage-units\" ||\n"
    "         action.id == \"org.freedesktop.systemd1.manage-unit-files\" ||\n"
    "         action.id == \"org.freedesktop.systemd1.reload-daemon\") &&\n"
    "        subject.user == \"%1\") {\n"
    "        return polkit.Result.YES;\n"
    "    }\n"
    "});\n"
);

bool PolkitHelper::isPasswordlessEnabled(const nyx::core::ConfigManager *configManager)
{
    if (configManager) {
        return configManager->getSetting(QStringLiteral("passwordless_mode"), false).toBool();
    }

    return QFileInfo::exists(POLKIT_RULE_FILE);
}

std::pair<bool, QString> PolkitHelper::enablePasswordlessMode(const QString &username,
                                                               nyx::core::ConfigManager *configManager)
{
    const QString ruleContent = POLKIT_RULE_TEMPLATE.arg(username);

    const QString tempPath = QDir::tempPath() + QStringLiteral("/systemd-tray-polkit.rules");
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {false, QStringLiteral("Failed to create temporary file")};
    }
    {
        QTextStream out(&tempFile);
        out << ruleContent;
    }
    tempFile.close();

    QProcess process;
    process.start(QStringLiteral("pkexec"),
                  {QStringLiteral("sh"), QStringLiteral("-c"),
                   QStringLiteral("cp %1 %2 && chmod 644 %2").arg(tempPath, POLKIT_RULE_FILE)});

    if (!process.waitForFinished(30000)) {
        QFile::remove(tempPath);
        return {false, QStringLiteral("Operation timed out")};
    }

    QFile::remove(tempPath);

    if (process.exitCode() != 0) {
        const QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
        const QString msg = err.isEmpty() ? QStringLiteral("Failed to create polkit rule") : err;
        qCWarning(lcPolkitHelper) << "Failed to enable passwordless mode:" << msg;
        return {false, msg};
    }

    // Restart polkit to apply
    QProcess restartProcess;
    restartProcess.start(QStringLiteral("pkexec"),
                         {QStringLiteral("systemctl"), QStringLiteral("restart"), QStringLiteral("polkit")});
    if (!restartProcess.waitForFinished(10000)) {
        qCWarning(lcPolkitHelper) << "Could not restart polkit service";
    }

    if (configManager) {
        configManager->setSetting(QStringLiteral("passwordless_mode"), true);
    }

    qCInfo(lcPolkitHelper) << "Passwordless mode enabled successfully";
    return {true, QStringLiteral("Passwordless mode enabled! You won't be asked for password anymore.")};
}

std::pair<bool, QString> PolkitHelper::disablePasswordlessMode(nyx::core::ConfigManager *configManager)
{
    QProcess process;
    process.start(QStringLiteral("pkexec"),
                  {QStringLiteral("rm"), QStringLiteral("-f"), POLKIT_RULE_FILE});

    if (!process.waitForFinished(30000)) {
        return {false, QStringLiteral("Operation timed out")};
    }

    if (process.exitCode() != 0) {
        const QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
        const QString msg = err.isEmpty() ? QStringLiteral("Failed to remove polkit rule") : err;
        qCWarning(lcPolkitHelper) << "Failed to disable passwordless mode:" << msg;
        return {false, msg};
    }

    if (configManager) {
        configManager->setSetting(QStringLiteral("passwordless_mode"), false);
    }

    qCInfo(lcPolkitHelper) << "Passwordless mode disabled successfully";
    return {true, QStringLiteral("Passwordless mode disabled. You will be asked for password again.")};
}

QString PolkitHelper::getCurrentUsername()
{
    const QString user = qEnvironmentVariable("USER");
    if (!user.isEmpty()) return user;

    const QString username = qEnvironmentVariable("USERNAME");
    if (!username.isEmpty()) return username;

    return QStringLiteral("unknown");
}

} // namespace nyx::utils
