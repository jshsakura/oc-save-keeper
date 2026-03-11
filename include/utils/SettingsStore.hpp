#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

#include <json-c/json.h>

#include "utils/Paths.hpp"

namespace utils {

class SettingsStore {
public:
    static std::string getString(const char* key, const std::string& fallback = "") {
        json_object* root = loadRoot();
        if (!root) {
            return fallback;
        }

        json_object* value = nullptr;
        const std::string result = json_object_object_get_ex(root, key, &value)
            ? std::string(json_object_get_string(value))
            : fallback;
        json_object_put(root);
        return result;
    }

    static int getInt(const char* key, int fallback = 0) {
        json_object* root = loadRoot();
        if (!root) {
            return fallback;
        }

        json_object* value = nullptr;
        const int result = json_object_object_get_ex(root, key, &value)
            ? json_object_get_int(value)
            : fallback;
        json_object_put(root);
        return result;
    }

    static bool setString(const char* key, const std::string& value) {
        json_object* root = loadRoot();
        if (!root) {
            root = json_object_new_object();
        }
        json_object_object_add(root, key, json_object_new_string(value.c_str()));
        return saveRoot(root);
    }

    static bool setInt(const char* key, int value) {
        json_object* root = loadRoot();
        if (!root) {
            root = json_object_new_object();
        }
        json_object_object_add(root, key, json_object_new_int(value));
        return saveRoot(root);
    }

private:
    static json_object* loadRoot() {
        utils::paths::ensureBaseDirectories();

        FILE* file = std::fopen(utils::paths::SETTINGS_JSON, "r");
        if (!file) {
            return nullptr;
        }

        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        if (size < 0) {
            std::fclose(file);
            return nullptr;
        }

        std::string buffer(static_cast<size_t>(size), '\0');
        if (size > 0) {
            std::fread(buffer.data(), 1, static_cast<size_t>(size), file);
        }
        std::fclose(file);

        if (buffer.empty()) {
            return json_object_new_object();
        }

        json_object* root = json_tokener_parse(buffer.c_str());
        return root ? root : json_object_new_object();
    }

    static bool saveRoot(json_object* root) {
        utils::paths::ensureBaseDirectories();

        FILE* file = std::fopen(utils::paths::SETTINGS_JSON, "w");
        if (!file) {
            if (root) {
                json_object_put(root);
            }
            return false;
        }

        const char* text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
        const int writeResult = std::fprintf(file, "%s\n", text);
        std::fclose(file);
        json_object_put(root);
        return writeResult > 0;
    }
};

} // namespace utils
