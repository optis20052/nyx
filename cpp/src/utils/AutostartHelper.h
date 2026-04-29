#pragma once

#include <QString>
#include <utility>

namespace nyx::utils {

class AutostartHelper {
public:
    static bool isAutostartEnabled();
    static std::pair<bool, QString> enableAutostart();
    static std::pair<bool, QString> disableAutostart();

private:
    static QString autostartDir();
    static QString autostartFile();
    static QString findDesktopFile();
};

} // namespace nyx::utils
