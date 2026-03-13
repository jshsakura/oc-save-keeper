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

#include "utils/Paths.hpp"
#include "utils/SettingsStore.hpp"

namespace utils {

class Language {
public:
    static Language& instance() {
        static Language lang;
        return lang;
    }
    
    // Initialize with saved or auto-detected system language
    bool init() {
        std::string langCode = loadSavedLanguage();
        if (langCode.empty()) {
            langCode = detectSystemLanguage();
        }
        return load(langCode);
    }
    
    bool load(const std::string& langCode) {
        saveLanguagePreference(langCode);
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
        
        std::string buffer(static_cast<std::size_t>(size), '\0');
        if (size > 0) {
            fread(buffer.data(), 1, static_cast<std::size_t>(size), file);
        }
        fclose(file);
        
        json_object* root = json_tokener_parse(buffer.c_str());
        
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

    void saveLanguagePreference(const std::string& lang) {
        utils::SettingsStore::setString("language", lang);
    }

    std::string loadSavedLanguage() {
        const std::string saved = utils::SettingsStore::getString("language", "");
        if (!saved.empty()) {
            return saved;
        }

        FILE* f = fopen("/switch/oc-save-keeper/language.pref", "r");
        if (f) {
            char buf[8];
            if (fgets(buf, sizeof(buf), f)) {
                fclose(f);
                return std::string(buf);
            }
            fclose(f);
        }
        return "";
    }

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
                {"app.name", "OC Save Keeper"},
                {"app.slogan", "안전한 세이브 백업과 기기 간 동기화"},
                {"status.connected", "연결됨"},
                {"status.disconnected", "연결 필요"},
                {"footer.controls.open", "선택"},
                {"footer.controls.sync_all", "클라우드"},
                {"footer.controls.lang", "언어"},
                {"footer.controls.exit", "종료"},
                {"footer.controls", "A: 선택  |  X: 클라우드  |  Y: 언어  |  L/R: 페이지  |  +: 종료"},
                {"footer.game_count", "개 게임"},
                {"detail.upload", "업로드"},
                {"detail.download", "받기"},
                {"detail.backup", "로컬 백업"},
                {"detail.history", "이력보기"},
                {"detail.back", "뒤로"},
                {"detail.save_size", "세이브 크기"},
                {"detail.title_id", "Title ID"},
                {"detail.recent_backup", "최근 백업"},
                {"detail.versions", "개 버전"},
                {"detail.no_backup", "없음"},
                {"detail.latest_device", "최근 백업 기기"},
                {"detail.latest_user", "최근 백업 사용자"},
                {"detail.latest_source", "최근 백업 출처"},
                {"detail.current_user_backup", "현재 사용자 백업"},
                {"detail.cloud_inside", "클라우드 안의 백업"},
                {"sync.uploading", "세이브를 클라우드에 업로드하는 중..."},
                {"sync.downloading", "클라우드 세이브를 확인하는 중..."},
                {"sync.syncing", "동기화 진행 중..."},
                {"sync.complete", "동기화가 완료되었습니다."},
                {"sync.syncing_game", "동기화 중..."},
                {"sync.no_uploads", "업로드할 세이브가 없습니다."},
                {"card.state.disconnected", "연결 필요"},
                {"card.state.local_only", "클라우드 없음"},
                {"card.state.up_to_date", "최신"},
                {"card.state.needs_upload", "업로드 필요"},
                {"card.state.cloud_newer", "클라우드 최신"},
                {"auth.title", "Dropbox 연동 설정"},
                {"auth.setup_time", "Switch 안에서 바로 Dropbox 로그인 창을 열고 연동할 수 있습니다."},
                {"auth.next", "다음 단계"},
                {"auth.steps_title", "진행 순서"},
                {"auth.step1", "1. [로그인 열기]를 누르면 Switch에서 Dropbox 로그인 화면이 열립니다."},
                {"auth.step2", "2. 로그인과 권한 승인을 마치면 앱이 콜백을 자동으로 받아옵니다."},
                {"auth.step2_api", "- 범위: App folder"},
                {"auth.step2_access", "- 방식: PKCE + callback"},
                {"auth.step2_name", "- fallback: 짧은 인증 코드"},
                {"auth.step3", "3. 자동 복귀에 실패하면 Dropbox가 보여주는 짧은 코드만 입력하세요."},
                {"auth.step4", "4. [Dropbox 연결]은 자동 복귀 이후 확인용이거나, 직접 입력한 인증 코드 교환용입니다."},
                {"auth.get_link", "로그인 열기"},
                {"auth.connect_code", "인증 코드 입력"},
                {"auth.status", "상태"},
                {"auth.status_waiting_link", "인증 링크를 생성할 준비가 되었습니다"},
                {"auth.status_waiting_code", "링크 준비 완료, 코드 입력 대기 중"},
                {"auth.status_ready_to_connect", "코드 입력 완료, 교환 준비됨"},
                {"auth.status_missing_config", "이 빌드에는 DROPBOX_APP_KEY가 없습니다"},
                {"auth.waiting_link_hint", "먼저 [로그인 열기]를 눌러 Switch 안에서 Dropbox 로그인을 시작하세요."},
                {"auth.waiting_code_hint", "브라우저를 열었습니다. 자동 복귀가 안 되면 짧은 인증 코드만 입력하세요."},
                {"auth.ready_to_connect_hint", "인증 정보가 준비되었습니다. [Dropbox 연결]로 토큰 교환을 마무리하세요."},
                {"auth.missing_config_hint", "이 빌드는 DROPBOX_APP_KEY 없이 만들어져 Dropbox OAuth를 시작할 수 없습니다."},
                {"auth.input_label", "인증 코드"},
                {"auth.token_placeholder", "여기에 Dropbox 인증 코드를 붙여넣기..."},
                {"auth.connect", "Dropbox 연결"},
                {"auth.cancel", "취소"},
                {"auth.tip", "PKCE 1회 설정 후에는 저장된 refresh token을 재사용합니다."},
                {"auth.browser_opened", "Dropbox 로그인 창을 열었습니다. 승인 후 자동으로 돌아오거나 짧은 인증 코드를 입력하세요."},
                {"auth.browser_failed", "브라우저를 열지 못했습니다. 다시 시도하거나 짧은 인증 코드를 입력하세요."},
                {"history.title", "버전 이력 목록"},
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
                {"ui.save_browser", "세이브 브라우저"},
                {"ui.section_titles", "설치된 타이틀"},
                {"ui.save_actions", "세이브 작업"},
                {"ui.actions_hint", "A 확인   B 닫기"},
                {"ui.pick_user", "세이브 대상 선택"},
                {"ui.pick_user_hint", "여기서 사용자 세이브와 디바이스 세이브를 전환합니다."},
                {"ui.mode_user", "사용자 세이브"},
                {"ui.mode_device", "디바이스 세이브"},
                {"ui.current", "현재 선택"},
                {"ui.selection_footer", "선택한 대상에 맞게 타이틀 목록을 다시 스캔합니다."},
                {"ui.user_changed", "선택 대상을 변경했습니다."},
                {"ui.no_user", "사용자 없음"},
                {"ui.no_save_entries", "표시할 세이브가 없습니다."},
                {"ui.no_revision_entries", "표시할 이력이 없습니다."},
                {"ui.refresh", "새로고침"},
                {"ui.revision", "버전"},
                {"ui.device", "기기"},
                {"ui.source", "출처"},
                {"ui.size", "크기"},
                {"ui.empty", "없음"},
                {"ui.cloud_ready", "클라우드"},
                {"ui.cloud_need_connect", "연결 필요"},
                {"ui.url", "로그인 링크"},
                {"ui.logout", "로그아웃"},
                {"ui.confirm", "확인"},
                {"ui.auth_failed", "Dropbox 인증 교환에 실패했습니다."},
                {"ui.auth_footer", "Switch에서 로그인 창을 열고, 돌아온 뒤 콜백 URL 또는 짧은 코드를 붙여넣으세요."},
                {"ui.fullscreen_hint", "전체 화면 레이아웃 적용됨"},
                {"ui.action_backup_hint", "로컬 백업 생성"},
                {"ui.action_upload_hint", "최신 백업을 Dropbox에 업로드"},
                {"ui.action_history_hint", "로컬 백업 이력 열기"},
                {"ui.action_cloud_hint", "클라우드 백업 이력 열기"},
            });
            return;
        }

        assign({
            {"app.name", "OC Save Keeper"},
            {"app.slogan", "Safe save backup and cross-device sync"},
            {"status.connected", "Connected"},
            {"status.disconnected", "Not connected"},
            {"footer.controls.open", "Open"},
            {"footer.controls.sync_all", "Cloud"},
            {"footer.controls.lang", "Language"},
            {"footer.controls.exit", "Exit"},
            {"footer.controls", "A: Select  |  X: Cloud  |  Y: Language  |  L/R: Page  |  +: Exit"},
            {"footer.game_count", " games"},
            {"detail.upload", "Upload"},
            {"detail.download", "Fetch"},
            {"detail.backup", "Local Backup"},
            {"detail.history", "Version History List"},
            {"detail.back", "Back"},
            {"detail.save_size", "Save Size"},
            {"detail.title_id", "Title ID"},
            {"detail.recent_backup", "Recent Backup"},
            {"detail.versions", " versions"},
            {"detail.no_backup", "None"},
            {"detail.latest_device", "Latest backup device"},
            {"detail.latest_user", "Latest backup user"},
            {"detail.latest_source", "Latest backup source"},
            {"detail.current_user_backup", "Current user backup"},
            {"detail.cloud_inside", "Backup stored in cloud"},
            {"sync.uploading", "Uploading save data to the cloud..."},
            {"sync.downloading", "Checking cloud save metadata..."},
            {"sync.syncing", "Syncing..."},
            {"sync.complete", "Sync complete."},
            {"sync.syncing_game", "Syncing..."},
            {"sync.no_uploads", "No uploads needed."},
            {"card.state.disconnected", "Not connected"},
            {"card.state.local_only", "No cloud backup"},
            {"card.state.up_to_date", "Up to date"},
            {"card.state.needs_upload", "Needs upload"},
            {"card.state.cloud_newer", "Cloud newer"},
            {"auth.title", "Dropbox Setup"},
            {"auth.setup_time", "Open Dropbox sign-in directly on the Switch and finish setup here."},
            {"auth.next", "Next"},
            {"auth.steps_title", "How It Works"},
            {"auth.step1", "1. Press [Open Sign-In] to open Dropbox sign-in on the Switch."},
            {"auth.step2", "2. Finish sign-in and approval, then let the app capture the callback automatically."},
            {"auth.step2_api", "- Scope: App folder"},
            {"auth.step2_access", "- OAuth: PKCE + callback"},
            {"auth.step2_name", "- fallback: short authorization code"},
            {"auth.step3", "3. If auto return fails, enter only the short authorization code shown by Dropbox."},
            {"auth.step4", "4. [Connect Dropbox] confirms the automatic return or exchanges the code you entered manually."},
            {"auth.get_link", "Open Sign-In"},
            {"auth.connect_code", "Enter Code"},
            {"auth.status", "Status"},
            {"auth.status_waiting_link", "Ready to generate an authorization link"},
            {"auth.status_waiting_code", "Link ready, waiting for pasted code"},
            {"auth.status_ready_to_connect", "Code captured, ready to exchange"},
            {"auth.status_missing_config", "Build is missing DROPBOX_APP_KEY"},
            {"auth.waiting_link_hint", "Start Dropbox sign-in on the Switch with [Open Sign-In]."},
            {"auth.waiting_code_hint", "The browser is open. If it does not return automatically, enter the short authorization code only."},
            {"auth.ready_to_connect_hint", "Authorization data is ready. Press [Connect Dropbox] to finish the token exchange."},
            {"auth.missing_config_hint", "This build cannot start Dropbox OAuth until it is rebuilt with DROPBOX_APP_KEY."},
            {"auth.input_label", "Authorization code"},
            {"auth.token_placeholder", "Paste the Dropbox authorization code here..."},
            {"auth.connect", "Connect Dropbox"},
            {"auth.cancel", "Cancel"},
            {"auth.tip", "One-time PKCE setup, then the saved refresh token is reused."},
            {"auth.browser_opened", "Dropbox sign-in opened. Approve access, then return automatically or enter the short authorization code."},
            {"auth.browser_failed", "Could not open the browser. Retry or enter the short authorization code manually."},
            {"history.title", "Version History List"},
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
            {"ui.save_browser", "Save Browser"},
            {"ui.section_titles", "Installed Titles"},
            {"ui.save_actions", "Save Actions"},
            {"ui.actions_hint", "A Confirm   B Close"},
            {"ui.pick_user", "Choose Save Target"},
            {"ui.pick_user_hint", "Switch between user saves and device saves here."},
            {"ui.mode_user", "User saves"},
            {"ui.mode_device", "Device saves"},
            {"ui.current", "Current"},
            {"ui.selection_footer", "The title list will rescan for the selected save type."},
            {"ui.user_changed", "Selection changed."},
            {"ui.no_user", "No user"},
            {"ui.no_save_entries", "No save entries"},
            {"ui.no_revision_entries", "No revision entries"},
            {"ui.refresh", "Refresh"},
            {"ui.revision", "Revision"},
            {"ui.device", "Device"},
            {"ui.source", "Source"},
            {"ui.size", "Size"},
            {"ui.empty", "Empty"},
            {"ui.cloud_ready", "Cloud"},
            {"ui.cloud_need_connect", "Connect"},
            {"ui.url", "Sign-in URL"},
            {"ui.logout", "Logout"},
            {"ui.confirm", "Confirm"},
            {"ui.auth_failed", "Dropbox authorization exchange failed."},
            {"ui.auth_footer", "Open sign-in on the Switch, then paste the callback URL or short code."},
            {"ui.fullscreen_hint", "Full-screen layout active"},
            {"ui.action_backup_hint", "Create a local backup"},
            {"ui.action_upload_hint", "Upload the latest backup to Dropbox"},
            {"ui.action_history_hint", "Open local revision history"},
            {"ui.action_cloud_hint", "Open cloud revision history"},
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
