#include "mainwindow.h"

#include "casecontroller.h"
#include "keyboardcontroller.h"
#include "keyboardwidget.h"
#include "zonegridwidget.h"
#include "core/aw410k_device.h"
#include "core/keymap.h"
#include "core/logitech_g810_iso105_visual.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include <KColorButton>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStatusNotifierItem>

using krgb::Mode;
using krgb::Speed;
using krgb::Direction;

namespace {

// --- Per-key flag presets ---------------------------------------------------
// Built from the physical key geometry in keymap.h so the bands line up with
// where the keys actually sit on the board.

// Ukraine: blue top half, yellow bottom half (split across the layout height).
// Generate from the active physical geometry so Logitech-only controls/media
// are not left unassigned by an Alienware-only preset map.
QHash<QString, QColor> makeUkraineFlag(bool logitechG810Iso105) {
    QHash<QString, QColor> m;
    const QColor blue(0, 87, 183);
    const QColor yellow(255, 215, 0);

    if(logitechG810Iso105) {
        const float mid =
            krgb::logitech::g810_iso105_visual::kLayoutHeight / 2.0f;
        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const auto* def =
                krgb::logitech::g810_iso105_visual::definition(element);
            if(!def) {
                continue;
            }
            const float cy = element.y + element.h / 2.0f;
            m.insert(QString::fromLatin1(def->name), cy < mid ? blue : yellow);
        }
        return m;
    }

    const float mid = krgb::kLayoutHeight / 2.0f;
    for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
        const krgb::KeyDef& k = krgb::kKeyMap[i];
        const float cy = k.y + k.h / 2.0f;
        m.insert(QString::fromLatin1(k.name), cy < mid ? blue : yellow);
    }
    return m;
}

// USA: solid blue over the left third (full height), red/white stripes by row
// across the right two-thirds.
QHash<QString, QColor> makeUsaFlag(bool logitechG810Iso105) {
    QHash<QString, QColor> m;
    const QColor red(140, 22, 36);
    const QColor white(255, 255, 255);
    const QColor blue(28, 28, 75);

    if(logitechG810Iso105) {
        const float blueRight =
            krgb::logitech::g810_iso105_visual::kLayoutWidth / 3.0f;
        const float stripeHeight =
            krgb::logitech::g810_iso105_visual::kLayoutHeight / 8.0f;
        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const auto* def =
                krgb::logitech::g810_iso105_visual::definition(element);
            if(!def) {
                continue;
            }
            const float cx = element.x + element.w / 2.0f;
            const float cy = element.y + element.h / 2.0f;
            QColor color;
            if(cx < blueRight) {
                color = blue;
            } else {
                const int stripe = static_cast<int>(cy / stripeHeight);
                color = (stripe % 2 == 0) ? red : white;
            }
            m.insert(QString::fromLatin1(def->name), color);
        }
        return m;
    }

    const float blueRight = krgb::kLayoutWidth / 3.0f;
    for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
        const krgb::KeyDef& k = krgb::kKeyMap[i];
        const float cx = k.x + k.w / 2.0f;
        const float cy = k.y + k.h / 2.0f;
        QColor color;
        if(cx < blueRight) {
            color = blue;
        } else {
            const int row = static_cast<int>(cy);
            color = (row % 2 == 0) ? red : white;
        }
        m.insert(QString::fromLatin1(k.name), color);
    }
    return m;
}

} // namespace

MainWindow::MainWindow(KeyboardController* controller, CaseController* caseController, QWidget* parent)
    : KMainWindow(parent), controller_(controller), caseController_(caseController) {
    setWindowTitle(i18n("k-rgb — RGB lighting"));
    populateModes();
    buildUi();
    setupTray();

    refreshProfileCombo();
    switchToProfile(Profiles::current(), /*apply=*/false);

    resize(460, 360);
    setAutoSaveSettings();

    connect(controller_, &KeyboardController::connectionChanged,
            this, &MainWindow::onConnectionChanged);
    connect(controller_, &KeyboardController::error,
            this, &MainWindow::onError);

    if(caseController_) {
        connect(caseController_, &CaseController::availabilityChanged,
                this, &MainWindow::onCaseAvailabilityChanged);
        connect(caseController_, &CaseController::error, this, &MainWindow::onError);
        onCaseAvailabilityChanged(caseController_->isAvailable());
    }

    onConnectionChanged(controller_->isConnected(), controller_->devicePath());
    onModeChanged();
}

