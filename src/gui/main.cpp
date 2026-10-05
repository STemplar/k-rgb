#include "casecontroller.h"
#include "diagnosewidget.h"
#include "keyboardcontroller.h"
#include "lightmountkeyboardwidget.h"
#include "mainwindow.h"
#include "settings.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
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

    if(auto* tabs = window->findChild<QTabWidget*>()) {
        QWidget* legacyKeyboardPage = tabs->count() > 0 ? tabs->widget(0) : nullptr;

        // be quiet! uses its own effect vocabulary and wire values. Do not map
        // Light Mount modes through the Alienware/Logitech page; give it a
        // protocol-native page instead. The old page remains available for the
        // other backends and retains the existing per-vendor logic there.
        auto* lightMountPage = new LightMountKeyboardWidget(controller, tabs);
        tabs->insertTab(0, lightMountPage,
                        QIcon::fromTheme(QStringLiteral("input-keyboard")),
                        i18n("Keyboard"));

        // Keep diagnostics as the next visible tab. The hidden backend-specific
        // page does not occupy visible tab-bar space.
        const int diagnoseInsert = legacyKeyboardPage
            ? tabs->indexOf(legacyKeyboardPage) + 1
            : 1;
        tabs->insertTab(diagnoseInsert,
                        new DiagnoseWidget(controller, tabs),
                        QIcon::fromTheme(QStringLiteral("dialog-information")),
                        i18n("Diagnose"));

        const auto updateKeyboardPages = [tabs, controller, lightMountPage,
                                          legacyKeyboardPage]() {
            const bool lightMount = controller->usesBeQuietLightMount();
            const int nativeIndex = tabs->indexOf(lightMountPage);
            if(nativeIndex >= 0) {
                tabs->setTabVisible(nativeIndex, lightMount);
            }
            if(legacyKeyboardPage) {
                const int legacyIndex = tabs->indexOf(legacyKeyboardPage);
                if(legacyIndex >= 0) {
                    tabs->setTabVisible(legacyIndex, !lightMount);
                }
            }

            if(lightMount) {
                lightMountPage->loadCurrentProfile();
                if(tabs->currentWidget() == legacyKeyboardPage) {
                    tabs->setCurrentWidget(lightMountPage);
                }
            } else if(tabs->currentWidget() == lightMountPage && legacyKeyboardPage) {
                tabs->setCurrentWidget(legacyKeyboardPage);
            }
        };

        QObject::connect(controller, &KeyboardController::connectionChanged,
                         window, [updateKeyboardPages](bool, const QString&) {
                             updateKeyboardPages();
                         });
        QObject::connect(tabs, &QTabWidget::currentChanged, window,
                         [tabs, lightMountPage](int index) {
                             if(index >= 0 && tabs->widget(index) == lightMountPage) {
                                 lightMountPage->loadCurrentProfile();
                             }
                         });

        // Keep the native page synchronized with the shared profile selector.
        for(auto* combo : window->findChildren<QComboBox*>()) {
            if(combo->accessibleName() == i18n("Profile")) {
                QObject::connect(combo, &QComboBox::currentTextChanged,
                                 lightMountPage,
                                 [lightMountPage](const QString&) {
                                     lightMountPage->loadCurrentProfile();
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

    // `--tray` (used by the "Start in system tray at login" autostart entry)
    // starts hidden: the tray icon is live, but no window pops up at login.
    if(!app.arguments().contains(QStringLiteral("--tray"))) {
        window->show();
    }
    return app.exec();
}
