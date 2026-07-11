#include "core/aw410k_device.h"

#include "core/keymap.h"

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
#include <thread>

namespace fs = std::filesystem;

namespace krgb {

namespace {
void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Non-throwing hex parser (the core is compiled with -fno-exceptions under
// KDE's compiler settings, so std::stoi's exceptions are unavailable).
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
} // namespace

AW410KDevice::~AW410KDevice() {
    close();
}

std::string AW410KDevice::findDevice(const KeyboardModel** outModel) {
    if(outModel) {
        *outModel = nullptr;
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

        // HID_ID line: HID_ID=0003:000004F2:00001968 (bus:vid:pid, hex).
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
                if(parseHex(body.substr(p1 + 1, p2 - p1 - 1), v) &&
                   parseHex(body.substr(p2 + 1), p)) {
                    vid = v;
                    pid = p;
                }
            }
        }

        // Match against every supported model.
        const KeyboardModel* match = nullptr;
        for(std::size_t i = 0; i < kModelCount; ++i) {
            if(vid == kModels[i].vendorId && pid == kModels[i].productId) {
                match = &kModels[i];
                break;
            }
        }
        if(!match) {
            continue;
        }

        // Confirm this is the lighting interface. The interface dir is an
        // ancestor of the resolved HID device node.
        const fs::path real = fs::canonical(devlink, ec);
        int ifnum = -1;
        for(const fs::path& parent : {real.parent_path(), real.parent_path().parent_path()}) {
            std::ifstream bi(parent / "bInterfaceNumber");
            if(bi) {
                std::string s;
                bi >> s;
                int n = -1;
                if(parseHex(s, n)) {
                    ifnum = n;
                }
                break;
            }
        }

        if(ifnum < 0 || ifnum == match->lightingInterface) {
            if(outModel) {
                *outModel = match;
            }
            return std::string("/dev/") + name;
        }
    }
    return {};
}

bool AW410KDevice::open(std::string* err) {
    const KeyboardModel* m = nullptr;
    const std::string p = findDevice(&m);
    if(p.empty()) {
        if(err) {
            *err = "No supported Alienware keyboard found (is it plugged in?)";
        }
        return false;
    }
    if(!openPath(p, err)) {
        return false;
    }
    model_ = m;
    return true;
}

bool AW410KDevice::openPath(const std::string& path, std::string* err) {
    close();
    const int fd = ::open(path.c_str(), O_RDWR);
    if(fd < 0) {
        if(err) {
            *err = "open " + path + ": " + std::strerror(errno) +
                   " (install packaging/udev/60-alienware-rgb.rules, or run as root)";
            if(errno == EACCES || errno == EPERM) {
                *err += "\nThe udev rule only takes effect for devices connected after installation."
                        "\nFix: unplug and reconnect the keyboard, or run:\n"
                        "  sudo udevadm control --reload-rules && "
                        "sudo udevadm trigger --subsystem-match=hidraw";
            }
        }
        return false;
    }
    fd_ = fd;
    path_ = path;
    return true;
}

void AW410KDevice::close() {
    if(fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    model_ = nullptr;
}

bool AW410KDevice::writeReport(const Report& buf) {
    if(fd_ < 0) {
        return false;
    }
    const ssize_t n = ::write(fd_, buf.data(), buf.size());
    return n == static_cast<ssize_t>(buf.size());
}

bool AW410KDevice::sendFeature(const Report& buf) {
    if(fd_ < 0) {
        return false;
    }
    return ::ioctl(fd_, HIDIOCSFEATURE(static_cast<int>(buf.size())), buf.data()) >= 0;
}

bool AW410KDevice::commit() {
    Report b{};
    b[0x01] = 0x05; b[0x02] = 0x01; b[0x0A] = 0x10; b[0x0B] = 0x0A;
    b[0x0C] = 0x01; b[0x0D] = 0x02; b[0x0E] = 0x01;
    const bool ok = writeReport(b);
    sleepMs(20);
    return ok;
}

bool AW410KDevice::initialize() {
    Report b{};
    b[0x01] = 0x0E; b[0x02] = 0x01; b[0x03] = 0x00; b[0x04] = 0x01; b[0x05] = 0xAD;
    b[0x06] = 0x80; b[0x07] = 0x10; b[0x08] = 0xA5; b[0x0A] = 0x0A; b[0x12] = 0x01;
    const bool ok = writeReport(b);
    sleepMs(2);
    return ok;
}

bool AW410KDevice::featureReport(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    Report buf{};
    buf[0x01] = a; buf[0x02] = b; buf[0x03] = c; buf[0x04] = d;
    const bool ok = sendFeature(buf);
    sleepMs(10);
    return ok;
}

bool AW410KDevice::setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    // Implemented via the per-key path (which performs the initialize handshake
    // the keyboard requires) with every key set to the same colour.
    std::vector<KeyColor> keys;
    keys.reserve(kKeyCount);
    for(std::size_t i = 0; i < kKeyCount; ++i) {
        keys.push_back({kKeyMap[i].idx, r, g, b});
    }
    return setPerKey(keys);
}

