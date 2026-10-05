#include "logitechkeyboardwidget.h"

#include "keyboardcontroller.h"
#include "settings.h"
#include "core/effect_code.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <KColorButton>
#include <KLocalizedString>

#include <algorithm>
#include <set>

LogitechKeyboardWidget::LogitechKeyboardWidget(KeyboardController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    auto* v = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    modeCombo_ = new QComboBox(this);
    form->addRow(i18n("HID++ effect:"), modeCombo_);

    colorButton_ = new KColorButton(QColor(0, 220, 255), this);
    form->addRow(i18n("Colour:"), colorButton_);

    periodSpin_ = new QSpinBox(this);
    periodSpin_->setRange(1, 65535);
    periodSpin_->setSuffix(i18n(" ms"));
    periodSpin_->setValue(5000);
    form->addRow(i18n("Period:"), periodSpin_);

    directionCombo_ = new QComboBox(this);
    directionCombo_->addItem(i18n("Firmware default"), 0);
    directionCombo_->addItem(i18n("Horizontal"), 1);
    directionCombo_->addItem(i18n("Vertical"), 2);
    directionCombo_->addItem(i18n("Center-out"), 3);
    directionCombo_->addItem(i18n("Inward"), 4);
    directionCombo_->addItem(i18n("Outward"), 5);
    directionCombo_->addItem(i18n("Reverse horizontal"), 6);
    directionCombo_->addItem(i18n("Reverse vertical"), 7);
    directionCombo_->addItem(i18n("Center-in"), 8);
    form->addRow(i18n("Direction:"), directionCombo_);

    auto* intensityRow = new QHBoxLayout();
    intensitySlider_ = new QSlider(Qt::Horizontal, this);
    intensitySlider_->setRange(1, 100);
    intensitySlider_->setValue(100);
    intensityValue_ = new QLabel(QStringLiteral("100%"), this);
    intensityRow->addWidget(intensitySlider_);
    intensityRow->addWidget(intensityValue_);
    form->addRow(i18n("Intensity:"), intensityRow);

    v->addLayout(form);

    detailLabel_ = new QLabel(this);
    detailLabel_->setWordWrap(true);
    detailLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(detailLabel_);
    v->addStretch();

    auto* buttons = new QHBoxLayout();
    offButton_ = new QPushButton(i18n("Turn Off"), this);
    applyButton_ = new QPushButton(i18n("Apply"), this);
    buttons->addWidget(offButton_);
    buttons->addStretch();
    buttons->addWidget(applyButton_);
    v->addLayout(buttons);

    connect(modeCombo_, &QComboBox::currentIndexChanged,
            this, &LogitechKeyboardWidget::updateControls);
    connect(intensitySlider_, &QSlider::valueChanged, this, [this](int value) {
        intensityValue_->setText(QStringLiteral("%1%").arg(value));
    });
    connect(applyButton_, &QPushButton::clicked,
            this, &LogitechKeyboardWidget::applyAndSave);
    connect(offButton_, &QPushButton::clicked, this, [this] {
        controller_->applyOff();
    });

    refreshFromDevice();
}

QString LogitechKeyboardWidget::effectName(std::uint16_t id) const {
    switch(id) {
        case 0: return i18n("Disabled");
        case 1: return i18n("Fixed color");
        case 2: return i18n("Pulsing / Breathing");
        case 3: return i18n("Color Cycling");
        case 4: return i18n("Color Wave");
        case 5: return i18n("Starlight");
        case 6: return i18n("Light on Press");
        case 7: return i18n("Audio Visualizer");
        case 8: return i18n("Boot Up");
        case 9: return i18n("Demo Mode");
        case 10: return i18n("Pulsing / Breathing (Waveform)");
        case 11: return i18n("Ripple");
        case 12: return i18n("Custom Onboard Stored Effect");
        case 13: return i18n("KITT lighting");
        case 14: return i18n("Color decomposition");
        case 19: return i18n("Host streaming");
        case 20: return i18n("HSV Pulsing / Breathing");
        case 21: return i18n("Color Cycling Configurable S");
        case 22: return i18n("Color Wave Configurable S");
        case 23: return i18n("Ripple Configurable S");
        default: return i18n("Effect 0x%1").arg(id, 4, 16, QLatin1Char('0'));
    }
}

bool LogitechKeyboardWidget::effectIsImplemented(std::uint16_t id) const {
    return id == 1 || id == 3 || id == 4 || id == 5 || id == 10;
}

void LogitechKeyboardWidget::refreshFromDevice() {
    effects_.clear();
    if(!controller_->usesLogitechHIDPP20()) {
        rebuildEffectList();
        return;
    }

    krgb::LogitechHIDPP20ColorLedInfo info;
    QString error;
    if(controller_->getLogitechColorLed8070Info(info, &error)) {
        std::set<std::uint16_t> seen;
        for(const auto& zone : info.zones) {
            for(const auto& effect : zone.effects) {
                if(seen.insert(effect.effectId).second) {
                    effects_.push_back({effect.effectId,
                                        effect.capabilities,
                                        effect.periodMs});
                }
            }
        }
        std::sort(effects_.begin(), effects_.end(), [](const auto& a, const auto& b) {
            return a.id < b.id;
        });
    } else {
        krgb::LogitechHIDPP20RgbEffectsInfo rgb;
        QString rgbError;
        if(controller_->getLogitechRgbEffects8071Info(rgb, &rgbError)) {
            std::set<std::uint16_t> seen;
            for(const auto& cluster : rgb.clusters) {
                for(const auto& effect : cluster.effects) {
                    if(seen.insert(effect.effectId).second) {
                        effects_.push_back({effect.effectId,
                                            effect.capabilities,
                                            effect.periodMs});
                    }
                }
            }
            detailLabel_->setText(i18n("This keyboard exposes HID++ 0x8071 RGB Effects. Read-only discovery is available; write support for 0x8071 is not implemented yet."));
        } else {
            detailLabel_->setText(error.isEmpty() ? rgbError : error);
        }
    }

    rebuildEffectList();
    loadCurrentProfile();
}

