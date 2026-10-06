#include "lightmountkeyboardwidget.h"

#include "keyboardcontroller.h"
#include "keyboardwidget.h"
#include "settings.h"
#include "core/effect_code.h"
#include "core/lightmount_effects.h"
#include "core/lightmount_ansi_visual.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <KColorButton>
#include <KLocalizedString>

namespace {

constexpr int kPerKeyMode = -1;
constexpr int kStaticRainbowMode = -2;

int percentToSliderStep(int percent) {
    return (qBound(10, percent, 100) + 5) / 10;
}

int effectCode(krgb::LightMountEffect effect) {
    return krgb::makeProtocolEffectCode(
        krgb::EffectProtocol::BeQuietMount,
        static_cast<std::uint16_t>(effect));
}

bool isLightMountEffectCode(int code) {
    return krgb::isProtocolEffectCode(code) &&
           krgb::protocolFromEffectCode(code) == krgb::EffectProtocol::BeQuietMount;
}

krgb::LightMountEffect nativeEffect(int code) {
    return static_cast<krgb::LightMountEffect>(krgb::nativeEffectValue(code));
}

bool hasDirection(int code) {
    return isLightMountEffectCode(code) &&
        (nativeEffect(code) == krgb::LightMountEffect::ColorWave ||
         nativeEffect(code) == krgb::LightMountEffect::Matrix ||
         nativeEffect(code) == krgb::LightMountEffect::Tornado);
}

int validDirection(int code, int direction) {
    if(isLightMountEffectCode(code) && nativeEffect(code) == krgb::LightMountEffect::Tornado) {
        return direction == 4 || direction == 5 ? direction : 4;
    }
    return direction >= 0 && direction <= 3 ? direction : 3;
}

} // namespace

