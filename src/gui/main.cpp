#include "casecontroller.h"
#include "diagnosewidget.h"
#include "keyboardcontroller.h"
#include "mainwindow.h"
#include "settings.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTabWidget>

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("krgb");

    KAboutData about(QStringLiteral("krgb"),
                     i18n("k-rgb"),
                     QStringLiteral("0.3.0"),
                     i18n("Control RGB lighting on supported keyboards and lighting devices"),
                     KAboutLicense::GPL_V2,
                     i18n("© 2026 Randy Yates"));
    about.addAuthor(i18n("Randy Yates"), QString(), QStringLiteral("randyyates@gmail.com"));
    about.setHomepage(QStringLiteral("https://github.com/BusyBeaverSoftware/k-rgb"));
    about.setDesktopFileName(QStringLiteral("io.github.busybeaversoftware.krgb"));
    KAboutData::setApplicationData(about);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("input-keyboard")));

    // The app lives in the system tray; closing the window keeps it running.
    app.setQuitOnLastWindowClosed(false);

    // Headless restore mode (used by the "Restore at login" autostart entry):
    // re-apply the saved lighting and exit, without showing a window.
    if(app.arguments().contains(QStringLiteral("--apply"))) {
        KeyboardController controller;
        CaseController     caseController;
        LightingSettings::load(Profiles::current()).apply(controller, &caseController);
        return 0;
    }

    // Single-instance: re-activating brings the existing window forward.
    KDBusService service(KDBusService::Unique);

    auto* controller = new KeyboardController(&app);
    auto* caseController = new CaseController(&app);
    auto* window = new MainWindow(controller, caseController);

    // Normal desktop application menu. Closing the window still hides to the
    // tray; File -> Quit is the explicit way to terminate the application.
    auto* fileMenu = window->menuBar()->addMenu(i18n("&File"));
    auto* quitAction = fileMenu->addAction(
        QIcon::fromTheme(QStringLiteral("application-exit")),
        i18n("Quit k-rgb"));
    quitAction->setShortcut(QKeySequence::Quit);
    QObject::connect(quitAction, &QAction::triggered,
                     &app, &QApplication::quit);

    // The old in-page Quit button is retained in MainWindow for source
    // compatibility but no longer shown. Its function now lives in File.
    for(auto* button : window->findChildren<QPushButton*>()) {
        if(button->text() == i18n("Quit k-rgb")) {
            button->hide();
        }
    }

    // MainWindow owns the application's tab widget. Keep diagnostics as the
    // second tab so device information is adjacent to the Keyboard controls,
    // while the optional Case tab remains after it.
    if(auto* tabs = window->findChild<QTabWidget*>()) {
        tabs->insertTab(1,
                        new DiagnoseWidget(controller, tabs),
                        QIcon::fromTheme(QStringLiteral("dialog-information")),
                        i18n("Diagnose"));
    }

    QObject::connect(&service, &KDBusService::activateRequested, window,
                     [window](const QStringList&, const QString&) {
                         window->show();
                         window->raise();
                         window->activateWindow();
                     });

    // `--tray` (used by the "Start in system tray at login" autostart entry)
    // starts hidden: the tray icon is live, but no window pops up at login.
    if(!app.arguments().contains(QStringLiteral("--tray"))) {
        window->show();
    }
    return app.exec();
}
