#include "keyboardcontroller.h"
#include "core/hid_lamp_array_device.h"

#include <chrono>
#include <thread>

#include <KLocalizedString>

bool KeyboardController::applyLightMountGeneralEffect(
    const krgb::LightMountGeneralEffect& effect) {

    if(g810KeyPressActive_) {
        stopG810KeyPressEffect();
    }
    if(!ensureOpen() || !usesBeQuietLightMount()) {
        Q_EMIT error(i18n("The connected keyboard is not a be quiet! Light Mount."));
        return false;
    }

    // Match the CLI native-effect path: release LampArray host control before
    // selecting a firmware effect on the separate vendor interface.
    krgb::HIDLampArrayDevice lamp;
    std::string err;
    if(!lamp.open(krgb::LightMountDevice::kVendorId,
                  krgb::LightMountDevice::kProductId, 3, &err)) {
        Q_EMIT error(QString::fromStdString(err));
        return false;
    }
    if(!lamp.setAutonomousMode(true)) {
        Q_EMIT error(i18n("Failed to enable Light Mount autonomous lighting."));
        return false;
    }
    if(!lightMountDevice_.setLightingMode(krgb::LightMountLightingMode::General)) {
        Q_EMIT error(QString::fromStdString(lightMountDevice_.lastError()));
        return false;
    }
    // Preserve the settling interval already used by the CLI.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(!lightMountDevice_.setGeneralEffect(effect)) {
        Q_EMIT error(QString::fromStdString(lightMountDevice_.lastError()));
        return false;
    }
    return true;
}
