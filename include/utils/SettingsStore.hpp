#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

#include <json-c/json.h>

#include "utils/Paths.hpp"

namespace utils {

namespace detail {
struct ScopedFile {
    FILE* fp = nullptr;
    explicit ScopedFile(FILE* f = nullptr) : fp(f) {}
    ~ScopedFile() { if (fp) fclose(fp); }
    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;
    ScopedFile(ScopedFile&& o) noexcept : fp(o.fp) { o.fp = nullptr; }
    ScopedFile& operator=(ScopedFile&& o) noexcept {
        if (this != &o) { if (fp) fclose(fp); fp = o.fp; o.fp = nullptr; }
        return *this;
    }
    FILE* get() const { return fp; }
    operator bool() const { return fp != nullptr; }
};
} // namespace detail

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
    operator bool() const { return obj != nullptr; }
    void reset(json_object* o = nullptr) {
        if (obj) json_object_put(obj);
        obj = o;
    }
};

class SettingsStore {
public:
    static std::string getString(const char* key, const std::string& fallback = "") {
        ScopedJson root(loadRoot());
        if (!root) {
            return fallback;
        }

        json_object* value = nullptr;
        const std::string result = json_object_object_get_ex(root.get(), key, &value)
            ? std::string(json_object_get_string(value))
            : fallback;
        return result;
    }

    static int getInt(const char* key, int fallback = 0) {
        ScopedJson root(loadRoot());
        if (!root) {
            return fallback;
        }

        json_object* value = nullptr;
        const int result = json_object_object_get_ex(root.get(), key, &value)
            ? json_object_get_int(value)
            : fallback;
        return result;
    }

    static bool setString(const char* key, const std::string& value) {
        ScopedJson root(loadRoot());
        if (!root) {
            root.reset(json_object_new_object());
        }
        json_object_object_add(root.get(), key, json_object_new_string(value.c_str()));
        return saveRoot(root.get());
    }

    static bool setInt(const char* key, int value) {
        ScopedJson root(loadRoot());
        if (!root) {
            root.reset(json_object_new_object());
        }
        json_object_object_add(root.get(), key, json_object_new_int(value));
        return saveRoot(root.get());
    }

private:
    static json_object* loadRoot() {
        utils::paths::ensureBaseDirectories();

        detail::ScopedFile file(std::fopen(utils::paths::SETTINGS_JSON, "r"));
        if (!file) {
            return nullptr;
        }

        std::fseek(file.get(), 0, SEEK_END);
        const long size = std::ftell(file.get());
        std::fseek(file.get(), 0, SEEK_SET);
        if (size < 0) {
            return nullptr;
        }

        std::string buffer(static_cast<size_t>(size), '\0');
        if (size > 0) {
            std::fread(buffer.data(), 1, static_cast<size_t>(size), file.get());
        }

        if (buffer.empty()) {
            return json_object_new_object();
        }

        json_object* root = json_tokener_parse(buffer.c_str());
        return root ? root : json_object_new_object();
    }

    static bool saveRoot(json_object* root) {
        utils::paths::ensureBaseDirectories();

        detail::ScopedFile file(std::fopen(utils::paths::SETTINGS_JSON, "w"));
        if (!file) {
            if (root) {
                json_object_put(root);
            }
            return false;
        }

        const char* text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
        const int writeResult = std::fprintf(file.get(), "%s\n", text);
        json_object_put(root);
        return writeResult > 0;
    }
};

} // namespace utils
