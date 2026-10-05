#include "core/device_diagnostics.h"
#include "core/device_diagnostics_builders.h"

#include <cstdio>
#include <string>

int krgb_cli_legacy_main(int argc, char** argv);

int main(int argc, char** argv) {
    if(argc == 3) {
        const std::string family = argv[1];
        const std::string command = argv[2];

        krgb::DeviceDiagnostics diagnostics;
        std::string err;

        if(family == "lightmount" && command == "info") {
            if(!krgb::buildLightMountDiagnostics(diagnostics, &err)) {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                return 1;
            }
            const std::string text = krgb::formatDeviceDiagnostics(diagnostics);
            std::fwrite(text.data(), 1, text.size(), stdout);
            return 0;
        }

        if(family == "logitech" && command == "info") {
            if(!krgb::buildLogitechHIDPP20Diagnostics(diagnostics, &err)) {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                return 1;
            }
            const std::string text = krgb::formatDeviceDiagnostics(diagnostics);
            std::fwrite(text.data(), 1, text.size(), stdout);
            return 0;
        }
    }

    return krgb_cli_legacy_main(argc, argv);
}
