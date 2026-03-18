# CMake generated Testfile for 
# Source directory: /home/prasanth/Downloads/kde-quicklook-pkg
# Build directory: /home/prasanth/Downloads/kde-quicklook-pkg/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(appstreamtest "/usr/bin/cmake" "-DAPPSTREAMCLI=/usr/bin/appstreamcli" "-DINSTALL_FILES=/home/prasanth/Downloads/kde-quicklook-pkg/build/install_manifest.txt" "-P" "/usr/share/ECM/kde-modules/appstreamtest.cmake")
set_tests_properties(appstreamtest PROPERTIES  _BACKTRACE_TRIPLES "/usr/share/ECM/kde-modules/KDECMakeSettings.cmake;173;add_test;/usr/share/ECM/kde-modules/KDECMakeSettings.cmake;191;appstreamtest;/usr/share/ECM/kde-modules/KDECMakeSettings.cmake;0;;/home/prasanth/Downloads/kde-quicklook-pkg/CMakeLists.txt;12;include;/home/prasanth/Downloads/kde-quicklook-pkg/CMakeLists.txt;0;")
subdirs("src")
