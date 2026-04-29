#include "AutostartHelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTextStream>

Q_LOGGING_CATEGORY(lcAutostartHelper, "nyx.utils.autostarthelper")

namespace nyx::utils {

QString AutostartHelper::autostartDir()
{
    return QDir::homePath() + QStringLiteral("/.config/autostart");
}

QString AutostartHelper::autostartFile()
{
    return autostartDir() + QStringLiteral("/nyxapp.desktop");
}

QString AutostartHelper::findDesktopFile()
{
    // Check development location
    const QString devFile = QCoreApplication::applicationDirPath()
                            + QStringLiteral("/../nyxapp.desktop");
    if (QFileInfo::exists(devFile))
        return devFile;

    // Check standard system locations
    const QStringList locations = {
        QStringLiteral("/usr/share/applications/nyxapp.desktop"),
        QStringLiteral("/usr/local/share/applications/nyxapp.desktop"),
        QDir::homePath() + QStringLiteral("/.local/share/applications/nyxapp.desktop"),
    };

    for (const QString &path : locations) {
        if (QFileInfo::exists(path))
            return path;
    }

    return {};
}

bool AutostartHelper::isAutostartEnabled()
{
    return QFileInfo::exists(autostartFile());
}

std::pair<bool, QString> AutostartHelper::enableAutostart()
{
    QDir().mkpath(autostartDir());

    const QString desktopFile = findDesktopFile();
    if (desktopFile.isEmpty()) {
        const QString msg = QStringLiteral(
            "Could not find nyxapp.desktop file in any standard location.\n"
            "Please ensure the application is properly installed.");
        qCWarning(lcAutostartHelper) << msg;
        return {false, msg};
    }

    QFile src(desktopFile);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {false, QStringLiteral("Failed to read desktop file: %1").arg(src.errorString())};
    }

    QStringList lines;
    QTextStream in(&src);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith(QLatin1String("Exec=")) && !line.contains(QLatin1String("--startup"))) {
            line = line.trimmed() + QStringLiteral(" --startup");
        } else if (line.startsWith(QLatin1String("StartupNotify="))) {
            line = QStringLiteral("StartupNotify=false");
        }
        lines.append(line);
    }
    src.close();

    QFile dest(autostartFile());
    if (!dest.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {false, QStringLiteral("Failed to write autostart file: %1").arg(dest.errorString())};
    }

    QTextStream out(&dest);
    for (const QString &line : lines) {
        out << line << '\n';
    }
    dest.close();
    dest.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

    qCInfo(lcAutostartHelper) << "Autostart enabled:" << autostartFile();
    return {true, QStringLiteral("Autostart enabled. NyxApp will start automatically on login.")};
}

std::pair<bool, QString> AutostartHelper::disableAutostart()
{
    const QString path = autostartFile();
    if (!QFileInfo::exists(path)) {
        return {true, QStringLiteral("Autostart is already disabled.")};
    }

    if (!QFile::remove(path)) {
        return {false, QStringLiteral("Failed to remove autostart file.")};
    }

    qCInfo(lcAutostartHelper) << "Autostart disabled:" << path;
    return {true, QStringLiteral("Autostart disabled. NyxApp will not start automatically on login.")};
}

} // namespace nyx::utils