void MainWindow::populateModes() {
    //          name                       value                          solid  rnbw   pkey  zones  color  speed  dir
    modes_ = {
        { i18n("Solid colour"),     static_cast<int>(Mode::Direct),      true,  false, false, false, true,  false, false },
        { i18n("Rainbow (static)"), 0,                                   false, true,  false, false, false, false, false },
        { i18n("Per-key (custom)"), 0,                                   false, false, true,  false, true,  false, false },
        { i18n("Zones (custom)"),   0,                                   false, false, false, true,  false, false, false },
        { i18n("Breathing"),        static_cast<int>(Mode::Breathing),   false, false, false, false, true,  true,  false },
        { i18n("Pulse"),            static_cast<int>(Mode::Pulse),       false, false, false, false, true,  true,  false },
        { i18n("Spectrum"),         static_cast<int>(Mode::Spectrum),    false, false, false, false, false, true,  false },
        { i18n("Single wave"),      static_cast<int>(Mode::SingleWave),  false, false, false, false, true,  true,  true  },
        { i18n("Rainbow wave"),     static_cast<int>(Mode::RainbowWave), false, false, false, false, false, true,  true  },
        { i18n("Scanner"),          static_cast<int>(Mode::Scanner),     false, false, false, false, true,  true,  false },
    };
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* outer = new QVBoxLayout(central);

    // --- Profile bar (shared across tabs) ----------------------------------
    auto* profileRow = new QHBoxLayout();
    profileRow->addWidget(new QLabel(i18n("Profile:"), central));
    profileCombo_ = new QComboBox(central);
    profileCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    profileRow->addWidget(profileCombo_, 1);
    newProfileBtn_ = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), QString(), central);
    newProfileBtn_->setToolTip(i18n("New profile"));
    renameProfileBtn_ = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-rename")), QString(), central);
    renameProfileBtn_->setToolTip(i18n("Rename profile"));
    deleteProfileBtn_ = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), QString(), central);
    deleteProfileBtn_->setToolTip(i18n("Delete profile"));
    profileRow->addWidget(newProfileBtn_);
    profileRow->addWidget(renameProfileBtn_);
    profileRow->addWidget(deleteProfileBtn_);
    outer->addLayout(profileRow);

    // --- Tabs --------------------------------------------------------------
    tabs_ = new QTabWidget(central);
    tabs_->addTab(buildKeyboardPage(), QIcon::fromTheme(QStringLiteral("input-keyboard")), i18n("Keyboard"));
    casePage_ = buildCasePage();  // added to the tab bar only when a controller is present
    outer->addWidget(tabs_, 1);

    // --- Login options (shared) --------------------------------------------
    autostartCheck_ = new QCheckBox(i18n("Restore lighting at login"), central);
    autostartCheck_->setChecked(QFile::exists(autostartFilePath()));
    autostartCheck_->setToolTip(i18n("Re-apply the active profile's lighting when you log in."));
    outer->addWidget(autostartCheck_);

    trayAutostartCheck_ = new QCheckBox(i18n("Start k-rgb in the system tray at login"), central);
    trayAutostartCheck_->setChecked(QFile::exists(trayAutostartFilePath()));
    trayAutostartCheck_->setToolTip(
        i18n("Launch k-rgb (hidden, to the system tray) when you log in, so the "
             "tray quick-switcher is always available."));
    outer->addWidget(trayAutostartCheck_);

    setCentralWidget(central);

    statusLabel_ = new QLabel(this);
    statusLabel_->setTextFormat(Qt::RichText);
    statusBar()->addPermanentWidget(statusLabel_);

    // --- Shared connections ------------------------------------------------
    connect(autostartCheck_, &QCheckBox::toggled, this, &MainWindow::onAutostartToggled);
    connect(trayAutostartCheck_, &QCheckBox::toggled, this, &MainWindow::onTrayAutostartToggled);
    connect(profileCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onProfileSelected);
    connect(newProfileBtn_, &QPushButton::clicked, this, &MainWindow::onNewProfile);
    connect(renameProfileBtn_, &QPushButton::clicked, this, &MainWindow::onRenameProfile);
    connect(deleteProfileBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteProfile);
}

