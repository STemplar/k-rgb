#include "keyboardcontroller.h"

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

    if(!lightMountDevice_.setLightingMode(krgb::LightMountLightingMode::General) ||
       !lightMountDevice_.setGeneralEffect(effect)) {
        Q_EMIT error(i18n("Failed to set Light Mount General effect."));
        return false;
    }
    return true;
}
