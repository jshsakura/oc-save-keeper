#pragma once

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cctype>
#include <random>
#include <string>
#include <vector>
#include <json-c/json.h>
#include <curl/curl.h>
#include <curl/curl.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace network::dropbox {

// Cryptographically secure random byte generator
// Uses platform CSRNG on device, falls back to std::random_device on host
inline bool getCryptographicBytes(unsigned char* buffer, std::size_t size) {
#ifdef __SWITCH__
    Result rc = csrngGetRandomBytes(buffer, size);
    return R_SUCCEEDED(rc);
#else
    std::random_device rd;
    for (std::size_t i = 0; i < size; ++i) {
        buffer[i] = static_cast<unsigned char>(rd() & 0xFFu);
    }
    return true;
#endif
}

// Bias-free random index selection using rejection sampling
// Eliminates modulo bias when selecting from alphabet
inline std::size_t getRandomIndex(std::size_t alphabetSize) {
    // Calculate rejection threshold to eliminate bias
    // We reject values >= threshold where threshold = (256 / alphabetSize) * alphabetSize
    constexpr unsigned int kMaxValue = 256;
    const unsigned int threshold = (kMaxValue / static_cast<unsigned int>(alphabetSize)) * static_cast<unsigned int>(alphabetSize);
    
    unsigned char byte;
    while (true) {
        if (!getCryptographicBytes(&byte, 1)) {
            // Fallback on failure
            std::random_device rd;
            byte = static_cast<unsigned char>(rd() & 0xFFu);
        }
        if (byte < threshold) {
            return static_cast<std::size_t>(byte) % alphabetSize;
        }
        // Reject and try again
    }
}

struct ScopedCurlSlist {
    struct curl_slist* list = nullptr;
    explicit ScopedCurlSlist(struct curl_slist* l = nullptr) : list(l) {}
    ~ScopedCurlSlist() { if (list) curl_slist_free_all(list); }
    ScopedCurlSlist(const ScopedCurlSlist&) = delete;
    ScopedCurlSlist& operator=(const ScopedCurlSlist&) = delete;
    ScopedCurlSlist(ScopedCurlSlist&& o) noexcept : list(o.list) { o.list = nullptr; }
    ScopedCurlSlist& operator=(ScopedCurlSlist&& o) noexcept {
        if (this != &o) { if (list) curl_slist_free_all(list); list = o.list; o.list = nullptr; }
        return *this;
    }
    struct curl_slist* get() const { return list; }
    operator bool() const { return list != nullptr; }
};

struct ScopedJson {
    json_object* obj = nullptr;
    explicit ScopedJson(json_object* o = nullptr) : obj(o) {}
    ~ScopedJson() { if (obj) json_object_put(obj); }
    ScopedJson(const ScopedJson&) = delete;
    ScopedJson& operator=(const ScopedJson&) = delete;
    ScopedJson(ScopedJson&& o) noexcept : obj(o.obj) { o.obj = nullptr; }
    ScopedJson& operator=(ScopedJson&& o) noexcept {
        if (this != &o) { if (obj) json_object_put(obj); obj = o.obj; o.obj = nullptr; }
        return *this;
    }
    json_object* get() const { return obj; }
    json_object* release() { json_object* r = obj; obj = nullptr; return r; }
    operator bool() const { return obj != nullptr; }
};

inline bool isSuccessfulHttpStatus(long statusCode) {
    return statusCode >= 200 && statusCode < 300;
}

inline bool isHexDigit(char ch) {
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'A' && ch <= 'F') ||
           (ch >= 'a' && ch <= 'f');
}

inline unsigned char hexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return static_cast<unsigned char>(ch - '0');
    }
    if (ch >= 'A' && ch <= 'F') {
        return static_cast<unsigned char>(10 + (ch - 'A'));
    }
    return static_cast<unsigned char>(10 + (ch - 'a'));
}

inline std::string trimCopy(std::string value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

inline std::string percentEncode(const std::string& value) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char ch : value) {
        const bool isUnreserved =
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '.' || ch == '_' || ch == '~';
        if (isUnreserved) {
            out.push_back(static_cast<char>(ch));
            continue;
        }

        out.push_back('%');
        out.push_back(kHex[(ch >> 4) & 0x0F]);
        out.push_back(kHex[ch & 0x0F]);
    }
    return out;
}

inline std::string percentDecode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '%' && i + 2 < value.size() && isHexDigit(value[i + 1]) && isHexDigit(value[i + 2])) {
            const unsigned char decoded = static_cast<unsigned char>((hexValue(value[i + 1]) << 4) | hexValue(value[i + 2]));
            out.push_back(static_cast<char>(decoded));
            i += 2;
            continue;
        }
        if (ch == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

inline std::string generateRandomHex(std::size_t bytes) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    
    std::vector<unsigned char> buffer(bytes);
    if (!getCryptographicBytes(buffer.data(), bytes)) {
        return "";
    }
    
    for (std::size_t i = 0; i < bytes; ++i) {
        out.push_back(kHex[(buffer[i] >> 4) & 0x0Fu]);
        out.push_back(kHex[buffer[i] & 0x0Fu]);
    }
    return out;
}

