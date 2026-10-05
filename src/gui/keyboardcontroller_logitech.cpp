#include "keyboardcontroller.h"

#include <KLocalizedString>

bool KeyboardController::getLogitechColorLed8070Info(
    krgb::LogitechHIDPP20ColorLedInfo& info, QString* error) {
    if(!ensureOpen() || !usesLogitechHIDPP20()) {
        if(error) {
            *error = i18n("No Logitech HID++ 2.0 keyboard is connected.");
        }
        return false;
    }

    std::string err;
    if(!logitechDevice_.getColorLed8070Info(info, &err)) {
        if(error) {
            *error = QString::fromStdString(err);
        }
        return false;
    }
    return true;
}

bool KeyboardController::getLogitechRgbEffects8071Info(
    krgb::LogitechHIDPP20RgbEffectsInfo& info, QString* error) {
    if(!ensureOpen() || !usesLogitechHIDPP20()) {
        if(error) {
            *error = i18n("No Logitech HID++ 2.0 keyboard is connected.");
        }
        return false;
    }

    std::string err;
    if(!logitechDevice_.getRgbEffects8071Info(info, &err)) {
        if(error) {
            *error = QString::fromStdString(err);
        }
        return false;
    }
    return true;
}

bool KeyboardController::applyLogitechColorLed8070Effect(
    std::uint16_t effectId, const QColor& color,
    int periodMs, int intensity, int direction) {

    if(!ensureOpen() || !usesLogitechHIDPP20()) {
        Q_EMIT error(i18n("No Logitech HID++ 2.0 keyboard is connected."));
        return false;
    }

    if(g810KeyPressActive_) {
        stopG810KeyPressEffect();
    }

    const int boundedPeriod = qBound(1, periodMs, 65535);
    const int boundedIntensity = qBound(1, intensity, 100);
    const int boundedDirection = qBound(0, direction, 8);

    std::string err;
    if(!logitechDevice_.setColorLed8070Effect(
           effectId,
           static_cast<std::uint8_t>(color.red()),
           static_cast<std::uint8_t>(color.green()),
           static_cast<std::uint8_t>(color.blue()),
           static_cast<std::uint16_t>(boundedPeriod),
           static_cast<std::uint8_t>(boundedIntensity),
           static_cast<std::uint8_t>(boundedDirection),
           &err)) {
        Q_EMIT error(QString::fromStdString(err));
        return false;
    }
    return true;
}