QWidget* MainWindow::buildKeyboardPage() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    modeCombo_ = new QComboBox(page);
    for(const ModeEntry& m : modes_) {
        modeCombo_->addItem(m.name);
    }
    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(modeCombo_, 1);
    auto* perKeyButton = new QPushButton(QIcon::fromTheme(QStringLiteral("input-keyboard")),
                                         i18n("Per-Key Editor…"), page);
    perKeyButton->setToolTip(i18n("Paint individual keys — includes USA and Ukraine flag presets."));
    modeRow->addWidget(perKeyButton);
    form->addRow(i18n("Mode:"), modeRow);
    connect(perKeyButton, &QPushButton::clicked, this, [this] {
        for(int i = 0; i < modes_.size(); ++i) {
            if(modes_.at(i).perkey) { modeCombo_->setCurrentIndex(i); break; }
        }
    });

    colorButton_ = new KColorButton(QColor(0, 170, 255), page);
    form->addRow(i18n("Colour:"), colorButton_);

    speedCombo_ = new QComboBox(page);
    speedCombo_->addItem(i18n("Slow"),   static_cast<int>(Speed::Slowest));
    speedCombo_->addItem(i18n("Normal"), static_cast<int>(Speed::Normal));
    speedCombo_->addItem(i18n("Fast"),   static_cast<int>(Speed::Fastest));
    speedCombo_->setCurrentIndex(1);
    form->addRow(i18n("Speed:"), speedCombo_);

    directionCombo_ = new QComboBox(page);
    directionCombo_->addItem(i18n("Left"),  static_cast<int>(Direction::Left));
    directionCombo_->addItem(i18n("Right"), static_cast<int>(Direction::Right));
    directionCombo_->addItem(i18n("Up"),    static_cast<int>(Direction::Up));
    directionCombo_->addItem(i18n("Down"),  static_cast<int>(Direction::Down));
    form->addRow(i18n("Direction:"), directionCombo_);

    auto* brightnessRow = new QHBoxLayout();
    brightnessSlider_ = new QSlider(Qt::Horizontal, page);
    brightnessSlider_->setRange(0, 100);
    brightnessSlider_->setValue(100);
    brightnessValue_ = new QLabel(QStringLiteral("100%"), page);
    brightnessValue_->setMinimumWidth(40);
    brightnessRow->addWidget(brightnessSlider_);
    brightnessRow->addWidget(brightnessValue_);
    form->addRow(i18n("Brightness:"), brightnessRow);

    v->addLayout(form);

    // Per-key editor (shown only in Per-key mode).
    perKeyPanel_ = new QWidget(page);
    auto* pkLayout = new QVBoxLayout(perKeyPanel_);
    pkLayout->setContentsMargins(0, 0, 0, 0);

    keyboardWidget_ = new KeyboardWidget(perKeyPanel_);
    pkLayout->addWidget(keyboardWidget_, 1);

    auto* pkControls = new QHBoxLayout();
    selectionLabel_ = new QLabel(i18n("No keys selected"), perKeyPanel_);
    pkControls->addWidget(selectionLabel_);
    pkControls->addStretch();
    auto* selectAllBtn = new QPushButton(i18n("Select All"), perKeyPanel_);
    auto* paintBtn     = new QPushButton(i18n("Paint Selected"), perKeyPanel_);
    auto* offSelBtn    = new QPushButton(i18n("Off Selected"), perKeyPanel_);
    auto* fillBtn      = new QPushButton(i18n("Fill All"), perKeyPanel_);
    pkControls->addWidget(selectAllBtn);
    pkControls->addWidget(paintBtn);
    pkControls->addWidget(offSelBtn);
    pkControls->addWidget(fillBtn);
    pkLayout->addLayout(pkControls);

    auto* presetRow = new QHBoxLayout();
    presetRow->addWidget(new QLabel(i18n("Presets:"), perKeyPanel_));
    auto* usaBtn = new QPushButton(i18n("USA Flag"), perKeyPanel_);
    auto* ukrBtn = new QPushButton(i18n("Ukraine Flag"), perKeyPanel_);
    presetRow->addWidget(usaBtn);
    presetRow->addWidget(ukrBtn);
    presetRow->addStretch();
    pkLayout->addLayout(presetRow);

    connect(usaBtn, &QPushButton::clicked, this, [this] {
        keyboardWidget_->setKeyColors(
            makeUsaFlag(controller_->usesLogitechG810Iso105VisualLayout()));
        onPerKeyChanged();
    });
    connect(ukrBtn, &QPushButton::clicked, this, [this] {
        keyboardWidget_->setKeyColors(
            makeUkraineFlag(controller_->usesLogitechG810Iso105VisualLayout()));
        onPerKeyChanged();
    });

    auto* hint = new QLabel(
        i18n("Click keys to select; drag to box-select; Ctrl-click to add. "
             "Pick a colour above, then Paint Selected."),
        perKeyPanel_);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    pkLayout->addWidget(hint);

    perKeyPanel_->setVisible(false);
    v->addWidget(perKeyPanel_, 1);

    // Keyboard zone editor for HID++ devices that report lighting zones.
    keyboardZonePanel_ = new QWidget(page);
    auto* kzLayout = new QVBoxLayout(keyboardZonePanel_);
    kzLayout->setContentsMargins(0, 0, 0, 0);
    auto* kzHint = new QLabel(
        i18n("Each block controls one device-reported lighting zone."),
        keyboardZonePanel_);
    kzHint->setWordWrap(true);
    kzHint->setEnabled(false);
    kzLayout->addWidget(kzHint);

    keyboardZoneRow_ = new QHBoxLayout();
    kzLayout->addLayout(keyboardZoneRow_);
    keyboardZonePanel_->setVisible(false);
    v->addWidget(keyboardZonePanel_);

    v->addStretch();

    auto* buttons = new QHBoxLayout();
    offButton_ = new QPushButton(QIcon::fromTheme(QStringLiteral("system-shutdown")),
                                 i18n("Turn Off"), page);
    applyButton_ = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")),
                                   i18n("Apply"), page);
    applyButton_->setDefault(true);
    buttons->addWidget(offButton_);
    buttons->addStretch();
    buttons->addWidget(applyButton_);
    v->addLayout(buttons);

    connect(modeCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onModeChanged);
    connect(brightnessSlider_, &QSlider::valueChanged, this, &MainWindow::onBrightnessChanged);
    connect(applyButton_, &QPushButton::clicked, this, &MainWindow::onApply);
    connect(offButton_, &QPushButton::clicked, this, &MainWindow::onOff);
    connect(selectAllBtn, &QPushButton::clicked, keyboardWidget_, &KeyboardWidget::selectAll);
    connect(paintBtn, &QPushButton::clicked, this, &MainWindow::onPaintSelection);
    connect(offSelBtn, &QPushButton::clicked, this, &MainWindow::onOffSelection);
    connect(fillBtn, &QPushButton::clicked, this, &MainWindow::onFillAll);
    connect(keyboardWidget_, &KeyboardWidget::changed, this, &MainWindow::onPerKeyChanged);
    connect(keyboardWidget_, &KeyboardWidget::selectionChanged, this, [this](int count) {
        selectionLabel_->setText(count == 0 ? i18n("No keys selected")
                                            : i18np("%1 key selected", "%1 keys selected", count));
    });
    connect(colorButton_, &KColorButton::changed, this, [this]() {
        if(loading_) {
            return;
        }
        const ModeEntry& m = modes_.at(modeCombo_->currentIndex());
        if(m.perkey || m.zones) {
            return;  // not a live shared-colour source in these editors
        }
        if(controller_->isConnected()) {
            onApply();
        }
    });
    connect(brightnessSlider_, &QSlider::sliderReleased, this, [this]() {
        if(!loading_ && controller_->isConnected()) {
            onApply();
        }
    });
    return page;
}

