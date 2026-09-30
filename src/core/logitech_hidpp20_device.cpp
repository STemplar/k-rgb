#include "core/logitech_hidpp20_device.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace krgb {

LogitechHIDPP20Device::~LogitechHIDPP20Device() {
    close();
}

bool LogitechHIDPP20Device::parseHex(const std::string& value, int& out) {
    if(value.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(value.c_str(), &end, 16);
    if(end == value.c_str() || *end != '\0' || errno != 0) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

std::string LogitechHIDPP20Device::findG810DevicePath(std::uint16_t* productId) {
    if(productId) {
        *productId = 0;
    }

    const fs::path base = "/sys/class/hidraw";
    std::error_code ec;
    if(!fs::is_directory(base, ec)) {
        return {};
    }

    std::vector<std::string> names;
    for(const auto& entry : fs::directory_iterator(base, ec)) {
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());

    for(const auto& name : names) {
        const fs::path devlink = base / name / "device";
        std::ifstream uevent(devlink / "uevent");
        if(!uevent) {
            continue;
        }

        int vid = -1;
        int pid = -1;
        std::string line;
        while(std::getline(uevent, line)) {
            if(line.rfind("HID_ID=", 0) != 0) {
                continue;
            }
            const std::string body = line.substr(7);
            const auto p1 = body.find(':');
            const auto p2 = p1 == std::string::npos
                ? std::string::npos : body.find(':', p1 + 1);
            if(p1 != std::string::npos && p2 != std::string::npos) {
                parseHex(body.substr(p1 + 1, p2 - p1 - 1), vid);
                parseHex(body.substr(p2 + 1), pid);
            }
        }

        if(vid != kVendorId ||
           (pid != kG810ProductId1 && pid != kG810ProductId2)) {
            continue;
        }

        // g810-led's working libusb path claims interface 1 on the G810.
        // Confirm that interface here so the normal keyboard HID collection is
        // not mistaken for the HID++ endpoint.
        const fs::path real = fs::canonical(devlink, ec);
        if(ec) {
            continue;
        }

        int ifnum = -1;
        fs::path parent = real;
        for(int depth = 0; depth < 4 && !parent.empty(); ++depth) {
            std::ifstream interfaceFile(parent / "bInterfaceNumber");
            if(interfaceFile) {
                std::string value;
                interfaceFile >> value;
                parseHex(value, ifnum);
                break;
            }
            parent = parent.parent_path();
        }
        if(ifnum != kG810Interface) {
            continue;
        }

        if(productId) {
            *productId = static_cast<std::uint16_t>(pid);
        }
        return std::string("/dev/") + name;
    }

    return {};
}

bool LogitechHIDPP20Device::openG810(std::string* err) {
    std::uint16_t pid = 0;
    const std::string devicePath = findG810DevicePath(&pid);
    if(devicePath.empty()) {
        if(err) {
            *err = "No supported Logitech G810 HID++ interface found";
        }
        return false;
    }
    if(!openPath(devicePath, err)) {
        return false;
    }
    productId_ = pid;
    return true;
}

bool LogitechHIDPP20Device::openPath(const std::string& devicePath, std::string* err) {
    close();

    const int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if(fd < 0) {
        if(err) {
            *err = "open " + devicePath + ": " + std::strerror(errno);
        }
        return false;
    }

    fd_ = fd;
    path_ = devicePath;
    return true;
}

void LogitechHIDPP20Device::close() {
    if(fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    path_.clear();
    productId_ = 0;
}

bool LogitechHIDPP20Device::requestLong(
    std::uint8_t featureIndex, std::uint8_t function,
    const std::uint8_t* params, std::size_t paramCount,
    LongReport& response, std::string* err) {

    if(fd_ < 0 || paramCount > 16) {
        if(err) {
            *err = "invalid HID++ request";
        }
        return false;
    }

    LongReport request{};
    request[0] = kLongReportId;
    request[1] = kDefaultDeviceIndex;
    request[2] = featureIndex;
    request[3] = static_cast<std::uint8_t>((function << 4) | kSoftwareId);
    for(std::size_t i = 0; i < paramCount; ++i) {
        request[4 + i] = params[i];
    }

    // Drop stale asynchronous reports before issuing a synchronous request.
    std::uint8_t stale[64];
    while(::read(fd_, stale, sizeof(stale)) > 0) {
    }

    const ssize_t written = ::write(fd_, request.data(), request.size());
    if(written != static_cast<ssize_t>(request.size())) {
        if(err) {
            *err = "HID++ write failed: " + std::string(std::strerror(errno));
        }
        return false;
    }

    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    // HID++ replies normally arrive immediately. Allow a generous timeout for
    // loaded systems and ignore unrelated reports on the same hidraw node.
    for(int attempt = 0; attempt < 8; ++attempt) {
        const int ready = ::poll(&pfd, 1, 125);
        if(ready < 0) {
            if(errno == EINTR) {
                continue;
            }
            if(err) {
                *err = "HID++ poll failed: " + std::string(std::strerror(errno));
            }
            return false;
        }
        if(ready == 0) {
            continue;
        }

        std::uint8_t buffer[64]{};
        const ssize_t count = ::read(fd_, buffer, sizeof(buffer));
        if(count < 4) {
            continue;
        }

        const std::uint8_t reportId = buffer[0];
        if(reportId != kShortReportId &&
           reportId != kLongReportId &&
           reportId != kVeryLongReportId) {
            continue;
        }
        if(buffer[1] != kDefaultDeviceIndex) {
            continue;
        }

        // HID++ 2.0 mirrors the software ID in the response. Match both it and
        // the requested feature/function so asynchronous notifications cannot
        // be mistaken for our reply.
        if(buffer[2] != featureIndex ||
           (buffer[3] & 0x0f) != kSoftwareId ||
           (buffer[3] >> 4) != function) {
            continue;
        }

        response.fill(0);
        const std::size_t copyCount = std::min<std::size_t>(
            response.size(), static_cast<std::size_t>(count));
        std::copy(buffer, buffer + copyCount, response.begin());
        return true;
    }

    if(err) {
        *err = "HID++ request timed out";
    }
    return false;
}

bool LogitechHIDPP20Device::getProtocolVersion(
    std::uint8_t& major, std::uint8_t& minor, std::string* err) {

    // IRoot function 1. The two leading zero bytes plus a ping value follow
    // Logitech's HID++ 2.0 protocol-version request definition.
    constexpr std::uint8_t kPing = 0xa5;
    const std::uint8_t params[3] = {0x00, 0x00, kPing};

    LongReport response{};
    if(!requestLong(0x00, 0x01, params, sizeof(params), response, err)) {
        return false;
    }
    if(response[6] != kPing) {
        if(err) {
            *err = "HID++ protocol-version ping mismatch";
        }
        return false;
    }

    major = response[4];
    minor = response[5];
    return true;
}

bool LogitechHIDPP20Device::setSolid(
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::string* err) {

    LogitechHIDPP20FeatureInfo fx;
    if(!getFeature(kFeatureColorLedEffects, fx, err)) {
        return false;
    }
    if(fx.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        }
        return false;
    }

    // 0x8070 function 0: GetInfo. On this feature generation byte 0 is the
    // number of firmware lighting zones.
    LongReport infoResponse{};
    if(!requestLong(fx.index, 0x00, nullptr, 0, infoResponse, err)) {
        return false;
    }
    const std::uint8_t zoneCount = infoResponse[4];
    if(zoneCount == 0) {
        if(err) {
            *err = "HID++ 0x8070 reported zero lighting zones";
        }
        return false;
    }

    // Find a static-color effect in every zone. 0x8070 function 1 returns
    // zone metadata; function 2 returns effect metadata. Effect type 0x0001
    // is the static-color effect.
    std::vector<std::uint8_t> staticEffectIndex(zoneCount, 0xff);
    for(std::uint8_t zone = 0; zone < zoneCount; ++zone) {
        const std::uint8_t zoneQuery[2] = {zone, 0x00};
        LongReport zoneResponse{};
        if(!requestLong(fx.index, 0x01, zoneQuery, sizeof(zoneQuery),
                        zoneResponse, err)) {
            return false;
        }

        const std::uint8_t effectCount = zoneResponse[7];
        for(std::uint8_t effect = 0; effect < effectCount; ++effect) {
            const std::uint8_t effectQuery[4] = {zone, effect, 0x00, 0x00};
            LongReport effectResponse{};
            if(!requestLong(fx.index, 0x02, effectQuery, sizeof(effectQuery),
                            effectResponse, err)) {
                return false;
            }

            const std::uint16_t effectType =
                (static_cast<std::uint16_t>(effectResponse[6]) << 8) |
                 static_cast<std::uint16_t>(effectResponse[7]);
            if(effectType == 0x0001) {
                staticEffectIndex[zone] = effect;
                break;
            }
        }

        if(staticEffectIndex[zone] == 0xff) {
            if(err) {
                *err = "HID++ 0x8070 zone " + std::to_string(zone) +
                       " has no static-color effect";
            }
            return false;
        }
    }

    // 0x8070 function 8: SetSWControl(enabled, persist). Claim live software
    // control before changing zone effects.
    const std::uint8_t claim[2] = {0x01, 0x01};
    LongReport claimResponse{};
    if(!requestLong(fx.index, 0x08, claim, sizeof(claim), claimResponse, err)) {
        return false;
    }

    // 0x8070 function 3: SetEffectByIndex.
    // Payload: zone, effect index, 10-byte parameter block, persistence/power.
    // Static effect parameters are RGB followed by marker 0x02.
    for(std::uint8_t zone = 0; zone < zoneCount; ++zone) {
        std::uint8_t params[16]{};
        params[0] = zone;
        params[1] = staticEffectIndex[zone];
        params[2] = r;
        params[3] = g;
        params[4] = b;
        params[5] = (r || g || b) ? 0x02 : 0x00;
        params[12] = 0x00; // live/volatile, do not persist to onboard storage

        LongReport response{};
        if(!requestLong(fx.index, 0x03, params, sizeof(params), response, err)) {
            return false;
        }
    }

    return true;
}

bool LogitechHIDPP20Device::getFeature(
    std::uint16_t featureId, LogitechHIDPP20FeatureInfo& info,
    std::string* err) {

    const std::uint8_t params[2] = {
        static_cast<std::uint8_t>(featureId >> 8),
        static_cast<std::uint8_t>(featureId & 0xff),
    };

    LongReport response{};
    if(!requestLong(0x00, 0x00, params, sizeof(params), response, err)) {
        return false;
    }

    info.index = response[4];
    info.type = response[5];
    info.version = response[6];
    return true;
}

} // namespace krgb
