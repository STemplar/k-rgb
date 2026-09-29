#include "core/hid_lamp_array_device.h"

#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
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

namespace {

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

std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

} // namespace

HIDLampArrayDevice::~HIDLampArrayDevice() {
    close();
}

std::string HIDLampArrayDevice::findDevicePath(std::uint16_t vendorId,
                                               std::uint16_t productId,
                                               int interfaceNumber) {
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
                ? std::string::npos
                : body.find(':', p1 + 1);
            if(p1 != std::string::npos && p2 != std::string::npos) {
                parseHex(body.substr(p1 + 1, p2 - p1 - 1), vid);
                parseHex(body.substr(p2 + 1), pid);
            }
        }

        if(vid != vendorId || pid != productId) {
            continue;
        }

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

        if(ifnum == interfaceNumber) {
            return std::string("/dev/") + name;
        }
    }

    return {};
}

bool HIDLampArrayDevice::open(std::uint16_t vendorId, std::uint16_t productId,
                              int interfaceNumber, std::string* err) {
    const std::string p = findDevicePath(vendorId, productId, interfaceNumber);
    if(p.empty()) {
        if(err) {
            *err = "No matching HID LampArray interface found";
        }
        return false;
    }
    return openPath(p, err);
}

bool HIDLampArrayDevice::openPath(const std::string& path, std::string* err) {
    close();

    const int fd = ::open(path.c_str(), O_RDWR);
    if(fd < 0) {
        if(err) {
            *err = "open " + path + ": " + std::strerror(errno) +
                   " (install the k-rgb udev rule, or run as root)";
        }
        return false;
    }

    fd_ = fd;
    path_ = path;
    return true;
}

void HIDLampArrayDevice::close() {
    if(fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    path_.clear();
}

bool HIDLampArrayDevice::getFeature(void* data, std::size_t len) {
    if(fd_ < 0) {
        return false;
    }
    return ::ioctl(fd_, HIDIOCGFEATURE(static_cast<int>(len)), data) >= 0;
}

bool HIDLampArrayDevice::setFeature(void* data, std::size_t len) {
    if(fd_ < 0) {
        return false;
    }
    return ::ioctl(fd_, HIDIOCSFEATURE(static_cast<int>(len)), data) >= 0;
}

bool HIDLampArrayDevice::getAttributes(HIDLampArrayAttributes& attrs) {
    std::uint8_t buf[kAttributesReportLen]{};
    buf[0] = kAttributesReportId;

    if(!getFeature(buf, sizeof(buf))) {
        return false;
    }

    attrs.lampCount = le16(&buf[1]);
    attrs.widthUm = le32(&buf[3]);
    attrs.heightUm = le32(&buf[7]);
    attrs.depthUm = le32(&buf[11]);
    attrs.kind = le32(&buf[15]);
    attrs.minUpdateIntervalUs = le32(&buf[19]);
    return true;
}

bool HIDLampArrayDevice::setAutonomousMode(bool enabled) {
    std::uint8_t buf[kControlReportLen]{};
    buf[0] = kControlReportId;
    buf[1] = enabled ? 1 : 0;
    return setFeature(buf, sizeof(buf));
}

bool HIDLampArrayDevice::setRange(std::uint16_t firstLamp, std::uint16_t lastLamp,
                                  std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                  std::uint8_t intensity) {
    if(firstLamp > lastLamp) {
        return false;
    }

    std::uint8_t buf[kRangeUpdateReportLen]{};
    buf[0] = kRangeUpdateReportId;
    buf[1] = 0x01; // LampUpdateComplete

    buf[2] = static_cast<std::uint8_t>(firstLamp & 0xff);
    buf[3] = static_cast<std::uint8_t>((firstLamp >> 8) & 0xff);
    buf[4] = static_cast<std::uint8_t>(lastLamp & 0xff);
    buf[5] = static_cast<std::uint8_t>((lastLamp >> 8) & 0xff);

    buf[6] = r;
    buf[7] = g;
    buf[8] = b;
    buf[9] = intensity;

    return setFeature(buf, sizeof(buf));
}

bool HIDLampArrayDevice::setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                  std::uint8_t intensity) {
    HIDLampArrayAttributes attrs;
    if(!getAttributes(attrs) || attrs.lampCount == 0) {
        return false;
    }

    if(!setAutonomousMode(false)) {
        return false;
    }

    return setRange(0, static_cast<std::uint16_t>(attrs.lampCount - 1),
                    r, g, b, intensity);
}

} // namespace krgb