QWidget* MainWindow::buildCasePage() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);

    // Whole-case quick controls.
    auto* caseRow = new QHBoxLayout();
    caseRow->addWidget(new QLabel(i18n("Whole-case colour:"), page));
    caseColorButton_ = new KColorButton(QColor(0, 90, 255), page);
    caseRow->addWidget(caseColorButton_);
    caseApplyButton_ = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")),
                                       i18n("Apply to All"), page);
    caseOffButton_ = new QPushButton(QIcon::fromTheme(QStringLiteral("system-shutdown")),
                                     i18n("Off"), page);
    caseRow->addWidget(caseApplyButton_);
    caseRow->addWidget(caseOffButton_);
    caseRow->addStretch();
    v->addLayout(caseRow);

    caseArrangeCheck_ = new QCheckBox(i18n("Arrange zones — drag them to match your case"), page);
    caseArrangeCheck_->setToolTip(
        i18n("When on, drag zones to mirror your case's physical layout (saved). "
             "When off, click/drag to select and paint."));
    v->addWidget(caseArrangeCheck_);

    zoneGrid_ = new ZoneGridWidget(page);
    v->addWidget(zoneGrid_, 1);

    auto* zoneControls = new QHBoxLayout();
    zoneSelectionLabel_ = new QLabel(i18n("No zones selected"), page);
    zoneControls->addWidget(zoneSelectionLabel_);
    zoneControls->addStretch();
    auto* zoneSelectAll = new QPushButton(i18n("Select All"), page);
    auto* zonePaint     = new QPushButton(i18n("Paint Selected"), page);
    auto* zoneOffSel    = new QPushButton(i18n("Off Selected"), page);
    auto* zoneFill      = new QPushButton(i18n("Fill All"), page);
    auto* zoneIdentify  = new QPushButton(i18n("Identify"), page);
    auto* zoneApply     = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")),
                                          i18n("Apply Zones"), page);
    for(QPushButton* b : {zoneSelectAll, zonePaint, zoneOffSel, zoneFill, zoneIdentify, zoneApply}) {
        zoneControls->addWidget(b);
    }
    v->addLayout(zoneControls);

    auto* zoneHint = new QLabel(
        i18n("Tip: turn on Arrange and drag zones to match your case, then turn it "
             "off to paint. Identify lights only the selected zones on the case so "
             "you can find them."),
        page);
    zoneHint->setWordWrap(true);
    zoneHint->setEnabled(false);
    v->addWidget(zoneHint);

    connect(caseApplyButton_, &QPushButton::clicked, this, &MainWindow::onCaseApply);
    connect(caseOffButton_, &QPushButton::clicked, this, &MainWindow::onCaseOff);
    connect(caseArrangeCheck_, &QCheckBox::toggled, this,
            [this](bool on) { zoneGrid_->setArrangeMode(on); });
    connect(zoneSelectAll, &QPushButton::clicked, zoneGrid_, &ZoneGridWidget::selectAll);
    connect(zonePaint, &QPushButton::clicked, this,
            [this] { zoneGrid_->paintSelection(caseColorButton_->color()); });
    connect(zoneOffSel, &QPushButton::clicked, zoneGrid_, &ZoneGridWidget::clearSelection);
    connect(zoneFill, &QPushButton::clicked, this,
            [this] { zoneGrid_->fillAll(caseColorButton_->color()); });
    connect(zoneIdentify, &QPushButton::clicked, this, &MainWindow::onCaseZoneIdentify);
    connect(zoneApply, &QPushButton::clicked, this, &MainWindow::onCaseZoneApply);
    connect(zoneGrid_, &ZoneGridWidget::layoutChanged, this, &MainWindow::saveZoneLayout);
    connect(zoneGrid_, &ZoneGridWidget::selectionChanged, this, [this](int count) {
        zoneSelectionLabel_->setText(count == 0 ? i18n("No zones selected")
                                                : i18np("%1 zone selected", "%1 zones selected", count));
    });
    return page;
}

