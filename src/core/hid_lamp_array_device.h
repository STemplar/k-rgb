// HIDLampArrayDevice — generic Linux hidraw helper for HID LampArray devices.
//
// Implements the standard Lighting and Illumination Usage Page (0x59)
// LampArray feature reports needed for host control, individual updates, and range updates.
// Device discovery is parameterized by VID/PID/interface so callers are not
// tied to unstable /dev/hidrawN numbering.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace krgb {

struct HIDLampArrayAttributes {
    std::uint16_t lampCount = 0;
    std::uint32_t widthUm = 0;
    std::uint32_t heightUm = 0;
    std::uint32_t depthUm = 0;
    std::uint32_t kind = 0;
    std::uint32_t minUpdateIntervalUs = 0;
};

class HIDLampArrayDevice {
public:
    // Microsoft reference report IDs/sizes used by the Light Mount and many
    // HID LampArray implementations. A future descriptor parser can make
    // these fully dynamic without changing the public API.
    static constexpr std::uint8_t kAttributesReportId = 0x01;
    static constexpr std::uint8_t kMultiUpdateReportId = 0x04;
    static constexpr std::uint8_t kRangeUpdateReportId = 0x05;
    static constexpr std::uint8_t kControlReportId = 0x06;

    static constexpr std::size_t kAttributesReportLen = 23;
    static constexpr std::size_t kMultiUpdateReportLen = 51;
    static constexpr std::size_t kMultiUpdateSlots = 8;
    static constexpr std::size_t kRangeUpdateReportLen = 10;
    static constexpr std::size_t kControlReportLen = 2;

    HIDLampArrayDevice() = default;
    ~HIDLampArrayDevice();
    HIDLampArrayDevice(const HIDLampArrayDevice&) = delete;
    HIDLampArrayDevice& operator=(const HIDLampArrayDevice&) = delete;

    // Locate a specific HID interface by USB VID/PID/interface number.
    static std::string findDevicePath(std::uint16_t vendorId,
                                      std::uint16_t productId,
                                      int interfaceNumber);

    bool open(std::uint16_t vendorId, std::uint16_t productId,
              int interfaceNumber, std::string* err = nullptr);
    bool openPath(const std::string& path, std::string* err = nullptr);
    void close();

    bool isOpen() const { return fd_ >= 0; }
    const std::string& path() const { return path_; }

    bool getAttributes(HIDLampArrayAttributes& attrs);
    bool setAutonomousMode(bool enabled);

    // LampMultiUpdate: set exactly one lamp without touching the others.
    bool setLamp(std::uint16_t lampId,
                 std::uint8_t r, std::uint8_t g, std::uint8_t b,
                 std::uint8_t intensity = 255);

    // LampRangeUpdate: set a contiguous range to one RGBI value.
    bool setRange(std::uint16_t firstLamp, std::uint16_t lastLamp,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  std::uint8_t intensity = 255);

    // Disable AutonomousMode and set every reported lamp to one colour.
    bool setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  std::uint8_t intensity = 255);

private:
    bool getFeature(void* data, std::size_t len);
    bool setFeature(void* data, std::size_t len);

    int fd_ = -1;
    std::string path_;
};

} // namespace krgb