LightMountKeyboardWidget::LightMountKeyboardWidget(
    KeyboardController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    modeCombo_ = new QComboBox(this);
    modeCombo_->addItem(i18n("Static"),
                        effectCode(krgb::LightMountEffect::Static));
    modeCombo_->addItem(i18n("Color Wave"),
                        effectCode(krgb::LightMountEffect::ColorWave));
    modeCombo_->addItem(i18n("Tornado"),
                        effectCode(krgb::LightMountEffect::Tornado));
    modeCombo_->addItem(i18n("Breathing"),
                        effectCode(krgb::LightMountEffect::Breathing));
    modeCombo_->addItem(i18n("Reactive"),
                        effectCode(krgb::LightMountEffect::Reactive));
    modeCombo_->addItem(i18n("Matrix"),
                        effectCode(krgb::LightMountEffect::Matrix));
    modeCombo_->addItem(i18n("Per-key (Custom)"), kPerKeyMode);
    modeCombo_->addItem(i18n("Rainbow (Custom, static)"), kStaticRainbowMode);
    form->addRow(i18n("Mode:"), modeCombo_);

    colorModeCombo_ = new QComboBox(this);
    colorModeCombo_->addItem(
        i18n("Single colour"),
        static_cast<int>(krgb::LightMountColorMode::Single));
    colorModeCombo_->addItem(
        i18n("Dual colour"),
        static_cast<int>(krgb::LightMountColorMode::Dual));
    colorModeCombo_->addItem(
        i18n("Gradient (rainbow)"),
        static_cast<int>(krgb::LightMountColorMode::Gradient));
    form->addRow(i18n("Colour mode:"), colorModeCombo_);

    primaryColor_ = new KColorButton(QColor(230, 48, 0), this);
    form->addRow(i18n("Primary colour:"), primaryColor_);

    secondaryColor_ = new KColorButton(QColor(255, 255, 255), this);
    form->addRow(i18n("Secondary colour:"), secondaryColor_);

    directionCombo_ = new QComboBox(this);
    directionCombo_->addItem(i18n("Up"),
                             static_cast<int>(krgb::LightMountDirection::Up));
    directionCombo_->addItem(i18n("Down"),
                             static_cast<int>(krgb::LightMountDirection::Down));
    directionCombo_->addItem(i18n("Left"),
                             static_cast<int>(krgb::LightMountDirection::Left));
    directionCombo_->addItem(i18n("Right"),
                             static_cast<int>(krgb::LightMountDirection::Right));
    directionCombo_->addItem(i18n("Clockwise"),
                             static_cast<int>(krgb::LightMountDirection::Clockwise));
    directionCombo_->addItem(i18n("Counter-clockwise"),
                             static_cast<int>(krgb::LightMountDirection::CounterClockwise));
    form->addRow(i18n("Direction:"), directionCombo_);

    auto* speedRow = new QWidget(this);
    auto* speedLayout = new QHBoxLayout(speedRow);
    speedLayout->setContentsMargins(0, 0, 0, 0);
    speedSlider_ = new QSlider(Qt::Horizontal, speedRow);
    // Ten positions also constrain mouse dragging; singleStep alone only
    // changes keyboard/wheel increments and permits intermediate percentages.
    speedSlider_->setRange(1, 10);
    speedSlider_->setSingleStep(1);
    speedSlider_->setPageStep(1);
    speedSlider_->setTickInterval(1);
    speedSlider_->setTickPosition(QSlider::TicksBelow);
    speedSlider_->setValue(5);
    speedValue_ = new QLabel(QStringLiteral("50%"), speedRow);
    speedValue_->setMinimumWidth(32);
    speedLayout->addWidget(speedSlider_, 1);
    speedLayout->addWidget(speedValue_);
    form->addRow(i18n("Speed:"), speedRow);

    auto* brightnessRow = new QWidget(this);
    auto* brightnessLayout = new QHBoxLayout(brightnessRow);
    brightnessLayout->setContentsMargins(0, 0, 0, 0);
    brightnessSlider_ = new QSlider(Qt::Horizontal, brightnessRow);
    brightnessSlider_->setRange(1, 10);
    brightnessSlider_->setSingleStep(1);
    brightnessSlider_->setPageStep(1);
    brightnessSlider_->setTickInterval(1);
    brightnessSlider_->setTickPosition(QSlider::TicksBelow);
    brightnessSlider_->setValue(4);
    brightnessValue_ = new QLabel(QStringLiteral("40%"), brightnessRow);
    brightnessValue_->setMinimumWidth(40);
    brightnessLayout->addWidget(brightnessSlider_, 1);
    brightnessLayout->addWidget(brightnessValue_);
    form->addRow(i18n("Brightness:"), brightnessRow);

    root->addLayout(form);

    keyboard_ = new KeyboardWidget(this);
    keyboard_->setLayoutKind(KeyboardWidget::LayoutKind::BeQuietLightMountAnsi);
    root->addWidget(keyboard_, 1);

    auto* selectionRow = new QHBoxLayout();
    selectionLabel_ = new QLabel(i18n("No keys selected"), this);
    auto* selectAll = new QPushButton(i18n("Select All"), this);
    auto* paint = new QPushButton(i18n("Paint Selected"), this);
    auto* offSelected = new QPushButton(i18n("Off Selected"), this);
    auto* fillAll = new QPushButton(i18n("Fill All"), this);
    selectionRow->addWidget(selectionLabel_);
    selectionRow->addStretch();
    selectionRow->addWidget(selectAll);
    selectionRow->addWidget(paint);
    selectionRow->addWidget(offSelected);
    selectionRow->addWidget(fillAll);
    root->addLayout(selectionRow);

    presetPanel_ = new QWidget(this);
    auto* presets = new QHBoxLayout(presetPanel_);
    presets->setContentsMargins(0, 0, 0, 0);
    presets->addWidget(new QLabel(i18n("Presets:"), presetPanel_));
    auto* usaFlag = new QPushButton(i18n("USA Flag"), presetPanel_);
    auto* ukraineFlag = new QPushButton(i18n("Ukraine Flag"), presetPanel_);
    presets->addWidget(usaFlag);
    presets->addWidget(ukraineFlag);
    presets->addStretch();
    root->addWidget(presetPanel_);
    connect(usaFlag, &QPushButton::clicked, this, [this] { applyFlagPreset(false); });
    connect(ukraineFlag, &QPushButton::clicked, this, [this] { applyFlagPreset(true); });

    auto* actionRow = new QHBoxLayout();
    auto* offButton = new QPushButton(
        QIcon::fromTheme(QStringLiteral("system-shutdown")), i18n("Turn Off"), this);
    applyButton_ = new QPushButton(
        QIcon::fromTheme(QStringLiteral("dialog-ok-apply")), i18n("Apply"), this);
    applyButton_->setDefault(true);
    actionRow->addWidget(offButton);
    actionRow->addStretch();
    actionRow->addWidget(applyButton_);
    root->addLayout(actionRow);

    connect(modeCombo_, &QComboBox::currentIndexChanged,
            this, &LightMountKeyboardWidget::onModeChanged);
    connect(colorModeCombo_, &QComboBox::currentIndexChanged,
            this, &LightMountKeyboardWidget::updateControls);
    connect(speedSlider_, &QSlider::valueChanged, this, [this](int value) {
        speedValue_->setText(QStringLiteral("%1%").arg(value * 10));
        if(!loading_ && !speedSlider_->isSliderDown()) applyAndSave();
    });
    connect(brightnessSlider_, &QSlider::valueChanged, this, [this](int value) {
        brightnessValue_->setText(QStringLiteral("%1%").arg(value * 10));
        if(!loading_ && !brightnessSlider_->isSliderDown()) applyAndSave();
    });
    connect(speedSlider_, &QSlider::sliderReleased,
            this, &LightMountKeyboardWidget::applyAndSave);
    connect(brightnessSlider_, &QSlider::sliderReleased,
            this, &LightMountKeyboardWidget::applyAndSave);
    connect(applyButton_, &QPushButton::clicked,
            this, &LightMountKeyboardWidget::applyAndSave);
    connect(offButton, &QPushButton::clicked,
            controller_, &KeyboardController::applyOff);
    connect(selectAll, &QPushButton::clicked,
            keyboard_, &KeyboardWidget::selectAll);
    connect(paint, &QPushButton::clicked,
            this, &LightMountKeyboardWidget::paintSelection);
    connect(offSelected, &QPushButton::clicked,
            keyboard_, &KeyboardWidget::clearSelection);
    connect(fillAll, &QPushButton::clicked, this, [this]() {
        keyboard_->fillAll(primaryColor_->color());
    });
    connect(keyboard_, &KeyboardWidget::selectionChanged, this, [this](int count) {
        selectionLabel_->setText(count == 0
            ? i18n("No keys selected")
            : i18np("%1 key selected", "%1 keys selected", count));
    });
    connect(controller_, &KeyboardController::connectionChanged,
            this, [this](bool, const QString&) {
                applyButton_->setEnabled(controller_->usesBeQuietLightMount());
            });

    loadCurrentProfile();
    updateControls();
}

