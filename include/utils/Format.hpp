#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace utils {

inline std::string formatStorageSize(int64_t bytes) {
    if (bytes <= 0) {
        return "0 B";
    }

    char buffer[32];
    if (bytes < 1024) {
        std::snprintf(buffer, sizeof(buffer), "%lld B", static_cast<long long>(bytes));
        return buffer;
    }

    const double kb = bytes / 1024.0;
    if (kb < 1024.0) {
        std::snprintf(buffer, sizeof(buffer), kb < 10.0 ? "%.1f KB" : "%.0f KB", kb);
        return buffer;
    }

    const double mb = kb / 1024.0;
    if (mb < 1024.0) {
        std::snprintf(buffer, sizeof(buffer), mb < 10.0 ? "%.1f MB" : "%.0f MB", mb);
        return buffer;
    }

    const double gb = mb / 1024.0;
    std::snprintf(buffer, sizeof(buffer), gb < 10.0 ? "%.1f GB" : "%.0f GB", gb);
    return buffer;
}

} // namespace utils
