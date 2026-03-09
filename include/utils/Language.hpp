/**
 * oc-save-keeper - Multi-language support
 * Auto-detects system language on Nintendo Switch
 */

#pragma once

#include <string>
#include <map>
#include <json-c/json.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace utils {

class Language {
public:
    static Language& instance() {
        static Language lang;
        return lang;
    }
    
    // Initialize with auto-detected system language
    bool init() {
        std::string langCode = detectSystemLanguage();
        return load(langCode);
    }
    
    bool load(const std::string& langCode) {
        std::string path = "romfs:/lang/" + langCode + ".json";
        
        FILE* file = fopen(path.c_str(), "r");
        if (!file) {
            // Fallback path for SD card
            path = "/switch/OpenCourse/oc-save-keeper/lang/" + langCode + ".json";
            file = fopen(path.c_str(), "r");
        }
        
        if (!file) {
            // Fallback to Korean
            if (langCode != "ko") {
                return load("ko");
            }
            return false;
        }
        
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char* buffer = new char[size + 1];
        fread(buffer, 1, size, file);
        buffer[size] = '\0';
        fclose(file);
        
        json_object* root = json_tokener_parse(buffer);
        delete[] buffer;
        
        if (!root) return false;
        
        m_strings.clear();
        
        json_object_object_foreach(root, key, val) {
            m_strings[key] = json_object_get_string(val);
        }
        
        json_object_put(root);
        m_currentLang = langCode;
        
        return true;
    }
    
    std::string get(const std::string& key) const {
        auto it = m_strings.find(key);
        if (it != m_strings.end()) {
            return it->second;
        }
        return "[" + key + "]"; // Missing key indicator
    }
    
    const char* c_str(const std::string& key) const {
        auto it = m_strings.find(key);
        if (it != m_strings.end()) {
            return it->second.c_str();
        }
        return key.c_str();
    }
    
    const std::string& currentLang() const { return m_currentLang; }

private:
    Language() : m_currentLang("ko") {}
    
    std::string detectSystemLanguage() {
#ifdef __SWITCH__
        Result rc = setInitialize();
        if (R_SUCCEEDED(rc)) {
            u64 languageCode = 0;
            rc = setGetSystemLanguage(&languageCode);
            if (R_SUCCEEDED(rc)) {
                SetLanguage lang;
                rc = setMakeLanguage(languageCode, &lang);
                if (R_SUCCEEDED(rc)) {
                    setExit();
            
                    // Keep the policy intentionally simple for now:
                    // Korean users get the localized UI, everyone else gets English.
                    switch (lang) {
                        case SetLanguage_KO:
                            return "ko";
                        default:
                            return "en";
                    }
                }
            }

            setExit();
        }
#endif
        // Default to Korean if detection fails or not on Switch
        return "ko";
    }
    
    std::map<std::string, std::string> m_strings;
    std::string m_currentLang;
};

// Macro for easy access
#define LANG(key) utils::Language::instance().c_str(key)

} // namespace utils
