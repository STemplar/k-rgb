#include "casecontroller.h"
#include "diagnosewidget.h"
#include "keyboardcontroller.h"
#include "lightmountkeyboardwidget.h"
#include "logitechkeyboardwidget.h"
#include "mainwindow.h"
#include "settings.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QIcon>
#include <QKeySequence>
#include <QListView>
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

    if(auto* tabs = window->findChild<QTabWidget*>()) {
        // MainWindow's original keyboard page is the Alienware page. Keep it
        // intact for Alienware and do not reuse it for either HID++ or be quiet!.
        QWidget* alienwarePage = tabs->count() > 0 ? tabs->widget(0) : nullptr;

        // The shared page had temporarily gained a Logitech-only Zones entry.
        // Hide it so Alienware presents its original mode list again.
        if(alienwarePage) {
            for(auto* combo : alienwarePage->findChildren<QComboBox*>()) {
                const int zones = combo->findText(i18n("Zones (custom)"));
                if(zones >= 0) {
                    if(auto* view = qobject_cast<QListView*>(combo->view())) {
                        view->setRowHidden(zones, true);
                    }
                }
            }
        }

        auto* lightMountPage = new LightMountKeyboardWidget(controller, tabs);
        auto* logitechPage = new LogitechKeyboardWidget(controller, tabs);

        tabs->insertTab(0, lightMountPage,
                        QIcon::fromTheme(QStringLiteral("input-keyboard")),
                        i18n("Keyboard"));
        tabs->insertTab(1, logitechPage,
                        QIcon::fromTheme(QStringLiteral("input-keyboard")),
                        i18n("Keyboard"));

        // Diagnose follows whichever protocol-specific keyboard page is visible.
        const int diagnoseInsert = alienwarePage
            ? tabs->indexOf(alienwarePage) + 1
            : 3;
        tabs->insertTab(diagnoseInsert,
                        new DiagnoseWidget(controller, tabs),
                        QIcon::fromTheme(QStringLiteral("dialog-information")),
                        i18n("Diagnose"));

        const auto updateKeyboardPages = [tabs, controller, lightMountPage,
                                          logitechPage, alienwarePage]() {
            const bool lightMount = controller->usesBeQuietLightMount();
            const bool logitech = controller->usesLogitechHIDPP20();
            const bool alienware = controller->usesAlienware();

            if(const int i = tabs->indexOf(lightMountPage); i >= 0) {
                tabs->setTabVisible(i, lightMount);
            }
            if(const int i = tabs->indexOf(logitechPage); i >= 0) {
                tabs->setTabVisible(i, logitech);
            }
            if(alienwarePage) {
                if(const int i = tabs->indexOf(alienwarePage); i >= 0) {
                    tabs->setTabVisible(i, alienware);
                }
            }

            if(lightMount) {
                lightMountPage->loadCurrentProfile();
                tabs->setCurrentWidget(lightMountPage);
            } else if(logitech) {
                logitechPage->refreshFromDevice();
                tabs->setCurrentWidget(logitechPage);
            } else if(alienware && alienwarePage) {
                tabs->setCurrentWidget(alienwarePage);
            }
        };

        QObject::connect(controller, &KeyboardController::connectionChanged,
                         window, [updateKeyboardPages](bool, const QString&) {
                             updateKeyboardPages();
                         });
        QObject::connect(tabs, &QTabWidget::currentChanged, window,
                         [tabs, lightMountPage, logitechPage](int index) {
                             if(index < 0) {
                                 return;
                             }
                             if(tabs->widget(index) == lightMountPage) {
                                 lightMountPage->loadCurrentProfile();
                             } else if(tabs->widget(index) == logitechPage) {
                                 logitechPage->loadCurrentProfile();
                             }
                         });

        // Keep both native pages synchronized with the shared profile selector.
        for(auto* combo : window->findChildren<QComboBox*>()) {
            if(combo->accessibleName() == i18n("Profile")) {
                QObject::connect(combo, &QComboBox::currentTextChanged,
                                 lightMountPage,
                                 [lightMountPage](const QString&) {
                                     lightMountPage->loadCurrentProfile();
                                 });
                QObject::connect(combo, &QComboBox::currentTextChanged,
                                 logitechPage,
                                 [logitechPage](const QString&) {
                                     logitechPage->loadCurrentProfile();
                                 });
                break;
            }
        }

        updateKeyboardPages();
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
