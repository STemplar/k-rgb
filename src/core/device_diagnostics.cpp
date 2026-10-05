#include "core/device_diagnostics.h"

#include <algorithm>
#include <cstddef>
#include <sstream>

namespace krgb {

namespace {

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
            appendFields(out, section.fields);
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
