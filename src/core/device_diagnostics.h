#pragma once

#include <string>
#include <vector>

namespace krgb {

struct DiagnosticField {
    std::string name;
    std::string value;
};

struct DiagnosticTable {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

struct DiagnosticSection {
    std::string title;
    std::vector<DiagnosticField> fields;
    std::vector<DiagnosticTable> tables;
    std::vector<std::string> notes;
};

struct DeviceDiagnostics {
    std::vector<DiagnosticSection> sections;
};

// Render the shared diagnostics model as stable, human-readable plain text.
// The GUI intentionally uses the same formatter as krgb-cli so both surfaces
// show the same information and protocol interpretation.
std::string formatDeviceDiagnostics(const DeviceDiagnostics& diagnostics);

} // namespace krgb
