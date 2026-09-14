cmake_minimum_required(VERSION 3.28)

project(plasma-nabu-kcm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Gui Qml Quick)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n KCMUtils)

kcmutils_add_qml_kcm(kcm_nabu SOURCES kcm_nabu.cpp)

target_compile_definitions(kcm_nabu PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(kcm_nabu PRIVATE
    Qt6::Core
    Qt6::DBus
    KF6::CoreAddons
    KF6::I18n
    KF6::KCMUtilsQuick
)

install(FILES org.senemos.nabu.kcm.desktop
    DESTINATION ${KDE_INSTALL_APPDIR})