inline std::string generateCodeVerifier(std::size_t length = 64) {
    static const char* kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "-._~";

    if (length < 43) {
        length = 43;
    } else if (length > 128) {
        length = 128;
    }

    std::string out;
    out.reserve(length);
    const std::size_t alphabetSize = 66;
    for (std::size_t i = 0; i < length; ++i) {
        out.push_back(kAlphabet[getRandomIndex(alphabetSize)]);
    }
    return out;
}

inline std::string base64UrlEncode(const std::vector<unsigned char>& bytes) {
    static const char* kTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 3 <= bytes.size()) {
        const unsigned int chunk =
            (static_cast<unsigned int>(bytes[i]) << 16) |
            (static_cast<unsigned int>(bytes[i + 1]) << 8) |
            static_cast<unsigned int>(bytes[i + 2]);

        out.push_back(kTable[(chunk >> 18) & 0x3Fu]);
        out.push_back(kTable[(chunk >> 12) & 0x3Fu]);
        out.push_back(kTable[(chunk >> 6) & 0x3Fu]);
        out.push_back(kTable[chunk & 0x3Fu]);
        i += 3;
    }

    if (i < bytes.size()) {
        const unsigned int b0 = static_cast<unsigned int>(bytes[i]);
        out.push_back(kTable[(b0 >> 2) & 0x3Fu]);
        if (i + 1 < bytes.size()) {
            const unsigned int b1 = static_cast<unsigned int>(bytes[i + 1]);
            out.push_back(kTable[((b0 & 0x03u) << 4) | ((b1 >> 4) & 0x0Fu)]);
            out.push_back(kTable[(b1 & 0x0Fu) << 2]);
            out.push_back('=');
        } else {
            out.push_back(kTable[(b0 & 0x03u) << 4]);
            out.push_back('=');
            out.push_back('=');
        }
    }

    for (char& ch : out) {
        if (ch == '+') {
            ch = '-';
        } else if (ch == '/') {
            ch = '_';
        }
    }
    while (!out.empty() && out.back() == '=') {
        out.pop_back();
    }
    return out;
}

inline std::vector<unsigned char> sha256(const std::string& input) {
    static const std::uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    std::uint32_t h0 = 0x6a09e667u;
    std::uint32_t h1 = 0xbb67ae85u;
    std::uint32_t h2 = 0x3c6ef372u;
    std::uint32_t h3 = 0xa54ff53au;
    std::uint32_t h4 = 0x510e527fu;
    std::uint32_t h5 = 0x9b05688cu;
    std::uint32_t h6 = 0x1f83d9abu;
    std::uint32_t h7 = 0x5be0cd19u;

    std::vector<unsigned char> msg(input.begin(), input.end());
    const std::uint64_t bitLen = static_cast<std::uint64_t>(msg.size()) * 8u;

    msg.push_back(0x80u);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00u);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<unsigned char>((bitLen >> (i * 8)) & 0xFFu));
    }

    auto rotr = [](std::uint32_t x, std::uint32_t n) {
        return (x >> n) | (x << (32u - n));
    };

    for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        std::uint32_t w[64] = {};
        for (int i = 0; i < 16; ++i) {
            const std::size_t j = chunk + static_cast<std::size_t>(i) * 4;
            w[i] =
                (static_cast<std::uint32_t>(msg[j]) << 24) |
                (static_cast<std::uint32_t>(msg[j + 1]) << 16) |
                (static_cast<std::uint32_t>(msg[j + 2]) << 8) |
                static_cast<std::uint32_t>(msg[j + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7u) ^ rotr(w[i - 15], 18u) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17u) ^ rotr(w[i - 2], 19u) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;
        std::uint32_t f = h5;
        std::uint32_t g = h6;
        std::uint32_t h = h7;

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6u) ^ rotr(e, 11u) ^ rotr(e, 25u);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2u) ^ rotr(a, 13u) ^ rotr(a, 22u);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
        h5 += f;
        h6 += g;
        h7 += h;
    }

    std::vector<unsigned char> digest;
    digest.reserve(32);
    const std::uint32_t words[8] = {h0, h1, h2, h3, h4, h5, h6, h7};
    for (std::uint32_t word : words) {
        digest.push_back(static_cast<unsigned char>((word >> 24) & 0xFFu));
        digest.push_back(static_cast<unsigned char>((word >> 16) & 0xFFu));
        digest.push_back(static_cast<unsigned char>((word >> 8) & 0xFFu));
        digest.push_back(static_cast<unsigned char>(word & 0xFFu));
    }
    return digest;
}

inline std::string buildCodeChallengeS256(const std::string& verifier) {
    return base64UrlEncode(sha256(verifier));
}

