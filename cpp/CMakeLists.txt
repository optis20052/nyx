cmake_minimum_required(VERSION 3.16)
project(NyxApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Network Svg Widgets)

set(NYXCORE_SOURCES
    src/models/ServiceConfig.cpp
    src/models/ServiceInfo.cpp
    src/utils/AutostartHelper.cpp
    src/utils/PolkitHelper.cpp
    src/core/ConfigManager.cpp
    src/core/NotificationManager.cpp
    src/core/ServiceManager.cpp
    src/ui/ServiceTrayIcon.cpp
    src/ui/MainWindow.cpp
    src/ui/dialogs/AddServiceDialog.cpp
    src/ui/dialogs/EditServiceDialog.cpp
    src/ui/dialogs/LogViewerDialog.cpp
    src/ui/dialogs/SettingsDialog.cpp
    src/NyxApp.cpp
)

set(NYXCORE_HEADERS
    src/models/ServiceStatus.h
    src/models/ServiceConfig.h
    src/models/ServiceInfo.h
    src/utils/Constants.h
    src/utils/AutostartHelper.h
    src/utils/PolkitHelper.h
    src/core/ConfigManager.h
    src/core/NotificationManager.h
    src/core/ServiceManager.h
    src/ui/ServiceTrayIcon.h
    src/ui/MainWindow.h
    src/ui/dialogs/AddServiceDialog.h
    src/ui/dialogs/EditServiceDialog.h
    src/ui/dialogs/LogViewerDialog.h
    src/ui/dialogs/SettingsDialog.h
    src/NyxApp.h
)

qt_add_resources(NYXCORE_RESOURCES resources/resources.qrc)

add_library(nyxcore STATIC ${NYXCORE_SOURCES} ${NYXCORE_HEADERS} ${NYXCORE_RESOURCES})

target_include_directories(nyxcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(nyxcore PUBLIC
    Qt6::Core
    Qt6::DBus
    Qt6::Network
    Qt6::Svg
    Qt6::Widgets
)

add_executable(nyxapp src/main.cpp)
target_link_libraries(nyxapp PRIVATE nyxcore)
