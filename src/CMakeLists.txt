find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6)

add_executable(kde-quicklook quicklook.cpp)

target_include_directories(kde-quicklook PRIVATE
    /usr/include/poppler/qt6
    /usr/include/poppler
)

target_link_libraries(kde-quicklook
    Qt6::Core
    Qt6::Widgets
    KF6::KIOCore
    KF6::I18n
    /usr/lib64/libpoppler-qt6.so
    /usr/lib64/libpoppler.so
)
