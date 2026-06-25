cmake_minimum_required(VERSION 3.16)
project(k-rgb VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(GNUInstallDirs)

option(BUILD_GUI "Build the KF6 GUI (requires Qt6 + KF6 dev packages)" OFF)
set(UDEV_RULES_INSTALL_DIR "/lib/udev/rules.d"
    CACHE PATH "Directory to install the udev rule into")

find_package(Threads REQUIRED)

# --- Core engine: pure C++ + Linux hidraw, no Qt/KF6 dependency -------------
add_library(krgb-core STATIC
    src/core/aw410k_device.cpp
    src/core/alienfx_device.cpp
)
target_include_directories(krgb-core PUBLIC src)
target_link_libraries(krgb-core PUBLIC Threads::Threads)

# --- Test / scripting CLI ---------------------------------------------------
add_executable(krgb-cli src/cli/main.cpp)
target_link_libraries(krgb-cli PRIVATE krgb-core)
install(TARGETS krgb-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- GUI: Qt6 + KF6 (opt-in) ------------------------------------------------
if(BUILD_GUI)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)

    add_subdirectory(src/gui)

    # --- Desktop integration ------------------------------------------------
    install(FILES packaging/io.github.busybeaversoftware.krgb.desktop
            DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES packaging/io.github.busybeaversoftware.krgb.metainfo.xml
            DESTINATION ${KDE_INSTALL_METAINFODIR})
    install(FILES packaging/icons/io.github.busybeaversoftware.krgb.svg
            DESTINATION ${KDE_INSTALL_FULL_ICONDIR}/hicolor/scalable/apps)
    install(FILES packaging/udev/60-alienware-rgb.rules
            DESTINATION ${UDEV_RULES_INSTALL_DIR})
endif()
