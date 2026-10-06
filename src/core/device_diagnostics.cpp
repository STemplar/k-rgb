#include "core/device_diagnostics.h"
#include "core/lightmount_device.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>

namespace krgb {

namespace {

std::uint16_t crc16Modbus(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xffff;
    for(std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for(int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u)
                ? static_cast<std::uint16_t>((crc >> 1) ^ 0xa001)
                : static_cast<std::uint16_t>(crc >> 1);
        }
    }
    return crc;
}

bool readLightMountVendorSerial(std::string& serial) {
    serial.clear();
    const std::string path = LightMountDevice::findDevicePath();
    if(path.empty()) {
        return false;
    }

    const int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK);
    if(fd < 0) {
        return false;
    }

    std::array<std::uint8_t, LightMountDevice::kReportLen> request{};
    // Captured IO Center Get Serial Number request:
    //   06 00 00 00 <seq> 03 02
    request[0] = 0x06;
    request[4] = 0x72;
    request[5] = 0x03;
    request[6] = 0x02;
    const std::uint16_t crc = crc16Modbus(request.data(), 62);
    request[62] = static_cast<std::uint8_t>(crc & 0xff);
    request[63] = static_cast<std::uint8_t>((crc >> 8) & 0xff);

    if(::write(fd, request.data(), request.size()) !=
       static_cast<ssize_t>(request.size())) {
        ::close(fd);
        return false;
    }

    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;

    for(int attempt = 0; attempt < 8; ++attempt) {
        if(::poll(&pfd, 1, 200) <= 0) {
            continue;
        }

        std::array<std::uint8_t, LightMountDevice::kReportLen> response{};
        const ssize_t count = ::read(fd, response.data(), response.size());
        if(count < 9) {
            continue;
        }
        if(response[4] != request[4] || response[5] != 0x03 ||
           response[6] != 0x02) {
            continue;
        }

        const std::size_t length = response[7];
        if(length == 0 || length > 32 ||
           8 + length > static_cast<std::size_t>(count)) {
            continue;
        }

        bool printable = true;
        for(std::size_t i = 0; i < length; ++i) {
            const auto ch = response[8 + i];
            if(ch < 0x20 || ch > 0x7e) {
                printable = false;
                break;
            }
        }
        if(!printable) {
            continue;
        }

        serial.assign(reinterpret_cast<const char*>(response.data() + 8), length);
        ::close(fd);
        return true;
    }

    ::close(fd);
    return false;
}

bool isLightMountDeviceSection(const DiagnosticSection& section) {
    if(section.title != "Device") {
        return false;
    }
    for(const auto& field : section.fields) {
        if(field.name == "VID:PID" && field.value == "373f:0002") {
            return true;
        }
    }
    return false;
}

void appendFields(std::ostringstream& out,
                  const std::vector<DiagnosticField>& fields) {
    std::size_t width = 0;
    for(const auto& field : fields) {
        width = std::max(width, field.name.size());
    }

    for(const auto& field : fields) {
        out << field.name;
        if(field.name.size() < width) {
            out << std::string(width - field.name.size(), ' ');
        }
        out << "  " << field.value << '\n';
    }
}

void appendTable(std::ostringstream& out, const DiagnosticTable& table) {
    if(table.headers.empty()) {
        return;
    }

    std::vector<std::size_t> widths(table.headers.size(), 0);
    for(std::size_t column = 0; column < table.headers.size(); ++column) {
        widths[column] = table.headers[column].size();
    }
    for(const auto& row : table.rows) {
        for(std::size_t column = 0;
            column < row.size() && column < widths.size(); ++column) {
            widths[column] = std::max(widths[column], row[column].size());
        }
    }

    auto appendRow = [&](const std::vector<std::string>& row) {
        for(std::size_t column = 0; column < widths.size(); ++column) {
            const std::string value = column < row.size() ? row[column] : std::string();
            out << value;
            if(column + 1 < widths.size()) {
                if(value.size() < widths[column]) {
                    out << std::string(widths[column] - value.size(), ' ');
                }
                out << "  ";
            }
        }
        out << '\n';
    };

    appendRow(table.headers);

    std::vector<std::string> separator;
    separator.reserve(widths.size());
    for(const auto width : widths) {
        separator.emplace_back(width, '-');
    }
    appendRow(separator);

    for(const auto& row : table.rows) {
        appendRow(row);
    }
}

} // namespace

std::string formatDeviceDiagnostics(const DeviceDiagnostics& diagnostics) {
    std::ostringstream out;

    for(std::size_t sectionIndex = 0;
        sectionIndex < diagnostics.sections.size(); ++sectionIndex) {
        const auto& section = diagnostics.sections[sectionIndex];
        if(sectionIndex != 0) {
            out << '\n' << '\n';
        }

        out << section.title << '\n' << '\n';
        if(!section.fields.empty()) {
            if(isLightMountDeviceSection(section)) {
                std::vector<DiagnosticField> fields = section.fields;
                std::string vendorSerial;
                if(readLightMountVendorSerial(vendorSerial)) {
                    bool replaced = false;
                    for(auto& field : fields) {
                        if(field.name == "Serial") {
                            field.value = vendorSerial;
                            replaced = true;
                            break;
                        }
                    }
                    if(!replaced) {
                        fields.push_back({"Serial", vendorSerial});
                    }
                } else {
                    // The USB descriptor serial (e.g. QUK123456789) is not the
                    // Light Mount product serial shown by IO Center. Never
                    // present that placeholder as the device serial.
                    fields.erase(
                        std::remove_if(fields.begin(), fields.end(),
                            [](const DiagnosticField& field) {
                                return field.name == "Serial";
                            }),
                        fields.end());
                }
                appendFields(out, fields);
            } else {
                appendFields(out, section.fields);
            }
        }

        for(std::size_t tableIndex = 0;
            tableIndex < section.tables.size(); ++tableIndex) {
            if(!section.fields.empty() || tableIndex != 0) {
                out << '\n';
            }
            appendTable(out, section.tables[tableIndex]);
        }

        if(!section.notes.empty()) {
            if(!section.fields.empty() || !section.tables.empty()) {
                out << '\n';
            }
            for(const auto& note : section.notes) {
                out << note << '\n';
            }
        }
    }

    return out.str();
}

} // namespace krgb