void MainWindow::setupTray() {
    tray_ = new KStatusNotifierItem(QStringLiteral("krgb"), this);
    tray_->setTitle(i18n("k-rgb"));
    tray_->setIconByName(QStringLiteral("input-keyboard"));
    tray_->setToolTip(QStringLiteral("input-keyboard"), i18n("k-rgb"),
                      i18n("Alienware AW410K lighting"));
    tray_->setCategory(KStatusNotifierItem::ApplicationStatus);
    tray_->setStatus(KStatusNotifierItem::Active);
    tray_->setStandardActionsEnabled(false);

    connect(tray_, &KStatusNotifierItem::activateRequested, this, [this](bool, const QPoint&) {
        if(isVisible() && !isMinimized()) {
            hide();
        } else {
            show();
            setWindowState(windowState() & ~Qt::WindowMinimized);
            raise();
            activateWindow();
        }
    });

    auto* menu = new QMenu(this);

    profilesMenu_ = menu->addMenu(QIcon::fromTheme(QStringLiteral("document-multiple")),
                                  i18n("Profiles"));
    rebuildProfilesMenu();

    QAction* refreshAction = menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                           i18n("Refresh connection"));
    connect(refreshAction, &QAction::triggered, this, [this] {
        controller_->refresh();
        if(caseController_) {
            caseController_->refresh();
        }
        statusBar()->showMessage(i18n("Rescanning for devices…"), 2000);
    });

    menu->addSection(i18n("Quick lighting"));

    QAction* offAction = menu->addAction(QIcon::fromTheme(QStringLiteral("system-shutdown")), i18n("Off"));
    connect(offAction, &QAction::triggered, this, [this] { controller_->applyOff(); });

    QAction* rainbowAction = menu->addAction(i18n("Rainbow (static)"));
    connect(rainbowAction, &QAction::triggered, this,
            [this] { controller_->applyRainbow(brightnessSlider_->value()); });

    QAction* spectrumAction = menu->addAction(i18n("Spectrum"));
    connect(spectrumAction, &QAction::triggered, this, [this] {
        controller_->applyEffect(static_cast<int>(Mode::Spectrum),
                                 static_cast<int>(Speed::Normal),
                                 static_cast<int>(Direction::Left),
                                 Qt::white, brightnessSlider_->value());
    });

    menu->addSection(i18n("Solid colour"));
    const struct { QString name; QColor color; } presets[] = {
        { i18n("Red"),        QColor(255, 0, 0)     },
        { i18n("Green"),      QColor(0, 255, 0)     },
        { i18n("Blue"),       QColor(0, 90, 255)    },
        { i18n("Warm white"), QColor(255, 160, 70)  },
        { i18n("White"),      QColor(255, 255, 255) },
    };
    for(const auto& p : presets) {
        const QColor c = p.color;
        QAction* a = menu->addAction(p.name);
        connect(a, &QAction::triggered, this,
                [this, c] { controller_->applySolid(c, brightnessSlider_->value()); });
    }

    menu->addSeparator();
    QAction* showAction = menu->addAction(QIcon::fromTheme(QStringLiteral("settings-configure")),
                                          i18n("Open k-rgb…"));
    connect(showAction, &QAction::triggered, this, [this] {
        show();
        raise();
        activateWindow();
    });
    QAction* quitAction = menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18n("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    tray_->setContextMenu(menu);
}

void MainWindow::rebuildProfilesMenu() {
    if(!profilesMenu_) {
        return;
    }
    profilesMenu_->clear();
    delete profileGroup_;
    profileGroup_ = new QActionGroup(this);
    profileGroup_->setExclusive(true);

    const QString cur = Profiles::current();
    for(const QString& name : Profiles::names()) {
        QAction* a = profilesMenu_->addAction(name);
        a->setCheckable(true);
        a->setChecked(name == cur);
        profileGroup_->addAction(a);
        connect(a, &QAction::triggered, this, [this, name] {
            // Route through the combo so window + tray stay in sync.
            profileCombo_->setCurrentText(name);
        });
    }
}

// --- Profile management -----------------------------------------------------

void MainWindow::refreshProfileCombo() {
    const bool wasLoading = loading_;
    loading_ = true;
    profileCombo_->clear();
    profileCombo_->addItems(Profiles::names());
    profileCombo_->setCurrentText(Profiles::current());
    loading_ = wasLoading;
    deleteProfileBtn_->setEnabled(Profiles::names().size() > 1);
}

void MainWindow::switchToProfile(const QString& name, bool apply) {
    loading_ = true;
    const LightingSettings s = LightingSettings::load(name);
    loadProfileIntoUi(s);
    Profiles::setCurrent(name);
    loading_ = false;

    if(apply) {
        if(controller_->isConnected()) {
            s.apply(*controller_, caseController_);
        } else if(caseController_ && caseController_->isAvailable() && s.caseSet) {
            caseController_->applySolid(s.caseColor);  // keyboard absent, case present
        }
    }
    deleteProfileBtn_->setEnabled(Profiles::names().size() > 1);
    rebuildProfilesMenu();
}

void MainWindow::onProfileSelected(int index) {
    if(loading_ || index < 0) {
        return;
    }
    switchToProfile(profileCombo_->itemText(index), /*apply=*/true);
}

void MainWindow::onNewProfile() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, i18n("New Profile"),
                                               i18n("Profile name:"), QLineEdit::Normal,
                                               QString(), &ok).trimmed();
    if(!ok || name.isEmpty()) {
        return;
    }
    if(Profiles::exists(name)) {
        QMessageBox::warning(this, i18n("New Profile"),
                             i18n("A profile named “%1” already exists.", name));
        return;
    }
    Profiles::add(name, currentSettings());  // seed from current UI
    Profiles::setCurrent(name);
    refreshProfileCombo();
    rebuildProfilesMenu();
}

