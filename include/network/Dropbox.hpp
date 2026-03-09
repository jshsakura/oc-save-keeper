/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * Dropbox client - Simple OAuth, phone-friendly
 */
#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <functional>
#include <curl/curl.h>

#include "utils/Config.hpp"
#include "utils/Logger.hpp"

namespace network {

// Dropbox file item
struct DropboxFile {
    std::string path;           // /DropKeep_Backups/Zelda/2026-03-03.zip
    std::string name;           // 2026-03-03.zip
    std::string rev;            // File revision
    std::time_t modifiedTime;
    size_t size;
    bool isFolder;
};

// Auth state
struct DropboxAuthState {
    std::string authorizeUrl;   // https://www.dropbox.com/oauth2/authorize?...
    std::string csrfToken;      // State parameter for security
    bool isWaiting;
    std::string message;
};

class Dropbox {
public:
    Dropbox();
    ~Dropbox();
    
    // Authentication - SUPER SIMPLE!
    bool needsAuthentication() const;
    std::string getAuthorizeUrl();                    // Get URL to show user
    bool checkAuthentication();                       // Check if user authorized (polling)
    bool isAuthenticated() const;
    bool setAccessToken(const std::string& token);
    void logout();
    
    // File operations
    bool uploadFile(const std::string& localPath, 
                    const std::string& dropboxPath,
                    std::function<void(size_t, size_t)> progress = nullptr);
    
    bool downloadFile(const std::string& dropboxPath,
                      const std::string& localPath,
                      std::function<void(size_t, size_t)> progress = nullptr);
    
    bool deleteFile(const std::string& dropboxPath);
    bool createFolder(const std::string& path);
    
    // Listing
    std::vector<DropboxFile> listFolder(const std::string& path = "");
    bool fileExists(const std::string& dropboxPath);
    
    // Getters
    const std::string& getAccessToken() const { return m_accessToken; }
    
private:
    // App credentials (embedded - safe for dropbox.file scope)
    // These are for "Drop-Keep" Dropbox app
    static constexpr const char* CLIENT_ID = "YOUR_DROPBOX_APP_KEY";
    
    // Tokens
    std::string m_accessToken;
    std::string m_csrfToken;
    bool m_authenticated;
    
    // Curl
    CURL* m_curl;
    
    // Internal
    std::string performRequest(const std::string& url, 
                               const std::string& postData = "",
                               const std::string& authHeader = "",
                               bool isApiV2 = false);
    
    bool loadToken();
    bool saveToken();
    bool refreshToken();
    
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t readCallback(void* ptr, size_t size, size_t nmemb, void* userp);
    std::string urlEncode(const std::string& str);
};

} // namespace network
