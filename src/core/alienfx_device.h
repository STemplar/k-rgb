// AlienFXDevice — driver for the Alienware "AW-ELC" RGB lighting controller
// (USB 187c:0550 / 187c:0551), which drives chassis / case zones.
//
// Unlike the keyboard, this controller is animation-based: a colour is applied
// by starting a non-saved user animation, selecting each zone and adding a
// colour action, then finishing+playing. Commands are HID *feature* reports
// (33-byte payload). The zone count is discovered from the controller.
//
// Protocol facts referenced from the OpenRGB AlienwareController driver
// (GPL-2.0); this is an independent implementation. Linux-only.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace krgb {

class AlienFXDevice {
public:
    static constexpr std::uint16_t kVendorId   = 0x187C;
    static constexpr int           kReportSize = 34;  // report-id byte + 33 payload

    AlienFXDevice() = default;
    ~AlienFXDevice();
    AlienFXDevice(const AlienFXDevice&) = delete;
    AlienFXDevice& operator=(const AlienFXDevice&) = delete;

    // Locate the controller's hidraw node; "" if not present.
    static std::string findDevicePath();

    bool open(std::string* err = nullptr);   // auto-detect + read config
    bool openPath(const std::string& path, std::string* err = nullptr);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    const std::string& path() const { return path_; }
    int                zoneCount() const { return zoneCount_; }
    const std::string& firmware() const { return firmware_; }

    using ZoneColor = std::array<std::uint8_t, 3>;  // {r, g, b}

    // High-level operations.
    bool setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b);  // all zones
    // Per-zone colours; colors[i] is zone i. Extra entries ignored, missing
    // zones left black. Zones are grouped by colour so it stays fast.
    bool setZoneColors(const std::vector<ZoneColor>& colors);
    bool setOff() { return setSolid(0, 0, 0); }
    bool reset();

private:
    using Buf = std::array<std::uint8_t, kReportSize>;

    bool sendFeature(const Buf& buf);   // HIDIOCSFEATURE
    bool getFeature(Buf& buf);          // HIDIOCGFEATURE (buf[0] = report id in)
    bool transact(const Buf& out, Buf& resp, bool slow = false);

    bool queryConfig();                 // zone count + firmware (called by open)
    bool beginAnimation();
    bool finishPlay();
    bool selectZones(const std::vector<std::uint8_t>& zones);  // one packet, <=28
    bool addColorAction(std::uint8_t r, std::uint8_t g, std::uint8_t b);

    int         fd_        = -1;
    std::string path_;
    int         zoneCount_ = 0;
    std::string firmware_;
};

} // namespace krgb
