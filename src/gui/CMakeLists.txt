find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core DBus)
find_package(KF6 REQUIRED COMPONENTS
    CoreAddons
    I18n
    WidgetsAddons
    Config
    ConfigWidgets
    XmlGui
    DBusAddons
    StatusNotifierItem
)

set(CMAKE_AUTOMOC ON)

add_executable(krgb
    main.cpp
    mainwindow.cpp
    keyboardcontroller.cpp
    keyboardwidget.cpp
    casecontroller.cpp
    zonegridwidget.cpp
    settings.cpp
)

target_link_libraries(krgb PRIVATE
    krgb-core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::DBus
    KF6::CoreAddons
    KF6::I18n
    KF6::WidgetsAddons
    KF6::ConfigGui
    KF6::ConfigWidgets
    KF6::XmlGui
    KF6::DBusAddons
    KF6::StatusNotifierItem
)

install(TARGETS krgb ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
