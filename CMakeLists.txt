cmake_minimum_required(VERSION 3.16)
project(kde-quicklook)

set(CMAKE_CXX_STANDARD 17)
set(QT_MIN_VERSION "6.0.0")
set(KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Widgets Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS KIO I18n Service)

add_subdirectory(src)
