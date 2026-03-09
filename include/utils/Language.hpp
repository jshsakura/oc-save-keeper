/**
 * oc-save-keeper - Multi-language support
 * Auto-detects system language on Nintendo Switch
 */

#pragma once

#include <string>
#include <map>
#include <initializer_list>
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
            path = "/switch/oc-save-keeper/lang/" + langCode + ".json";
            file = fopen(path.c_str(), "r");
        }
        
        if (!file) {
            loadBuiltIn(langCode);
            return true;
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

    void loadBuiltIn(const std::string& langCode) {
        m_strings.clear();
        m_currentLang = (langCode == "ko") ? "ko" : "en";

        const auto assign = [this](std::initializer_list<std::pair<const char*, const char*>> items) {
            for (const auto& item : items) {
                m_strings[item.first] = item.second;
            }
        };

        if (m_currentLang == "ko") {
            assign({
                {"app.name", "oc-save-keeper"},
                {"app.slogan", "안전한 세이브 백업과 기기 간 동기화"},
                {"status.connected", "연결됨"},
                {"status.disconnected", "연결 필요"},
                {"footer.controls", "A: 선택  |  Y: 언어  |  L/R: 페이지  |  -/+: 종료"},
                {"footer.game_count", "개 게임"},
                {"detail.upload", "클라우드 업로드"},
                {"detail.download", "클라우드 다운로드"},
                {"detail.backup", "로컬 백업"},
                {"detail.history", "버전 히스토리"},
                {"detail.back", "뒤로"},
                {"detail.save_size", "세이브 크기"},
                {"detail.title_id", "Title ID"},
                {"detail.recent_backup", "최근 백업"},
                {"detail.versions", "개 버전"},
                {"detail.no_backup", "없음"},
                {"detail.latest_device", "최근 백업 기기"},
                {"detail.latest_user", "최근 백업 사용자"},
                {"detail.latest_source", "최근 백업 출처"},
                {"sync.uploading", "세이브를 클라우드에 업로드하는 중..."},
                {"sync.downloading", "클라우드 세이브를 확인하는 중..."},
                {"sync.syncing", "동기화 진행 중..."},
                {"sync.complete", "동기화가 완료되었습니다."},
                {"sync.syncing_game", "동기화 중..."},
                {"auth.title", "Dropbox 연동 설정"},
                {"auth.setup_time", "스마트폰으로 간단하게 설정 가능한 과정입니다."},
                {"auth.step1", "1. 폰 브라우저로 접속:"},
                {"auth.step2", "2. [Create App] 클릭"},
                {"auth.step2_api", "- API: Dropbox API"},
                {"auth.step2_access", "- Access: App folder"},
                {"auth.step2_name", "- Name: OCSaveKeeper-Backup"},
                {"auth.step3", "3. [Generate access token] 클릭"},
                {"auth.step4", "4. 토큰 복사 후 아래에 입력:"},
                {"auth.token_placeholder", "여기에 토큰 붙여넣기..."},
                {"auth.open_keyboard", "키보드 열기"},
                {"auth.connect", "연결"},
                {"auth.cancel", "취소"},
                {"auth.tip", "1회 설정으로 계속 사용할 수 있습니다."},
                {"history.title", "버전 히스토리"},
                {"history.local", "로컬 백업"},
                {"history.cloud", "클라우드 백업"},
                {"history.synced", "동기화됨"},
                {"history.local_only", "로컬만"},
                {"history.restore", "복원"},
                {"history.no_backup", "백업 없음"},
                {"history.no_cloud", "클라우드 백업 없음"},
                {"history.login_needed", "로그인 필요"},
                {"history.unknown_device", "알 수 없는 기기"},
                {"history.unknown_user", "알 수 없는 사용자"},
                {"history.unknown_source", "출처 정보 없음"},
                {"error.not_authenticated", "Dropbox: 연결되지 않음"},
                {"error.no_selection", "선택된 게임 없음"},
            });
            return;
        }

        assign({
            {"app.name", "oc-save-keeper"},
            {"app.slogan", "Safe save backup and cross-device sync"},
            {"status.connected", "Connected"},
            {"status.disconnected", "Not connected"},
            {"footer.controls", "A: Select  |  Y: Language  |  L/R: Page  |  -/+: Exit"},
            {"footer.game_count", " games"},
            {"detail.upload", "Upload to Cloud"},
            {"detail.download", "Download from Cloud"},
            {"detail.backup", "Local Backup"},
            {"detail.history", "Version History"},
            {"detail.back", "Back"},
            {"detail.save_size", "Save Size"},
            {"detail.title_id", "Title ID"},
            {"detail.recent_backup", "Recent Backup"},
            {"detail.versions", " versions"},
            {"detail.no_backup", "None"},
            {"detail.latest_device", "Latest backup device"},
            {"detail.latest_user", "Latest backup user"},
            {"detail.latest_source", "Latest backup source"},
            {"sync.uploading", "Uploading save data to the cloud..."},
            {"sync.downloading", "Checking cloud save metadata..."},
            {"sync.syncing", "Syncing..."},
            {"sync.complete", "Sync complete."},
            {"sync.syncing_game", "Syncing..."},
            {"auth.title", "Dropbox Setup"},
            {"auth.setup_time", "2-minute phone setup"},
            {"auth.step1", "1. Visit on your phone:"},
            {"auth.step2", "2. Click [Create App]"},
            {"auth.step2_api", "- API: Dropbox API"},
            {"auth.step2_access", "- Access: App folder"},
            {"auth.step2_name", "- Name: OCSaveKeeper-Backup"},
            {"auth.step3", "3. Click [Generate access token]"},
            {"auth.step4", "4. Copy the token and paste it below:"},
            {"auth.token_placeholder", "Paste token here..."},
            {"auth.open_keyboard", "Open Keyboard"},
            {"auth.connect", "Connect"},
            {"auth.cancel", "Cancel"},
            {"auth.tip", "One-time setup, then reuse the saved token."},
            {"history.title", "Version History"},
            {"history.local", "Local Backups"},
            {"history.cloud", "Cloud Backups"},
            {"history.synced", "Synced"},
            {"history.local_only", "Local only"},
            {"history.restore", "Restore"},
            {"history.no_backup", "No backups"},
            {"history.no_cloud", "No cloud backups"},
            {"history.login_needed", "Login required"},
            {"history.unknown_device", "Unknown device"},
            {"history.unknown_user", "Unknown user"},
            {"history.unknown_source", "No source info"},
            {"error.not_authenticated", "Dropbox: Not connected"},
            {"error.no_selection", "No game selected"},
        });
    }
    
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