bool LightMountKeyboardWidget::isPerKeyMode() const {
    return modeCombo_->currentData().toInt() == kPerKeyMode;
}

void LightMountKeyboardWidget::onModeChanged() {
    if(loading_) return;
    if(hasDirection(activeEffectCode_)) {
        effectDirections_.insert(activeEffectCode_, directionCombo_->currentData().toInt());
    }
    activeEffectCode_ = selectedEffectCode();
    if(hasDirection(activeEffectCode_)) {
        const int direction = validDirection(activeEffectCode_,
            effectDirections_.value(activeEffectCode_, -1));
        directionCombo_->setCurrentIndex(directionCombo_->findData(direction));
    }
    updateControls();
}

bool LightMountKeyboardWidget::isRainbowMode() const {
    return modeCombo_->currentData().toInt() == kStaticRainbowMode;
}

int LightMountKeyboardWidget::selectedEffectCode() const {
    const int code = modeCombo_->currentData().toInt();
    return isLightMountEffectCode(code) ? code : 0;
}

void LightMountKeyboardWidget::setDirectionRows(bool cardinal, bool rotational) {
    auto* view = qobject_cast<QListView*>(directionCombo_->view());
    if(view) {
        for(int row = 0; row < 4; ++row) {
            view->setRowHidden(row, !cardinal);
        }
        for(int row = 4; row < 6; ++row) {
            view->setRowHidden(row, !rotational);
        }
    }

    const int current = directionCombo_->currentData().toInt();
    const bool currentCardinal = current >= 0 && current <= 3;
    const bool currentRotational = current >= 4 && current <= 5;
    if((cardinal && !currentCardinal) || (rotational && !currentRotational)) {
        directionCombo_->setCurrentIndex(cardinal ? 3 : 4); // Right / Clockwise
    }
}

