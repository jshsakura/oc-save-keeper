#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace utils {

struct QRCodeMatrix {
    int size = 0;
    std::vector<std::uint8_t> modules;
};

bool generateQRCode(const std::string& text, QRCodeMatrix& out);

}