void MainWindow::onRenameProfile() {
    const QString from = Profiles::current();
    bool ok = false;
    const QString to = QInputDialog::getText(this, i18n("Rename Profile"),
                                             i18n("New name:"), QLineEdit::Normal,
                                             from, &ok).trimmed();
    if(!ok || to.isEmpty() || to == from) {
        return;
    }
    if(Profiles::exists(to)) {
        QMessageBox::warning(this, i18n("Rename Profile"),
                             i18n("A profile named “%1” already exists.", to));
        return;
    }
    Profiles::rename(from, to);
    refreshProfileCombo();
    rebuildProfilesMenu();
}

void MainWindow::onDeleteProfile() {
    const QString cur = Profiles::current();
    if(Profiles::names().size() <= 1) {
        return;
    }
    if(QMessageBox::question(this, i18n("Delete Profile"),
                             i18n("Delete the profile “%1”?", cur))
       != QMessageBox::Yes) {
        return;
    }
    Profiles::remove(cur);
    refreshProfileCombo();
    switchToProfile(Profiles::current(), /*apply=*/true);
}

// --- UI <-> settings --------------------------------------------------------

void MainWindow::loadProfileIntoUi(const LightingSettings& s) {
    int idx = 0;
    for(int i = 0; i < modes_.size(); ++i) {
        const ModeEntry& m = modes_.at(i);
        if(s.kind == LightingSettings::Solid && m.solid) { idx = i; break; }
        if(s.kind == LightingSettings::Rainbow && m.rainbow) { idx = i; break; }
        if(s.kind == LightingSettings::PerKey && m.perkey) { idx = i; break; }
        if(s.kind == LightingSettings::Zones && m.zones) { idx = i; break; }
        if(s.kind == LightingSettings::Effect && !m.solid && !m.rainbow && !m.perkey && !m.zones
           && m.value == s.effectMode) {
            idx = i;
            break;
        }
    }
    modeCombo_->setCurrentIndex(idx);
    colorButton_->setColor(s.color);
    const int si = speedCombo_->findData(s.speed);
    if(si >= 0) {
        speedCombo_->setCurrentIndex(si);
    }
    const int di = directionCombo_->findData(s.direction);
    if(di >= 0) {
        directionCombo_->setCurrentIndex(di);
    }
    brightnessSlider_->setValue(s.brightness);
    brightnessValue_->setText(QStringLiteral("%1%").arg(s.brightness));
    keyboardWidget_->setKeyColors(s.keyColors);
    for(int i = 0; i < keyboardZoneButtons_.size(); ++i) {
        const QColor z = s.keyboardZoneColors.value(i, QColor(0, 0, 0));
        keyboardZoneButtons_.at(i)->setColor(z);
    }
    if(s.caseColor.isValid()) {
        caseColorButton_->setColor(s.caseColor);
    }
    casePerZone_ = s.casePerZone;
    zoneGrid_->setZoneColors(s.caseZoneColors);
}

LightingSettings MainWindow::currentSettings() const {
    LightingSettings s;
    const ModeEntry& m = modes_.at(modeCombo_->currentIndex());
    if(m.solid) {
        s.kind = LightingSettings::Solid;
    } else if(m.rainbow) {
        s.kind = LightingSettings::Rainbow;
    } else if(m.perkey) {
        s.kind = LightingSettings::PerKey;
        s.keyColors = keyboardWidget_->keyColors();
    } else if(m.zones) {
        s.kind = LightingSettings::Zones;
        for(int i = 0; i < keyboardZoneButtons_.size(); ++i) {
            s.keyboardZoneColors.insert(i, keyboardZoneButtons_.at(i)->color());
        }
    } else {
        s.kind = LightingSettings::Effect;
        s.effectMode = m.value;
    }
    s.color = colorButton_->color();
    s.speed = speedCombo_->currentData().toInt();
    s.direction = directionCombo_->currentData().toInt();
    s.brightness = brightnessSlider_->value();
    s.caseSet = caseController_ && caseController_->isAvailable();
    s.casePerZone = casePerZone_;
    s.caseColor = caseColorButton_->color();
    s.caseZoneColors = zoneGrid_->zoneColors();
    return s;
}