bool AW410KDevice::sendMode(Mode mode, Speed speed, Direction dir, ColorMode cm,
                            std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Report buf{};
    buf[0x01] = 0x05; buf[0x02] = 0x01; buf[0x03] = static_cast<std::uint8_t>(mode);
    buf[0x04] = r; buf[0x05] = g; buf[0x06] = b;
    buf[0x0A] = static_cast<std::uint8_t>(speed); buf[0x0B] = 0x0A; buf[0x0D] = 0x01;
    buf[0x0E] = static_cast<std::uint8_t>(cm); buf[0x0F] = static_cast<std::uint8_t>(dir);
    return writeReport(buf);
}

bool AW410KDevice::setEffect(Mode mode, Speed speed, Direction dir, ColorMode cm,
                             std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    // Feature-report prelude: without this the keyboard ignores the new mode
    // and keeps showing its previously stored lighting.
    if(!featureReport(0x05, 0x01, 0x51, 0x00)) {
        return false;
    }
    return sendMode(mode, speed, dir, cm, r, g, b) && commit();
}

bool AW410KDevice::setStatic(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return setEffect(Mode::Static, Speed::Normal, Direction::Right, ColorMode::Single, r, g, b);
}

bool AW410KDevice::setOff() {
    return setEffect(Mode::Off, Speed::Normal, Direction::Right, ColorMode::Single, 0, 0, 0);
}

bool AW410KDevice::setMorph(Speed speed,
                            std::uint8_t r1, std::uint8_t g1, std::uint8_t b1,
                            std::uint8_t r2, std::uint8_t g2, std::uint8_t b2) {
    Report buf{};
    buf[0x01] = 0x05; buf[0x02] = 0x01; buf[0x03] = static_cast<std::uint8_t>(Mode::Morph);
    buf[0x04] = r1; buf[0x05] = g1; buf[0x06] = b1;
    buf[0x07] = r2; buf[0x08] = g2; buf[0x09] = b2;
    buf[0x0A] = static_cast<std::uint8_t>(speed); buf[0x0B] = 0x0A; buf[0x0D] = 0x01;
    buf[0x0E] = static_cast<std::uint8_t>(ColorMode::Two);
    return writeReport(buf) && commit();
}

bool AW410KDevice::setPerKey(const std::vector<KeyColor>& keys) {
    if(!initialize()) {
        return false;
    }
    if(!featureReport(0x05, 0x01, 0x51, 0x00)) {
        return false;
    }
    if(!commit()) {
        return false;
    }

    std::vector<KeyColor> frame = keys;
    if(!featureReport(0x0E, static_cast<std::uint8_t>(frame.size() & 0xFF), 0x00, 0x01)) {
        return false;
    }
    // Pad to a multiple of 4 (4 keys per packet).
    while(frame.size() % 4 != 0) {
        frame.push_back(KeyColor{0, 0, 0, 0});
    }

    std::uint8_t frameIdx = 0;
    for(std::size_t i = 0; i < frame.size(); i += 4) {
        Report buf{};
        buf[0x01] = 0x0E; buf[0x02] = 0x01; buf[0x03] = 0x00; buf[0x04] = ++frameIdx;
        for(int slot = 0; slot < 4; ++slot) {
            const KeyColor& k = frame[i + slot];
            const int base = 0x05 + slot * 0x0F;  // key-blocks at 0x05/0x14/0x23/0x32
            buf[base + 0]  = k.idx;
            buf[base + 1]  = 0x81;
            buf[base + 3]  = 0xA5;
            buf[base + 5]  = 0x0A;
            buf[base + 6]  = k.r;
            buf[base + 7]  = k.g;
            buf[base + 8]  = k.b;
            buf[base + 13] = 0x01;
        }
        if(!writeReport(buf)) {
            return false;
        }
    }
    return true;
}

} // namespace krgb
