#pragma once

#include <sys/stat.h>

namespace utils::paths {

inline constexpr const char* ROOT = "/switch/oc-save-keeper";
inline constexpr const char* CONFIG = "/switch/oc-save-keeper/config";
inline constexpr const char* CACHE = "/switch/oc-save-keeper/cache";
inline constexpr const char* CACHE_TITLE_ICONS = "/switch/oc-save-keeper/cache/title-icons";
inline constexpr const char* CACHE_USER_ICONS = "/switch/oc-save-keeper/cache/user-icons";
inline constexpr const char* BACKUPS = "/switch/oc-save-keeper/backups";
inline constexpr const char* TEMP = "/switch/oc-save-keeper/temp";
inline constexpr const char* LOGS = "/switch/oc-save-keeper/logs";

inline constexpr const char* SETTINGS_JSON = "/switch/oc-save-keeper/config/settings.json";
inline constexpr const char* DROPBOX_AUTH_JSON = "/switch/oc-save-keeper/config/dropbox_auth.json";
inline constexpr const char* DROPBOX_LEGACY_TOKEN = "/switch/oc-save-keeper/config/dropbox_token.txt";

inline void ensureBaseDirectories() {
    mkdir(ROOT, 0777);
    mkdir(CONFIG, 0777);
    mkdir(CACHE, 0777);
    mkdir(CACHE_TITLE_ICONS, 0777);
    mkdir(CACHE_USER_ICONS, 0777);
    mkdir(BACKUPS, 0777);
    mkdir(TEMP, 0777);
    mkdir(LOGS, 0777);
}

} // namespace utils::paths