void LightMountKeyboardWidget::updateControls() {
    if(loading_) {
        return;
    }

    const int code = selectedEffectCode();
    const bool perKey = isPerKeyMode();
    const bool rainbow = isRainbowMode();
    const bool general = code != 0;

    krgb::LightMountEffect effect = krgb::LightMountEffect::Static;
    if(general) {
        effect = nativeEffect(code);
    }

    const bool colorWave = general && effect == krgb::LightMountEffect::ColorWave;
    const bool tornado = general && effect == krgb::LightMountEffect::Tornado;
    const bool breathing = general && effect == krgb::LightMountEffect::Breathing;
    const bool reactive = general && effect == krgb::LightMountEffect::Reactive;
    const bool matrix = general && effect == krgb::LightMountEffect::Matrix;
    const bool staticEffect = general && effect == krgb::LightMountEffect::Static;

    colorModeCombo_->setEnabled(colorWave);
    const auto colorMode = static_cast<krgb::LightMountColorMode>(
        colorModeCombo_->currentData().toInt());

    const bool primary = staticEffect || perKey ||
        (colorWave && colorMode != krgb::LightMountColorMode::Gradient) || reactive;
    const bool secondary = reactive ||
        (colorWave && colorMode == krgb::LightMountColorMode::Dual);
    primaryColor_->setEnabled(primary);
    secondaryColor_->setEnabled(secondary);

    const bool speed = colorWave || tornado || breathing || reactive || matrix;
    speedSlider_->setEnabled(speed);
    speedValue_->setEnabled(speed);

    const bool cardinal = colorWave || matrix;
    const bool rotational = tornado;
    directionCombo_->setEnabled(cardinal || rotational);
    setDirectionRows(cardinal, rotational);

    keyboard_->setVisible(perKey);
    presetPanel_->setVisible(perKey);
    selectionLabel_->setVisible(perKey);
    for(auto* button : findChildren<QPushButton*>()) {
        const QString text = button->text();
        if(text == i18n("Select All") || text == i18n("Paint Selected") ||
           text == i18n("Off Selected") || text == i18n("Fill All")) {
            button->setVisible(perKey);
        }
    }

    if(rainbow) {
        primaryColor_->setEnabled(false);
        secondaryColor_->setEnabled(false);
    }

    applyButton_->setEnabled(controller_->usesBeQuietLightMount());
}

void LightMountKeyboardWidget::paintSelection() {
    keyboard_->paintSelection(primaryColor_->color());
}

void LightMountKeyboardWidget::applyFlagPreset(bool ukrainian) {
    if(!isPerKeyMode()) return;
    QHash<QString, QColor> colors;
    using namespace krgb::lightmount::ansi_visual;
    // Match the existing presets using Light Mount physical coordinates,
    // including the media wheel and individually addressable accent LEDs.
    for(const auto& key : keys()) {
        const float cx = key.x + key.w / 2.0f;
        const float cy = key.y + key.h / 2.0f;
        QColor color;
        if(ukrainian) {
            color = cy < kLayoutHeight / 2.0f
                ? QColor(0, 87, 183) : QColor(255, 215, 0);
        } else if(cx < kLayoutWidth / 3.0f) {
            color = QColor(28, 28, 75);
        } else {
            const int stripe = static_cast<int>(cy / (kLayoutHeight / 8.0f));
            color = stripe % 2 == 0 ? QColor(140, 22, 36) : QColor(255, 255, 255);
        }
        colors.insert(QString::fromLatin1(key.name), color);
    }
    keyboard_->setKeyColors(colors);
    applyAndSave();
}

