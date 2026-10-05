#pragma once

#include <cstdlib>
#include <string>
#include <utility>

#include "core/device_diagnostics.h"

namespace krgb {

bool buildLightMountDiagnostics(DeviceDiagnostics& diagnostics,
                                std::string* err = nullptr);

bool buildLogitechHIDPP20Diagnostics(DeviceDiagnostics& diagnostics,
                                     std::string* err = nullptr);

} // namespace krgb