void MainWindow::refreshKeyboardZoneEditor() {
    if(!keyboardZoneRow_) {
        return;
    }

    keyboardZoneButtons_.clear();
    while(QLayoutItem* item = keyboardZoneRow_->takeAt(0)) {
        if(QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const LightingSettings saved = LightingSettings::load(Profiles::current());
    const int count = controller_->supportsZoneColors() ? controller_->zoneCount() : 0;
    for(int zone = 0; zone < count; ++zone) {
        auto* columnWidget = new QWidget(keyboardZonePanel_);
        auto* column = new QVBoxLayout(columnWidget);
        column->setContentsMargins(0, 0, 0, 0);
        auto* label = new QLabel(i18n("Zone %1", zone + 1), columnWidget);
        label->setAlignment(Qt::AlignHCenter);
        auto* button = new KColorButton(
            saved.keyboardZoneColors.value(zone, QColor(0, 170, 255)),
            columnWidget);
        button->setToolTip(i18n("Colour for keyboard zone %1", zone + 1));
        keyboardZoneButtons_.push_back(button);
        column->addWidget(label);
        column->addWidget(button);
        keyboardZoneRow_->addWidget(columnWidget);

        connect(button, &KColorButton::changed, this, [this]() {
            if(loading_ || !controller_->isConnected()) {
                return;
            }
            const int idx = modeCombo_->currentIndex();
            if(idx >= 0 && idx < modes_.size() && modes_.at(idx).zones) {
                onApply();
            }
        });
    }
    keyboardZoneRow_->addStretch();
}

void MainWindow::onModeChanged() {
    const int idx = modeCombo_->currentIndex();
    if(idx < 0 || idx >= modes_.size()) {
        return;
    }
    const ModeEntry& m = modes_.at(idx);
    const bool advanced = controller_->supportsAdvancedModes();
    const bool perKey = controller_->supportsPerKeyColors();
    const bool rainbow = controller_->supportsStaticRainbow();
    const bool zoned = controller_->supportsZoneColors();
    const bool effectMode = !m.solid && !m.rainbow && !m.perkey && !m.zones;
    const bool supported =
        m.solid ||
        (m.rainbow && rainbow) ||
        (m.perkey && perKey) ||
        (m.zones && zoned) ||
        (effectMode && advanced);

    colorButton_->setEnabled(supported && m.usesColor);
    speedCombo_->setEnabled(supported && m.usesSpeed);
    directionCombo_->setEnabled(supported && m.usesDirection);
    applyButton_->setEnabled(controller_->isConnected() && supported);

    perKeyPanel_->setVisible(perKey && m.perkey);
    keyboardZonePanel_->setVisible(zoned && m.zones);
    if(perKey && m.perkey) {
        // Grow (never shrink) so the keyboard has room.
        resize(qMax(width(), 780), qMax(height(), 560));
    }

    if(controller_->isConnected() && !supported) {
        applyButton_->setToolTip(
            i18n("This mode is not available for the connected keyboard."));
    } else {
        applyButton_->setToolTip(QString());
    }
}

void MainWindow::onBrightnessChanged(int value) {
    brightnessValue_->setText(QStringLiteral("%1%").arg(value));
}

void MainWindow::onApply() {
    const int idx = modeCombo_->currentIndex();
    if(idx < 0 || idx >= modes_.size()) {
        return;
    }
    const LightingSettings s = currentSettings();
    s.apply(*controller_);
    s.save(Profiles::current());
}

void MainWindow::onOff() {
    controller_->applyOff();
}

// --- Per-key editor ---------------------------------------------------------

void MainWindow::onPaintSelection() {
    keyboardWidget_->paintSelection(colorButton_->color());
}

void MainWindow::onOffSelection() {
    keyboardWidget_->clearSelection();
}

void MainWindow::onFillAll() {
    keyboardWidget_->fillAll(colorButton_->color());
}

void MainWindow::onPerKeyChanged() {
    if(loading_) {
        return;
    }
    const LightingSettings s = currentSettings();
    if(controller_->isConnected()) {
        s.apply(*controller_);
    }
    s.save(Profiles::current());
}

// --- Case lighting ----------------------------------------------------------

void MainWindow::onCaseApply() {
    if(!caseController_ || !caseController_->isAvailable()) {
        return;
    }
    casePerZone_ = false;  // whole-case solid mode
    // The case apply is a multi-packet sequence (~1-2s); show a busy cursor.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    caseController_->applySolid(caseColorButton_->color());
    QApplication::restoreOverrideCursor();
    currentSettings().save(Profiles::current());  // persist into the active profile
}

void MainWindow::onCaseOff() {
    if(!caseController_) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    caseController_->applyOff();
    QApplication::restoreOverrideCursor();
}

void MainWindow::onCaseZoneApply() {
    if(!caseController_ || !caseController_->isAvailable()) {
        return;
    }
    casePerZone_ = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    caseController_->applyZones(zoneGrid_->zoneColors());
    QApplication::restoreOverrideCursor();
    currentSettings().save(Profiles::current());
}

void MainWindow::onCaseZoneIdentify() {
    if(!caseController_ || !caseController_->isAvailable()) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    caseController_->identify(zoneGrid_->selectedZones());  // selected zones white, rest off
    QApplication::restoreOverrideCursor();
}

void MainWindow::onCaseAvailabilityChanged(bool available) {
    if(!tabs_ || !casePage_) {
        return;
    }
    const int idx = tabs_->indexOf(casePage_);
    if(available && idx < 0) {
        tabs_->addTab(casePage_, QIcon::fromTheme(QStringLiteral("preferences-desktop-color")), i18n("Case"));
        if(caseController_) {
            zoneGrid_->setZoneCount(caseController_->zoneCount());
            tabs_->setTabToolTip(tabs_->indexOf(casePage_), caseController_->description());
        }
        loadZoneLayout();
    } else if(!available && idx >= 0) {
        tabs_->removeTab(idx);
    }
}

void MainWindow::saveZoneLayout() {
    KConfigGroup g(KSharedConfig::openConfig(), QStringLiteral("CaseLayout"));
    const QHash<int, QPoint> pos = zoneGrid_->positions();
    for(auto it = pos.cbegin(); it != pos.cend(); ++it) {
        g.writeEntry(QStringLiteral("z%1").arg(it.key()),
                     QStringLiteral("%1,%2").arg(it.value().x()).arg(it.value().y()));
    }
    g.sync();
}

void MainWindow::loadZoneLayout() {
    const KConfigGroup g(KSharedConfig::openConfig(), QStringLiteral("CaseLayout"));
    QHash<int, QPoint> pos;
    for(const QString& key : g.keyList()) {
        if(!key.startsWith(QLatin1Char('z'))) {
            continue;
        }
        bool ok = false;
        const int idx = key.mid(1).toInt(&ok);
        const QStringList parts = g.readEntry(key, QString()).split(QLatin1Char(','));
        if(ok && parts.size() == 2) {
            pos.insert(idx, QPoint(parts.at(0).toInt(), parts.at(1).toInt()));
        }
    }
    zoneGrid_->setPositions(pos);
}

// --- Autostart / status -----------------------------------------------------

void MainWindow::writeAutostartEntry(const QString& path, const QString& name, const QString& args) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    const QString exec = QCoreApplication::applicationFilePath() + args;
    QFile f(path);
    if(f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "[Desktop Entry]\n"
           << "Type=Application\n"
           << "Name=" << name << "\n"
           << "Exec=" << exec << "\n"
           << "Icon=input-keyboard\n"
           << "Terminal=false\n"
           << "NoDisplay=true\n"
           << "X-GNOME-Autostart-enabled=true\n";
    } else {
        onError(i18n("Could not write autostart entry to %1", path));
    }
}

void MainWindow::onAutostartToggled(bool checked) {
    const QString path = autostartFilePath();
    if(checked) {
        writeAutostartEntry(path, i18n("k-rgb (restore lighting)"), QStringLiteral(" --apply"));
    } else {
        QFile::remove(path);
    }
}

void MainWindow::onTrayAutostartToggled(bool checked) {
    const QString path = trayAutostartFilePath();
    if(checked) {
        writeAutostartEntry(path, i18n("k-rgb (system tray)"), QStringLiteral(" --tray"));
    } else {
        QFile::remove(path);
    }
}

QString MainWindow::autostartFilePath() const {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/autostart/krgb-restore.desktop");
}

QString MainWindow::trayAutostartFilePath() const {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/autostart/krgb-tray.desktop");
}

void MainWindow::onConnectionChanged(bool connected, const QString& path) {
    if(connected) {
        const QString model = controller_->modelName().isEmpty()
                                  ? i18n("Keyboard")
                                  : controller_->modelName();
        statusLabel_->setText(i18n("<span style='color:#27ae60'>●</span> %1", model));
        statusLabel_->setToolTip(i18n("Connected to %1 (%2)", model,
                                      path.isEmpty() ? i18n("unknown") : path));
        keyboardWidget_->setLayoutKind(
            controller_->usesLogitechG810Iso105VisualLayout()
                ? KeyboardWidget::LayoutKind::LogitechG810Iso105
                : KeyboardWidget::LayoutKind::Alienware);
        keyboardWidget_->setModelBit(controller_->modelBit());
        refreshKeyboardZoneEditor();

        if(!controller_->supportsAdvancedModes()) {
            const LightingSettings saved = LightingSettings::load(Profiles::current());
            bool selected = false;

            for(int i = 0; i < modes_.size(); ++i) {
                const auto& mode = modes_.at(i);
                if(saved.kind == LightingSettings::PerKey &&
                   controller_->supportsPerKeyColors() && mode.perkey) {
                    modeCombo_->setCurrentIndex(i);
                    selected = true;
                    break;
                }
                if(saved.kind == LightingSettings::Rainbow &&
                   controller_->supportsStaticRainbow() && mode.rainbow) {
                    modeCombo_->setCurrentIndex(i);
                    selected = true;
                    break;
                }
                if(saved.kind == LightingSettings::Zones &&
                   controller_->supportsZoneColors() && mode.zones) {
                    modeCombo_->setCurrentIndex(i);
                    selected = true;
                    break;
                }
            }

            if(!selected) {
                for(int i = 0; i < modes_.size(); ++i) {
                    if(modes_.at(i).solid) {
                        modeCombo_->setCurrentIndex(i);
                        break;
                    }
                }
            }
        }
    } else {
        statusLabel_->setText(i18n("<span style='color:#c0392b'>●</span> Not found"));
        statusLabel_->setToolTip(
            i18n("No supported keyboard found — check it's plugged in "
                 "and the udev rule is installed."));
    }

    modeCombo_->setEnabled(connected);
    applyButton_->setEnabled(connected);
    offButton_->setEnabled(connected);
    if(connected) {
        onModeChanged();
    } else {
        colorButton_->setEnabled(false);
        speedCombo_->setEnabled(false);
        directionCombo_->setEnabled(false);
        if(keyboardZonePanel_) {
            keyboardZonePanel_->setVisible(false);
        }
    }
}

void MainWindow::onError(const QString& message) {
    statusBar()->showMessage(i18n("Error: %1", message), 5000);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Closing hides to the system tray; quit via the tray menu (or Ctrl+Q).
    if(QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        event->accept();
        qApp->quit();
    }
}
