#include "utils/QRCode.hpp"

#include "third_party/qrcodegen.h"

#include <vector>

namespace utils {

bool generateQRCode(const std::string& text, QRCodeMatrix& out) {
    out.size = 0;
    out.modules.clear();
    if (text.empty()) {
        return false;
    }

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(qrcodegen_BUFFER_LEN_MAX), 0);
    std::vector<std::uint8_t> qrCode(static_cast<std::size_t>(qrcodegen_BUFFER_LEN_MAX), 0);

    const bool encoded = qrcodegen_encodeText(
        text.c_str(),
        buffer.data(),
        qrCode.data(),
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true
    );

    if (!encoded) {
        out.size = 0;
        out.modules.clear();
        return false;
    }

    const int size = qrcodegen_getSize(qrCode.data());
    if (size <= 0) {
        return false;
    }

    out.size = size;
    out.modules.resize(static_cast<std::size_t>(size * size), 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            out.modules[static_cast<std::size_t>(y * size + x)] = qrcodegen_getModule(qrCode.data(), x, y) ? 1u : 0u;
        }
    }
    return true;
}

}