void LogitechKeyboardWidget::rebuildEffectList() {
    loading_ = true;
    modeCombo_->clear();
    modeCombo_->addItem(i18n("Solid colour"), -1);
    if(controller_->supportsPerKeyColors()) {
        modeCombo_->addItem(i18n("Per-key (custom)"), -2);
        modeCombo_->addItem(i18n("Rainbow (static)"), -3);
    }
    for(const auto& effect : effects_) {
        modeCombo_->addItem(effectName(effect.id), static_cast<int>(effect.id));
    }
    loading_ = false;
    updateControls();
}

void LogitechKeyboardWidget::updateControls() {
    const int value = modeCombo_->currentData().toInt();
    const bool effect = value >= 0;
    const bool implemented = !effect || effectIsImplemented(static_cast<std::uint16_t>(value));

    bool color = value == -1 || value == 1 || value == 5 || value == 10;
    bool period = value == 3 || value == 4 || value == 10;
    bool direction = value == 4;

    colorButton_->setEnabled(color);
    periodSpin_->setEnabled(period);
    directionCombo_->setEnabled(direction);
    intensitySlider_->setEnabled(effect && value != 1);
    applyButton_->setEnabled(controller_->isConnected() && implemented);

    if(effect && !implemented) {
        detailLabel_->setText(i18n("The device reports this HID++ effect, but k-rgb does not yet implement its parameter writer."));
    } else if(effect) {
        for(const auto& e : effects_) {
            if(e.id == static_cast<std::uint16_t>(value)) {
                detailLabel_->setText(i18n("HID++ 0x8070 effect ID 0x%1 · capabilities 0x%2 · device period %3 ms")
                    .arg(e.id, 4, 16, QLatin1Char('0'))
                    .arg(e.capabilities, 4, 16, QLatin1Char('0'))
                    .arg(e.periodMs));
                if(e.periodMs > 0 && loading_) {
                    periodSpin_->setValue(e.periodMs);
                }
                break;
            }
        }
    } else {
        detailLabel_->setText(i18n("Static and per-key modes use the HID++ lighting capabilities exposed by this keyboard."));
    }
}

void LogitechKeyboardWidget::applyAndSave() {
    if(loading_) {
        return;
    }

    const int value = modeCombo_->currentData().toInt();
    LightingSettings s = LightingSettings::load(Profiles::current());

    bool ok = false;
    if(value == -1) {
        s.kind = LightingSettings::Solid;
        s.color = colorButton_->color();
        s.brightness = intensitySlider_->value();
        ok = controller_->applySolid(s.color, s.brightness);
    } else if(value == -3) {
        s.kind = LightingSettings::Rainbow;
        s.brightness = intensitySlider_->value();
        ok = controller_->applyRainbow(s.brightness);
    } else if(value == -2) {
        detailLabel_->setText(i18n("Use the per-key editor on a supported Logitech layout."));
        return;
    } else {
        const auto id = static_cast<std::uint16_t>(value);
        if(!effectIsImplemented(id)) {
            return;
        }
        s.kind = LightingSettings::Effect;
        s.effectMode = krgb::makeProtocolEffectCode(krgb::EffectProtocol::LogitechHIDPP2, id);
        s.color = colorButton_->color();
        s.effectPeriodMs = periodSpin_->value();
        s.brightness = intensitySlider_->value();
        s.direction = directionCombo_->currentData().toInt();

        if(id == 1) {
            ok = controller_->applySolid(s.color, s.brightness);
        } else {
            ok = controller_->applyLogitechColorLed8070Effect(
                id, s.color, s.effectPeriodMs, s.brightness, s.direction);
        }
    }

    if(ok) {
        s.save(Profiles::current());
    }
}

void LogitechKeyboardWidget::loadCurrentProfile() {
    if(!controller_->usesLogitechHIDPP20() || modeCombo_->count() == 0) {
        return;
    }

    const LightingSettings s = LightingSettings::load(Profiles::current());
    loading_ = true;

    int wanted = -1;
    if(s.kind == LightingSettings::Rainbow) {
        wanted = -3;
    } else if(s.kind == LightingSettings::PerKey) {
        wanted = -2;
    } else if(s.kind == LightingSettings::Effect && krgb::isProtocolEffectCode(s.effectMode) &&
              krgb::protocolFromEffectCode(s.effectMode) == krgb::EffectProtocol::LogitechHIDPP2) {
        wanted = static_cast<int>(krgb::nativeEffectValue(s.effectMode));
    }

    const int idx = modeCombo_->findData(wanted);
    modeCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
    if(s.color.isValid()) {
        colorButton_->setColor(s.color);
    }
    if(s.effectPeriodMs > 0) {
        periodSpin_->setValue(qBound(1, s.effectPeriodMs, 65535));
    }
    intensitySlider_->setValue(qBound(1, s.brightness, 100));
    const int di = directionCombo_->findData(s.direction);
    if(di >= 0) {
        directionCombo_->setCurrentIndex(di);
    }

    loading_ = false;
    updateControls();
}
