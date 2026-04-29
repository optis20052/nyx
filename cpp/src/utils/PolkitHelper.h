#pragma once

#include <QString>
#include <utility>

namespace nyx::core {
class ConfigManager;
}

namespace nyx::utils {

class PolkitHelper {
public:
    static bool isPasswordlessEnabled(const nyx::core::ConfigManager *configManager = nullptr);
    static std::pair<bool, QString> enablePasswordlessMode(const QString &username,
                                                           nyx::core::ConfigManager *configManager = nullptr);
    static std::pair<bool, QString> disablePasswordlessMode(nyx::core::ConfigManager *configManager = nullptr);
    static QString getCurrentUsername();

private:
    static inline const QString POLKIT_RULE_FILE =
        QStringLiteral("/etc/polkit-1/rules.d/50-nyxapp-systemctl.rules");
};

} // namespace nyx::utils
