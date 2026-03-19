/**
 * oc-save-keeper - Multi-language support
 * Auto-detects system language on Nintendo Switch
 */

#pragma once

#include <string>
#include <map>
#include <initializer_list>
#include <json-c/json.h>
#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "utils/Paths.hpp"
#include "utils/SettingsStore.hpp"
#include "fs/FileUtil.hpp"

namespace utils {

// ScopedJson is defined in SettingsStore.hpp

class Language {
public:
    static Language& instance() {
        static Language lang;
        return lang;
    }
    
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
        fs::ScopedFile file(std::fopen(path.c_str(), "r"));
        if (!file) {
            path = std::string(utils::paths::ROOT) + "/lang/" + langCode + ".json";
            file = fs::ScopedFile(std::fopen(path.c_str(), "r"));
        }
        
        if (!file) {
            loadBuiltIn(langCode);
            return true;
        }
        
        std::fseek(file.get(), 0, SEEK_END);
        long size = std::ftell(file.get());
        std::fseek(file.get(), 0, SEEK_SET);
        
        std::string buffer(static_cast<std::size_t>(size), '\0');
        if (size > 0) {
            std::fread(buffer.data(), 1, static_cast<std::size_t>(size), file.get());
        }
        
        ScopedJson root(json_tokener_parse(buffer.c_str()));
        
        if (!root) {
            loadBuiltIn(langCode);
            return true;
        }
        
        m_strings.clear();
        
        json_object_object_foreach(root.get(), key, val) {
            m_strings[key] = json_object_get_string(val);
        }
        
        m_currentLang = langCode;
        
        return true;
    }
    