inline std::time_t parseDropboxTime(const char* value) {
    if (!value || !*value) {
        return 0;
    }

    std::tm tm{};
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(value, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }

    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

#ifdef _GNU_SOURCE
    return timegm(&tm);
#else
    return mktime(&tm);
#endif
}

inline std::string buildAuthorizeUrl(const std::string& baseUrl,
                                     const std::string& clientId,
                                     const std::string& redirectUri,
                                     const std::string& csrfToken,
                                     const std::string& codeChallenge) {
    std::string url = baseUrl +
                      "?client_id=" + percentEncode(clientId) +
                      "&response_type=code" +
                      "&token_access_type=offline" +
                      "&code_challenge_method=S256" +
                      "&code_challenge=" + percentEncode(codeChallenge) +
                      "&state=" + percentEncode(csrfToken);
    if (!redirectUri.empty()) {
        url += "&redirect_uri=" + percentEncode(redirectUri);
    }
    return url;
}

inline std::string extractQueryParam(const std::string& input, const std::string& key) {
    const std::string trimmed = trimCopy(input);
    if (trimmed.empty()) {
        return "";
    }

    std::size_t queryStart = trimmed.find('?');
    queryStart = (queryStart == std::string::npos) ? 0 : queryStart + 1;

    for (std::size_t pos = queryStart; pos <= trimmed.size();) {
        const std::size_t end = trimmed.find_first_of("&#", pos);
        const std::string token = trimmed.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        const std::size_t separator = token.find('=');
        if (separator != std::string::npos) {
            const std::string candidateKey = percentDecode(token.substr(0, separator));
            if (candidateKey == key) {
                return percentDecode(token.substr(separator + 1));
            }
        }

        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }

    return "";
}

inline std::string extractAuthorizationCode(const std::string& input) {
    const std::string trimmed = trimCopy(input);
    if (trimmed.empty()) {
        return "";
    }
    const std::string fromQuery = extractQueryParam(trimmed, "code");
    if (!fromQuery.empty()) {
        return fromQuery;
    }
    if (trimmed.find('=') != std::string::npos || trimmed.find('&') != std::string::npos) {
        return "";
    }
    return percentDecode(trimmed);
}

inline std::string escapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    // Escape remaining control characters as \u00XX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
                    escaped += buf;
                } else {
                    escaped += ch;
                }
                break;
        }
    }
    return escaped;
}

inline std::string buildListFolderRequest(const std::string& path, bool recursive = false) {
    return "{\"path\":\"" + escapeJsonString(path) + "\",\"recursive\":" +
           std::string(recursive ? "true" : "false") +
           ",\"include_deleted\":false}";
}

inline std::string buildUploadArg(const std::string& path) {
    return "{\"path\":\"" + escapeJsonString(path) + "\",\"mode\":\"overwrite\",\"autorename\":false}";
}

inline std::string buildDownloadArg(const std::string& path) {
    return "{\"path\":\"" + escapeJsonString(path) + "\"}";
}

inline std::string buildCreateFolderRequest(const std::string& path) {
    return "{\"path\":\"" + escapeJsonString(path) + "\"}";
}

inline std::string buildDeleteRequest(const std::string& path) {
    return "{\"path\":\"" + escapeJsonString(path) + "\"}";
}

inline std::string parentPath(const std::string& dropboxPath) {
    const std::size_t slash = dropboxPath.rfind('/');
    if (slash == std::string::npos) {
        return "";
    }
    if (slash == 0) {
        return "/";
    }
    return dropboxPath.substr(0, slash);
}

inline std::string fileName(const std::string& dropboxPath) {
    const std::size_t slash = dropboxPath.rfind('/');
    if (slash == std::string::npos) {
        return dropboxPath;
    }
    return dropboxPath.substr(slash + 1);
}

inline bool hasFileComponent(const std::string& dropboxPath) {
    return !fileName(dropboxPath).empty();
}

inline bool constantTimeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    
    int result = 0;
    for (std::size_t i = 0; i < a.size(); i++) {
        result |= static_cast<int>(static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]));
    }
    return result == 0;
}

inline bool isUrlPrefixValid(const std::string& url, const std::string& expectedPrefix) {
    if (url.empty() || expectedPrefix.empty()) {
        return false;
    }
    if (url.find(expectedPrefix) != 0) {
        return false;
    }
    if (url.length() == expectedPrefix.length()) {
        return true;
    }
    char nextChar = url[expectedPrefix.length()];
    return nextChar == '/' || nextChar == '?' || nextChar == '#';
}

inline std::string buildSafePollUrl(const std::string& pollUrl, 
                                    const std::string& bridgeBase,
                                    const std::string& sessionId) {
    const std::string expectedPrefix = bridgeBase + "/";
    
    if (!pollUrl.empty() && isUrlPrefixValid(pollUrl, expectedPrefix)) {
        return pollUrl;
    }
    return expectedPrefix + "v1/sessions/" + sessionId + "/status";
}

} // namespace network::dropbox
