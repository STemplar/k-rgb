#include "core/alienfx_device.h"

#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace krgb {

namespace {

void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool parseHex(const std::string& s, int& out) {
    if(s.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(s.c_str(), &end, 16);
    if(end == s.c_str() || errno != 0) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

// AW-ELC commands (subset; see OpenRGB AlienwareController).
constexpr std::uint8_t kCmdReport      = 0x20;
constexpr std::uint8_t kCmdUserAnim    = 0x21;
constexpr std::uint8_t kCmdSelectZones = 0x23;
constexpr std::uint8_t kCmdAddAction   = 0x24;
constexpr std::uint8_t kCmdReset       = 0x28;

constexpr std::uint8_t kReportConfig   = 0x02;
constexpr std::uint8_t kReportFirmware = 0x00;

constexpr std::uint16_t kAnimNew        = 0x0001;
constexpr std::uint16_t kAnimFinishPlay = 0x0003;
constexpr std::uint16_t kAnimSlotTemp   = 0xFFFF;  // non-saved animation slot

constexpr std::uint8_t  kModeColor   = 0x00;
constexpr std::uint16_t kColorDur    = 2000;    // 0x07D0
constexpr std::uint16_t kTempoMax    = 0x00FA;

} // namespace

AlienFXDevice::~AlienFXDevice() {
    close();
}

std::string AlienFXDevice::findDevicePath() {
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
        std::ifstream uevent(base / name / "device" / "uevent");
        if(!uevent) {
            continue;
        }
        int vid = -1, pid = -1;
        std::string line;
        while(std::getline(uevent, line)) {
            if(line.rfind("HID_ID=", 0) != 0) {
                continue;
            }
            const std::string body = line.substr(7);
            const auto p1 = body.find(':');
            const auto p2 = (p1 == std::string::npos) ? std::string::npos : body.find(':', p1 + 1);
            if(p1 != std::string::npos && p2 != std::string::npos) {
                int v = -1, p = -1;
                if(parseHex(body.substr(p1 + 1, p2 - p1 - 1), v) && parseHex(body.substr(p2 + 1), p)) {
                    vid = v;
                    pid = p;
                }
            }
        }
        if(vid == kVendorId && (pid == 0x0550 || pid == 0x0551)) {
            return std::string("/dev/") + name;
        }
    }
    return {};
}

bool AlienFXDevice::open(std::string* err) {
    const std::string p = findDevicePath();
    if(p.empty()) {
        if(err) {
            *err = "Alienware lighting controller (187c:0550) not found.";
        }
        return false;
    }
    if(!openPath(p, err)) {
        return false;
    }
    queryConfig();  // best effort; zoneCount_ may stay 0 on an odd controller
    return true;
}

bool AlienFXDevice::openPath(const std::string& path, std::string* err) {
    close();
    const int fd = ::open(path.c_str(), O_RDWR);
    if(fd < 0) {
        if(err) {
            *err = "open " + path + ": " + std::strerror(errno) +
                   " (install packaging/udev/60-alienware-rgb.rules, or run as root)";
        }
        return false;
    }
    fd_ = fd;
    path_ = path;
    return true;
}

void AlienFXDevice::close() {
    if(fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    zoneCount_ = 0;
    firmware_.clear();
}

bool AlienFXDevice::sendFeature(const Buf& buf) {
    if(fd_ < 0) {
        return false;
    }
    return ::ioctl(fd_, HIDIOCSFEATURE(static_cast<int>(buf.size())), buf.data()) >= 0;
}

bool AlienFXDevice::getFeature(Buf& buf) {
    if(fd_ < 0) {
        return false;
    }
    buf.fill(0);  // buf[0] = report id (0)
    return ::ioctl(fd_, HIDIOCGFEATURE(static_cast<int>(buf.size())), buf.data()) >= 0;
}

bool AlienFXDevice::transact(const Buf& out, Buf& resp, bool slow) {
    const bool ok = sendFeature(out);
    // The controller dislikes being spammed; pause (longer after play/save).
    sleepMs(slow ? 1000 : 60);
    getFeature(resp);
    return ok;
}

bool AlienFXDevice::queryConfig() {
    Buf out{}, resp{};
    out[0x01] = 0x03; out[0x02] = kCmdReport; out[0x03] = kReportConfig;
    if(!transact(out, resp)) {
        return false;
    }
    // Zone count is reported in byte 6 of the config response (matches OpenRGB).
    zoneCount_ = resp[0x06];

    out.fill(0);
    out[0x01] = 0x03; out[0x02] = kCmdReport; out[0x03] = kReportFirmware;
    if(transact(out, resp)) {
        firmware_ = std::to_string(resp[0x04]) + '.' + std::to_string(resp[0x05]) +
                    '.' + std::to_string(resp[0x06]);
    }
    return true;
}

bool AlienFXDevice::beginAnimation() {
    Buf out{}, resp{};
    out[0x01] = 0x03; out[0x02] = kCmdUserAnim;
    out[0x03] = kAnimNew >> 8;       out[0x04] = kAnimNew & 0xFF;
    out[0x05] = kAnimSlotTemp >> 8;  out[0x06] = kAnimSlotTemp & 0xFF;
    return transact(out, resp);
}

bool AlienFXDevice::finishPlay() {
    Buf out{}, resp{};
    out[0x01] = 0x03; out[0x02] = kCmdUserAnim;
    out[0x03] = kAnimFinishPlay >> 8;  out[0x04] = kAnimFinishPlay & 0xFF;
    out[0x05] = kAnimSlotTemp >> 8;    out[0x06] = kAnimSlotTemp & 0xFF;
    return transact(out, resp, /*slow=*/true);
}

bool AlienFXDevice::selectZones(const std::vector<std::uint8_t>& zones) {
    Buf out{}, resp{};
    out[0x01] = 0x03; out[0x02] = kCmdSelectZones;
    out[0x03] = 0x01;                                       // loop flag (always 1)
    out[0x05] = static_cast<std::uint8_t>(zones.size());    // count (big-endian)
    for(std::size_t i = 0; i < zones.size() && i < 28; ++i) {
        out[0x06 + i] = zones[i];
    }
    return transact(out, resp);
}

bool AlienFXDevice::addColorAction(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Buf out{}, resp{};
    out[0x01] = 0x03; out[0x02] = kCmdAddAction;
    out[0x03] = kModeColor;
    out[0x04] = kColorDur >> 8;  out[0x05] = kColorDur & 0xFF;
    out[0x06] = kTempoMax >> 8;  out[0x07] = kTempoMax & 0xFF;
    out[0x08] = r; out[0x09] = g; out[0x0A] = b;
    return transact(out, resp);
}

bool AlienFXDevice::setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if(zoneCount_ <= 0) {
        return false;
    }
    return setZoneColors(std::vector<ZoneColor>(zoneCount_, ZoneColor{r, g, b}));
}

bool AlienFXDevice::setZoneColors(const std::vector<ZoneColor>& colors) {
    if(fd_ < 0 || zoneCount_ <= 0) {
        return false;
    }
    // Group zones by colour so identical zones share one action (a SelectZones
    // packet holds at most 28 ids, so each group is sent in batches of 28).
    std::map<ZoneColor, std::vector<std::uint8_t>> groups;
    for(int z = 0; z < zoneCount_; ++z) {
        const ZoneColor c = (static_cast<std::size_t>(z) < colors.size()) ? colors[z]
                                                                          : ZoneColor{0, 0, 0};
        groups[c].push_back(static_cast<std::uint8_t>(z));
    }

    if(!beginAnimation()) {
        return false;
    }
    constexpr std::size_t kBatch = 28;
    for(const auto& [color, zones] : groups) {
        for(std::size_t i = 0; i < zones.size(); i += kBatch) {
            const std::vector<std::uint8_t> batch(
                zones.begin() + i, zones.begin() + std::min(i + kBatch, zones.size()));
            if(!selectZones(batch) || !addColorAction(color[0], color[1], color[2])) {
                return false;
            }
        }
    }
    return finishPlay();
}

bool AlienFXDevice::reset() {
    Buf out{}, resp{};
    out[0x01] = 0x03; out[0x02] = kCmdReset;
    return transact(out, resp, /*slow=*/true);
}

} // namespace krgb
