/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * Configuration manager
 */

#pragma once

#include <string>
#include <json-c/json.h>

namespace utils {

class Config {
public:
    Config() = default;
    ~Config() = default;
    
    bool load(const std::string& path) {
        FILE* file = fopen(path.c_str(), "r");
        if (!file) return false;
        
        // Read file contents
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char* buffer = new char[size + 1];
        fread(buffer, 1, size, file);
        buffer[size] = '\0';
        fclose(file);
        
        // Parse JSON
        m_root = json_tokener_parse(buffer);
        delete[] buffer;
        
        if (!m_root) return false;
        
        return true;
    }
    
    bool save(const std::string& path) {
        if (!m_root) return false;
        
        FILE* file = fopen(path.c_str(), "w");
        if (!file) return false;
        
        const char* jsonStr = json_object_to_json_string_ext(m_root, JSON_C_TO_STRING_PRETTY);
        fprintf(file, "%s\n", jsonStr);
        fclose(file);
        
        return true;
    }
    
    void createDefault(const std::string& path) {
        m_root = json_object_new_object();
        
        json_object_object_add(m_root, "dropbox_enabled", 
            json_object_new_boolean(true));
        json_object_object_add(m_root, "auto_sync", 
            json_object_new_boolean(false));
        json_object_object_add(m_root, "max_versions", 
            json_object_new_int(5));
        json_object_object_add(m_root, "language", 
            json_object_new_string("ko"));
        json_object_object_add(m_root, "backup_on_exit", 
            json_object_new_boolean(false));
        
        save(path);
    }
    
    // Getters
    bool isDropboxEnabled() const {
        return getBool("dropbox_enabled", true);
    }
    
    bool getAutoSync() const {
        return getBool("auto_sync", false);
    }
    
    int getMaxVersions() const {
        return getInt("max_versions", 5);
    }
    
    std::string getLanguage() const {
        return getString("language", "ko");
    }
    
    bool getBackupOnExit() const {
        return getBool("backup_on_exit", false);
    }
    
    // Setters
    void setDropboxEnabled(bool enabled) {
        setBool("dropbox_enabled", enabled);
    }
    
    void setAutoSync(bool enabled) {
        setBool("auto_sync", enabled);
    }
    
    void setMaxVersions(int versions) {
        setInt("max_versions", versions);
    }
    
    void setLanguage(const std::string& lang) {
        setString("language", lang);
    }
    
    void setBackupOnExit(bool enabled) {
        setBool("backup_on_exit", enabled);
    }
    
private:
    json_object* m_root = nullptr;
    
    std::string getString(const char* key, const std::string& defaultVal = "") const {
        json_object* obj = nullptr;
        if (m_root && json_object_object_get_ex(m_root, key, &obj)) {
            return json_object_get_string(obj);
        }
        return defaultVal;
    }
    
    bool getBool(const char* key, bool defaultVal = false) const {
        json_object* obj = nullptr;
        if (m_root && json_object_object_get_ex(m_root, key, &obj)) {
            return json_object_get_boolean(obj);
        }
        return defaultVal;
    }
    
    int getInt(const char* key, int defaultVal = 0) const {
        json_object* obj = nullptr;
        if (m_root && json_object_object_get_ex(m_root, key, &obj)) {
            return json_object_get_int(obj);
        }
        return defaultVal;
    }
    
    void setString(const char* key, const std::string& value) {
        if (!m_root) m_root = json_object_new_object();
        json_object_object_add(m_root, key, json_object_new_string(value.c_str()));
    }
    
    void setBool(const char* key, bool value) {
        if (!m_root) m_root = json_object_new_object();
        json_object_object_add(m_root, key, json_object_new_boolean(value));
    }
    
    void setInt(const char* key, int value) {
        if (!m_root) m_root = json_object_new_object();
        json_object_object_add(m_root, key, json_object_new_int(value));
    }
};

} // namespace utils
