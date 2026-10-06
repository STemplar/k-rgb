#include "casecontroller.h"
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

    app.setQuitOnLastWindowClosed(false);

    if(app.arguments().contains(QStringLiteral("--apply"))) {
        KeyboardController controller;
        CaseController     caseController;
        LightingSettings::load(Profiles::current()).apply(controller, &caseController);
        return 0;
    }

    KDBusService service(KDBusService::Unique);

    auto* controller = new KeyboardController(&app);
    auto* caseController = new CaseController(&app);
    auto* window = new MainWindow(controller, caseController);

    auto* fileMenu = window->menuBar()->addMenu(i18n("&File"));
    auto* quitAction = fileMenu->addAction(
        QIcon::fromTheme(QStringLiteral("application-exit")),
        i18n("Quit k-rgb"));
    quitAction->setShortcut(QKeySequence::Quit);
    QObject::connect(quitAction, &QAction::triggered,
                     &app, &QApplication::quit);

    for(auto* button : window->findChildren<QPushButton*>()) {
        if(button->text() == i18n("Quit k-rgb")) {
            button->hide();
        }
    }

    QObject::connect(&service, &KDBusService::activateRequested, window,
                     [window](const QStringList&, const QString&) {
                         window->show();
                         window->raise();
                         window->activateWindow();
                     });

    if(!app.arguments().contains(QStringLiteral("--tray"))) {
        window->show();
    }
    return app.exec();
}
