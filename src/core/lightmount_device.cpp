#include "core/lightmount_device.h"

#include "core/lightmount_map.h"
#include "core/lightmount_keymap.h"

#include <fcntl.h>
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

LightMountDevice::~LightMountDevice() {
    close();
}

std::string LightMountDevice::findDevicePath() {
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
            const auto p2 = (p1 == std::string::npos)
                ? std::string::npos
                : body.find(':', p1 + 1);
            if(p1 != std::string::npos && p2 != std::string::npos) {
                parseHex(body.substr(p1 + 1, p2 - p1 - 1), vid);
                parseHex(body.substr(p2 + 1), pid);
            }
        }

        if(vid != kVendorId || pid != kProductId) {
            continue;
        }

        // The USB interface directory is an ancestor of the resolved HID node.
        const fs::path real = fs::canonical(devlink, ec);
        if(ec) {
            continue;
        }

        int ifnum = -1;
        fs::path parent = real.parent_path();
        for(int depth = 0; depth < 4 && !parent.empty(); ++depth) {
            std::ifstream bi(parent / "bInterfaceNumber");
            if(bi) {
                std::string s;
                bi >> s;
                parseHex(s, ifnum);
                break;
            }
            parent = parent.parent_path();
        }

        if(ifnum == kVendorInterface) {
            return std::string("/dev/") + name;
        }
    }

    return {};
}

bool LightMountDevice::open(std::string* err) {
    const std::string p = findDevicePath();
    if(p.empty()) {
        if(err) {
            *err = "No be quiet! Light Mount vendor HID interface found";
        }
        return false;
    }
    return openPath(p, err);
}

bool LightMountDevice::openPath(const std::string& path, std::string* err) {
    close();
    const int fd = ::open(path.c_str(), O_RDWR);
    if(fd < 0) {
        if(err) {
            *err = "open " + path + ": " + std::strerror(errno) +
                   " (install the k-rgb udev rule, or run as root)";
            if(errno == EACCES || errno == EPERM) {
                *err +=
                    "\nThe udev rule only takes effect for devices connected after installation."
                    "\nFix: unplug and reconnect the keyboard, or run:\n"
                    "  sudo udevadm control --reload-rules && "
                    "sudo udevadm trigger --subsystem-match=hidraw";
            }
        }
        return false;
    }

    fd_ = fd;
    path_ = path;
    sequence_ = 1;
    return true;
}

void LightMountDevice::close() {
    if(fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    path_.clear();
}

std::uint16_t LightMountDevice::crc16Modbus(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xffff;

    for(std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for(int bit = 0; bit < 8; ++bit) {
            if(crc & 1) {
                crc = static_cast<std::uint16_t>((crc >> 1) ^ 0xa001);
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

bool LightMountDevice::writePacket(Report& packet) {
    if(fd_ < 0) {
        return false;
    }

    packet[4] = sequence_++;
    const std::uint16_t crc = crc16Modbus(packet.data(), 62);
    packet[62] = static_cast<std::uint8_t>(crc & 0xff);
    packet[63] = static_cast<std::uint8_t>((crc >> 8) & 0xff);

    const ssize_t n = ::write(fd_, packet.data(), packet.size());
    return n == static_cast<ssize_t>(packet.size());
}

bool LightMountDevice::setCustomMode() {
    Report packet{};
    packet[0] = 0x07;
    packet[2] = 0x01;
    packet[5] = 0x10;
    packet[6] = 0x02;
    packet[7] = 0x03;
    return writePacket(packet);
}

bool LightMountDevice::sendFiveLeds(const LightMountLedColor* leds) {
    Report packet{};

    // Validated short IO Center Custom update:
    //   26 00 01 00 <seq> 10 0d 06
    // followed by five records:
    //   03 <id-lo> <id-hi> <r> <g> <b>
    packet[0] = 0x26;
    packet[2] = 0x01;
    packet[5] = 0x10;
    packet[6] = 0x0d;
    packet[7] = 0x06;

    for(std::size_t i = 0; i < kLedsPerPacket; ++i) {
        const std::size_t offset = 8 + i * 6;
        packet[offset + 0] = 0x03;
        packet[offset + 1] = static_cast<std::uint8_t>(leds[i].id & 0xff);
        packet[offset + 2] = static_cast<std::uint8_t>((leds[i].id >> 8) & 0xff);
        packet[offset + 3] = leds[i].r;
        packet[offset + 4] = leds[i].g;
        packet[offset + 5] = leds[i].b;
    }

    return writePacket(packet);
}

bool LightMountDevice::setLeds(const std::vector<LightMountLedColor>& leds) {
    if(leds.empty()) {
        return false;
    }

    for(std::size_t i = 0; i < leds.size(); i += kLedsPerPacket) {
        const std::size_t remaining = leds.size() - i;
        if(remaining >= kLedsPerPacket) {
            if(!sendFiveLeds(&leds[i])) {
                return false;
            }
        } else {
            std::array<LightMountLedColor, kLedsPerPacket> packetLeds{};
            for(std::size_t j = 0; j < remaining; ++j) {
                packetLeds[j] = leds[i + j];
            }

            // The protocol has no validated "record count" field. Repeat the
            // last requested record to fill the packet without changing any
            // unrelated LED state.
            for(std::size_t j = remaining; j < kLedsPerPacket; ++j) {
                packetLeds[j] = packetLeds[remaining - 1];
            }

            if(!sendFiveLeds(packetLeds.data())) {
                return false;
            }
        }
        sleepMs(10);
    }

    return true;
}

bool LightMountDevice::setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    std::vector<LightMountLedColor> leds;
    leds.reserve(lightmount::kTopBarCount + 1 + lightmount::kKeyCount +
                 lightmount::kLeftStripCount + lightmount::kRightStripCount);

    for(std::uint16_t id = lightmount::kTopBarFirst; id <= lightmount::kTopBarLast; ++id) {
        leds.push_back({id, r, g, b});
    }

    leds.push_back({lightmount::kMediaKnobLed, r, g, b});

    for(const auto& key : lightmount::kKeys) {
        leds.push_back({key.ledId, r, g, b});
    }

    for(std::uint16_t id = lightmount::kLeftStripFirst; id <= lightmount::kLeftStripLast; ++id) {
        leds.push_back({id, r, g, b});
    }
    for(std::uint16_t id = lightmount::kRightStripFirst; id <= lightmount::kRightStripLast; ++id) {
        leds.push_back({id, r, g, b});
    }

    if(!setCustomMode()) {
        return false;
    }
    sleepMs(100);
    return setLeds(leds);
}

bool LightMountDevice::setAccentSolid(
    std::uint8_t topR, std::uint8_t topG, std::uint8_t topB,
    std::uint8_t leftR, std::uint8_t leftG, std::uint8_t leftB,
    std::uint8_t rightR, std::uint8_t rightG, std::uint8_t rightB) {

    std::vector<LightMountLedColor> leds;
    leds.reserve(lightmount::kAccentLedCount);

    for(std::uint16_t id = lightmount::kTopBarFirst; id <= lightmount::kTopBarLast; ++id) {
        leds.push_back({id, topR, topG, topB});
    }
    for(std::uint16_t id = lightmount::kLeftStripFirst; id <= lightmount::kLeftStripLast; ++id) {
        leds.push_back({id, leftR, leftG, leftB});
    }
    for(std::uint16_t id = lightmount::kRightStripFirst; id <= lightmount::kRightStripLast; ++id) {
        leds.push_back({id, rightR, rightG, rightB});
    }

    if(!setCustomMode()) {
        return false;
    }
    sleepMs(100);
    return setLeds(leds);
}

} // namespace krgb