void LightMountKeyboardWidget::loadCurrentProfile() {
    loading_ = true;
    const LightingSettings s = LightingSettings::load(Profiles::current());
    effectDirections_ = s.effectDirections;
    // Older profiles only stored the direction of the active effect. Migrate
    // that value for that effect alone; other modes keep their own defaults.
    if(s.kind == LightingSettings::Effect && hasDirection(s.effectMode) &&
       !effectDirections_.contains(s.effectMode)) {
        effectDirections_.insert(s.effectMode, validDirection(s.effectMode, s.direction));
    }

    int modeIndex = 0;
    if(s.kind == LightingSettings::PerKey) {
        modeIndex = modeCombo_->findData(kPerKeyMode);
    } else if(s.kind == LightingSettings::Rainbow) {
        modeIndex = modeCombo_->findData(kStaticRainbowMode);
    } else if(s.kind == LightingSettings::Effect && isLightMountEffectCode(s.effectMode)) {
        modeIndex = modeCombo_->findData(s.effectMode);
    } else if(s.kind == LightingSettings::Solid) {
        // Migrate the old shared-page solid profile to the Light Mount's native
        // General/Static effect without changing the saved profile until Apply.
        modeIndex = modeCombo_->findData(effectCode(krgb::LightMountEffect::Static));
    }
    if(modeIndex >= 0) {
        modeCombo_->setCurrentIndex(modeIndex);
    }

    primaryColor_->setColor(s.color);
    secondaryColor_->setColor(s.secondaryColor);

    int colorMode = s.effectColorMode;
    if(colorMode < static_cast<int>(krgb::LightMountColorMode::Single) ||
       colorMode > static_cast<int>(krgb::LightMountColorMode::Gradient)) {
        colorMode = static_cast<int>(krgb::LightMountColorMode::Single);
    }
    const int colorModeIndex = colorModeCombo_->findData(colorMode);
    if(colorModeIndex >= 0) {
        colorModeCombo_->setCurrentIndex(colorModeIndex);
    }

    int speed = s.speed;
    if(speed < 10 || speed > 100) {
        speed = 50;
    }
    speedSlider_->setValue(percentToSliderStep(speed));

    activeEffectCode_ = selectedEffectCode();
    const int direction = validDirection(activeEffectCode_,
        effectDirections_.value(activeEffectCode_, -1));
    const int directionIndex = directionCombo_->findData(direction);
    if(directionIndex >= 0) {
        directionCombo_->setCurrentIndex(directionIndex);
    }

    brightnessSlider_->setValue(percentToSliderStep(s.brightness));
    keyboard_->setKeyColors(s.keyColors);
    loading_ = false;
    updateControls();
}

void LightMountKeyboardWidget::applyAndSave() {
    if(loading_ || !controller_->usesBeQuietLightMount()) {
        return;
    }

    LightingSettings s = LightingSettings::load(Profiles::current());
    s.brightness = brightnessSlider_->value() * 10;
    s.color = primaryColor_->color();
    s.secondaryColor = secondaryColor_->color();

    if(isPerKeyMode()) {
        s.kind = LightingSettings::PerKey;
        s.keyColors = keyboard_->keyColors();
    } else if(isRainbowMode()) {
        s.kind = LightingSettings::Rainbow;
    } else {
        s.kind = LightingSettings::Effect;
        s.effectMode = selectedEffectCode();
        s.effectColorMode = colorModeCombo_->currentData().toInt();
        s.speed = speedSlider_->value() * 10;
        s.direction = directionCombo_->currentData().toInt();
        if(hasDirection(s.effectMode)) {
            effectDirections_.insert(s.effectMode, s.direction);
        }
        s.effectPeriodMs = 0;
    }
    s.effectDirections = effectDirections_;

    if(s.apply(*controller_)) {
        s.save(Profiles::current());
    }
}
