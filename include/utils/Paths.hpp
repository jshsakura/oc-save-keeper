#pragma once

#include <sys/stat.h>

namespace utils::paths {

inline constexpr const char* ROOT = "/oc-save-keeper";
inline constexpr const char* CONFIG = "/oc-save-keeper/config";
inline constexpr const char* CACHE = "/oc-save-keeper/cache";
inline constexpr const char* CACHE_TITLE_ICONS = "/oc-save-keeper/cache/title-icons";
inline constexpr const char* CACHE_USER_ICONS = "/oc-save-keeper/cache/user-icons";
inline constexpr const char* BACKUPS = "/oc-save-keeper/backups";
inline constexpr const char* TEMP = "/oc-save-keeper/temp";
inline constexpr const char* LOGS = "/oc-save-keeper/logs";

inline constexpr const char* SETTINGS_JSON = "/oc-save-keeper/config/settings.json";
inline constexpr const char* DROPBOX_AUTH_JSON = "/oc-save-keeper/config/dropbox_auth.json";
inline constexpr const char* DROPBOX_LEGACY_TOKEN = "/oc-save-keeper/config/dropbox_token.txt";
inline constexpr const char* DROPBOX_APP_KEY_TXT = "/oc-save-keeper/config/dropbox_app_key.txt";
inline constexpr const char* ROOT_ENV = "/oc-save-keeper/.env";
inline constexpr const char* CONFIG_ENV = "/oc-save-keeper/config/.env";

inline constexpr const char* DEVICE_ID = "/oc-save-keeper/config/device_id.txt";
inline constexpr const char* DEVICE_PRIORITY = "/oc-save-keeper/config/device_priority.txt";
inline constexpr const char* LANGUAGE_PREF = "/oc-save-keeper/config/language.pref";
inline constexpr const char* LOG_FILE = "/oc-save-keeper/logs/oc-save-keeper.log";

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