    std::string get(const std::string& key) const {
        auto it = m_strings.find(key);
        if (it != m_strings.end()) {
            return it->second;
        }
        return "[" + key + "]";
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

        fs::ScopedFile f(std::fopen(utils::paths::LANGUAGE_PREF, "r"));
        if (f) {
            char buf[8];
            if (std::fgets(buf, sizeof(buf), f.get())) {
                std::string s(buf);
                if (!s.empty() && s.back() == '\n') s.pop_back();
                return s;
            }
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
                {"status.disconnected", "Dropbox 미연동"},
                {"footer.controls.open", "선택"},
                {"footer.controls.sync_all", "드롭박스"},
                {"footer.controls.lang", "언어"},
                {"footer.controls.exit", "종료"},
                {"footer.game_count", "개 게임"},
                {"detail.upload", "드롭박스 업로드"},
                {"detail.download", "받기"},
                {"detail.backup", "로컬 백업"},
                {"detail.history", "로컬 버전 이력"},
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
                {"detail.cloud_inside", "드롭박스 안의 백업"},
                {"sync.uploading", "세이브를 드롭박스에 업로드하는 중..."},
                {"sync.downloading", "드롭박스 세이브를 확인하는 중..."},
                {"sync.syncing", "동기화 진행 중..."},
                {"sync.complete", "동기화가 완료되었습니다."},
                {"sync.syncing_game", "동기화 중..."},
                {"sync.no_uploads", "업로드할 세이브가 없습니다."},
                {"card.state.disconnected", "백업 없음"},
                {"card.state.local_only", "드롭박스 없음"},
                {"card.state.up_to_date", "최신"},
                {"card.state.needs_upload", "업로드 필요"},
                {"card.state.cloud_newer", "드롭박스 최신"},
                {"auth.title", "Dropbox 연동 설정"},
                {"auth.start_login", "로그인 시작 (QR 생성)"},
                {"auth.status_authenticated", "이미 Dropbox에 연결되어 있습니다."},
                {"auth.status_ready", "드롭박스 연결을 시작할 준비가 되었습니다."},
                {"auth.status_starting_session", "로그인 세션을 시작하는 중..."},
                {"auth.status_waiting_scan", "휴대폰으로 QR 코드를 스캔하여 로그인하세요."},
                {"auth.status_bridge_failed", "로그인 세션을 시작하지 못했습니다. 인터넷 연결을 확인하세요."},
                {"auth.status_approved", "인증되었습니다! 연결을 마무리하는 중..."},
                {"auth.status_failed", "세션이 만료되었거나 실패했습니다. 다시 시도해 주세요."},
                {"auth.status_expired", "세션이 만료되었습니다. 다시 시도해 주세요."},
                {"auth.status_success", "Dropbox에 성공적으로 연결되었습니다!"},
                {"auth.status_consume_failed", "토큰 교환에 실패했습니다. 다시 시도해 주세요."},
                {"auth.confirm_logout", "정말 Dropbox 연결을 해제하시겠습니까?"},
                {"auth.qr_hint", "스캔하여 로그인"},
                {"auth.qr_failed", "QR 생성 실패"},
                {"auth.waiting_bridge", "서버 응답 대기 중..."},
                {"history.title", "버전 이력 목록"},
                {"history.local", "로컬 백업"},
                {"history.cloud", "드롭박스 이력"},
                {"history.synced", "동기화됨"},
                {"history.local_only", "로컬만"},
                {"history.restore", "복원"},
                {"history.no_backup", "백업 없음"},
                {"history.no_cloud", "드롭박스 백업 없음"},
                {"history.login_needed", "로그인 필요"},
                {"history.unknown_device", "알 수 없는 기기"},
                {"history.unknown_user", "알 수 없는 사용자"},
                {"history.unknown_source", "출처 정보 없음"},
                {"history.source_local", "현재 기기"},
                {"history.source_dropbox", "드롭박스"},
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
                {"ui.retry", "재시도"},
                {"ui.revision", "버전"},
                {"ui.device", "기기"},
                {"ui.source", "출처"},
                {"ui.size", "크기"},
                {"ui.empty", "없음"},
                {"ui.cloud_ready", "드롭박스"},
                {"ui.cloud_need_connect", "연결 필요"},
                {"ui.url", "로그인 링크"},
                {"ui.logout", "로그아웃"},
                {"ui.yes", "예"},
                {"ui.no", "아니요"},
                {"ui.confirm", "확인"},
                {"ui.auth_failed", "Dropbox 인증 교환에 실패했습니다."},
                {"ui.auth_footer_new", "휴대폰으로 QR 코드를 스캔하여 로그인하세요."},
                {"ui.fullscreen_hint", "전체 화면 레이아웃 적용됨"},
                {"ui.action_backup_hint", "로컬 백업 생성"},
                {"ui.action_upload_hint", "로컬 백업을 드롭박스에 업로드"},
                {"ui.action_history_hint", "로컬 백업 이력 열기"},
                {"ui.action_cloud_hint", "드롭박스 백업 이력 열기"},
                {"app.applet_warning_title", "애플릿 모드 감지됨"},
                {"app.applet_warning_message", "현재 앨범 모드로 실행 중입니다."},
                {"app.applet_warning_instruction", "R 버튼을 누른 채 게임을 실행하세요."}
            });
            return;
        }

        assign({
            {"app.name", "OC Save Keeper"},
            {"app.slogan", "Safe save backup and cross-device sync"},
            {"status.connected", "Connected"},
            {"status.disconnected", "Cloud offline"},
            {"footer.controls.open", "Open"},
            {"footer.controls.sync_all", "Cloud"},
            {"footer.controls.lang", "Language"},
            {"footer.controls.exit", "Exit"},
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
            {"card.state.disconnected", "No backup yet"},
            {"card.state.local_only", "No cloud backup"},
            {"card.state.up_to_date", "Up to date"},
            {"card.state.needs_upload", "Needs upload"},
            {"card.state.cloud_newer", "Cloud newer"},
            {"auth.title", "Dropbox Setup"},
            {"auth.start_login", "Start Login"},
            {"auth.status_authenticated", "You are already connected to Dropbox."},
            {"auth.status_ready", "Ready to start Dropbox connection."},
            {"auth.status_starting_session", "Starting login session..."},
            {"auth.status_waiting_scan", "Scan the QR code with your phone to login."},
            {"auth.status_bridge_failed", "Failed to start login session. Check your internet."},
            {"auth.status_approved", "Authorized! Finishing connection..."},
            {"auth.status_failed", "Session expired or failed. Please try again."},
            {"auth.status_expired", "Session expired. Please try again."},
            {"auth.status_success", "Successfully connected to Dropbox!"},
            {"auth.status_consume_failed", "Failed to exchange tokens. Please try again."},
            {"auth.confirm_logout", "Are you sure you want to disconnect from Dropbox?"},
            {"auth.qr_hint", "Scan to sign in"},
            {"auth.qr_failed", "QR Code Failed"},
            {"auth.waiting_bridge", "Waiting for bridge..."},
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
            {"ui.retry", "Retry"},
            {"ui.revision", "Revision"},
            {"ui.device", "Device"},
            {"ui.source", "Source"},
            {"ui.size", "Size"},
            {"ui.empty", "Empty"},
            {"ui.cloud_ready", "Cloud"},
            {"ui.cloud_need_connect", "Connect"},
            {"ui.url", "Sign-in URL"},
            {"ui.logout", "Logout"},
            {"ui.yes", "Yes"},
            {"ui.no", "No"},
            {"ui.confirm", "Confirm"},
            {"ui.auth_failed", "Dropbox authorization exchange failed."},
            {"ui.auth_footer_new", "Use your phone to scan and approve the connection."},
            {"ui.fullscreen_hint", "Full-screen layout active"},
            {"ui.action_backup_hint", "Create a local backup"},
            {"ui.action_upload_hint", "Upload the latest backup to Dropbox"},
            {"ui.action_history_hint", "Open local revision history"},
            {"ui.action_cloud_hint", "Open cloud revision history"},
            {"app.applet_warning_title", "Applet Mode Detected"},
            {"app.applet_warning_message", "You are running in Album mode. Memory and Save access are strictly restricted."},
            {"app.applet_warning_instruction", "Please use 'Title Takeover' (Hold R while launching a game) to use the full memory mode."}
        });
    }
    
    std::string detectSystemLanguage() {
#ifdef __SWITCH__
        Result rc = setInitialize();
        if (R_SUCCEEDED(rc)) {
            u64 languageCode = 0;
            if (R_SUCCEEDED(setGetSystemLanguage(&languageCode))) {
                SetLanguage lang;
                if (R_SUCCEEDED(setMakeLanguage(languageCode, &lang))) {
                    setExit();
                    return (lang == SetLanguage_KO) ? "ko" : "en";
                }
            }
            setExit();
        }
#endif
        return "ko";
    }
    
    std::map<std::string, std::string> m_strings;
    std::string m_currentLang;
};

#define LANG(key) utils::Language::instance().c_str(key)

} // namespace utils
