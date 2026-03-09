/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * Dropbox implementation - Phone-friendly OAuth
 */
#include "network/Dropbox.hpp"
#include <fstream>
#include <cstring>

namespace network {

// Dropbox API endpoints
static constexpr const char* DROPBOX_AUTHORIZE_URL = "https://www.dropbox.com/oauth2/authorize";
static constexpr const char* DROPBOX_TOKEN_URL = "https://api.dropboxapi.com/oauth2/token";
static constexpr const char* DROPBOX_API_V2 = "https://api.dropboxapi.com/2";
static constexpr const char* DROPBOX_CONTENT = "https://content.dropboxapi.com/2";

Dropbox::Dropbox() 
    : m_authenticated(false), m_curl(nullptr) {
    
    m_curl = curl_easy_init();
    
    // Try to load existing token
    if (loadToken()) {
        m_authenticated = true;
        LOG_INFO("Dropbox: Loaded existing token");
    }
}

Dropbox::~Dropbox() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

bool Dropbox::needsAuthentication() const {
    return !m_authenticated;
}

bool Dropbox::isAuthenticated() const {
    return m_authenticated;
}

bool Dropbox::setAccessToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    m_accessToken = token;
    m_authenticated = saveToken();
    return m_authenticated;
}

// Get the authorization URL - user just clicks this on their phone!
std::string Dropbox::getAuthorizeUrl() {
    // Generate CSRF token for security
    char csrf[17];
    snprintf(csrf, sizeof(csrf), "%016lx", time(nullptr));
    m_csrfToken = csrf;
    
    // Build authorization URL
    // User just opens this on their phone, logs in, and we're done!
    std::string url = std::string(DROPBOX_AUTHORIZE_URL) +
        "?client_id=" + CLIENT_ID +
        "&response_type=token" +           // Implicit grant - no code exchange needed!
        "&redirect_uri=https://example.com/complete" +  // We'll intercept this
        "&state=" + m_csrfToken;
    
    LOG_INFO("Dropbox: Generated auth URL");
    return url;
}

// Poll for authentication - check if user clicked the link and logged in
bool Dropbox::checkAuthentication() {
    // For Dropbox implicit flow, we need to detect when user completes auth
    // In reality, this requires a local HTTP server or manual token entry
    
    // Alternative: Show user a simple code entry screen after they auth on phone
    // Dropbox shows a short code (6 chars) after auth that user can type in
    
    // For now, let's use the "device code" style where we show:
    // 1. Link to click
    // 2. After clicking, Dropbox shows a code
    // 3. User enters code on Switch
    
    // This is still simpler than Google because:
    // - No Google Cloud Console setup
    // - Just click link → login → enter code
    
    return m_authenticated;
}

// Simplified OAuth using "Generated Access Token" approach
// User creates a Dropbox app token on dropbox.com/developers ONCE
// and pastes it into S.O.S - ONE time setup, works forever
bool Dropbox::loadToken() {
    FILE* file = fopen("/switch/OpenCourse/oc-save-keeper/dropbox_token.txt", "r");
    if (!file) return false;
    
    char token[256];
    if (fgets(token, sizeof(token), file)) {
        // Remove newline
        token[strcspn(token, "\n")] = 0;
        m_accessToken = token;
        fclose(file);
        return !m_accessToken.empty();
    }
    
    fclose(file);
    return false;
}

bool Dropbox::saveToken() {
    FILE* file = fopen("/switch/OpenCourse/oc-save-keeper/dropbox_token.txt", "w");
    if (!file) return false;
    
    fprintf(file, "%s\n", m_accessToken.c_str());
    fclose(file);
    return true;
}

void Dropbox::logout() {
    m_accessToken.clear();
    m_authenticated = false;
    remove("/switch/OpenCourse/oc-save-keeper/dropbox_token.txt");
}

// Upload file to Dropbox
bool Dropbox::uploadFile(const std::string& localPath, 
                         const std::string& dropboxPath,
                         std::function<void(size_t, size_t)> progress) {
    
    if (!m_authenticated) {
        LOG_ERROR("Dropbox: Not authenticated");
        return false;
    }
    
    LOG_INFO("Dropbox: Uploading %s", dropboxPath.c_str());
    
    // Read file
    FILE* file = fopen(localPath.c_str(), "rb");
    if (!file) {
        LOG_ERROR("Dropbox: Cannot open file: %s", localPath.c_str());
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Build API arguments header
    std::string apiArg = "{\"path\":\"" + dropboxPath + "\",\"mode\":\"overwrite\",\"autorename\":false}";
    
    // Setup curl for upload
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + m_accessToken).c_str());
    headers = curl_slist_append(headers, ("Dropbox-API-Arg: " + apiArg).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    
    curl_easy_reset(m_curl);
    const std::string uploadUrl = std::string(DROPBOX_CONTENT) + "/files/upload";
    curl_easy_setopt(m_curl, CURLOPT_URL, uploadUrl.c_str());
    curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, file);
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)fileSize);
    
    std::string response;
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(m_curl);
    fclose(file);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        LOG_ERROR("Dropbox: Upload failed: %s", curl_easy_strerror(res));
        return false;
    }
    
    LOG_INFO("Dropbox: Upload complete");
    return true;
}

// Download file from Dropbox
bool Dropbox::downloadFile(const std::string& dropboxPath,
                           const std::string& localPath,
                           std::function<void(size_t, size_t)> progress) {
    
    if (!m_authenticated) {
        LOG_ERROR("Dropbox: Not authenticated");
        return false;
    }
    
    LOG_INFO("Dropbox: Downloading %s", dropboxPath.c_str());
    
    // Build API arguments header
    std::string apiArg = "{\"path\":\"" + dropboxPath + "\"}";
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + m_accessToken).c_str());
    headers = curl_slist_append(headers, ("Dropbox-API-Arg: " + apiArg).c_str());
    
    FILE* file = fopen(localPath.c_str(), "wb");
    if (!file) {
        LOG_ERROR("Dropbox: Cannot create file: %s", localPath.c_str());
        curl_slist_free_all(headers);
        return false;
    }
    
    curl_easy_reset(m_curl);
    const std::string downloadUrl = std::string(DROPBOX_CONTENT) + "/files/download";
    curl_easy_setopt(m_curl, CURLOPT_URL, downloadUrl.c_str());
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, file);
    
    CURLcode res = curl_easy_perform(m_curl);
    fclose(file);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        LOG_ERROR("Dropbox: Download failed: %s", curl_easy_strerror(res));
        return false;
    }
    
    LOG_INFO("Dropbox: Download complete");
    return true;
}

// List folder contents
std::vector<DropboxFile> Dropbox::listFolder(const std::string& path) {
    std::vector<DropboxFile> files;
    
    if (!m_authenticated) {
        LOG_ERROR("Dropbox: Not authenticated");
        return files;
    }
    
    // Build request
    std::string postData = "{\"path\":\"" + path + "\",\"recursive\":false,\"include_deleted\":false}";
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + m_accessToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    std::string response = performRequest(std::string(DROPBOX_API_V2) + "/files/list_folder", postData, "", true);
    curl_slist_free_all(headers);
    
    // Parse JSON response
    // TODO: Use json-c to parse entries
    
    return files;
}

bool Dropbox::fileExists(const std::string& dropboxPath) {
    auto files = listFolder(dropboxPath.substr(0, dropboxPath.rfind('/')));
    std::string filename = dropboxPath.substr(dropboxPath.rfind('/') + 1);
    
    for (const auto& file : files) {
        if (file.name == filename) return true;
    }
    return false;
}

bool Dropbox::createFolder(const std::string& path) {
    if (!m_authenticated) return false;
    
    std::string postData = "{\"path\":\"" + path + "\"}";
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + m_accessToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    std::string response = performRequest(std::string(DROPBOX_API_V2) + "/files/create_folder_v2", postData, "", true);
    curl_slist_free_all(headers);
    
    return !response.empty();
}

bool Dropbox::deleteFile(const std::string& dropboxPath) {
    if (!m_authenticated) return false;
    
    std::string postData = "{\"path\":\"" + dropboxPath + "\"}";
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + m_accessToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    std::string response = performRequest(std::string(DROPBOX_API_V2) + "/files/delete_v2", postData, "", true);
    curl_slist_free_all(headers);
    
    return !response.empty();
}

std::string Dropbox::performRequest(const std::string& url, 
                                    const std::string& postData,
                                    const std::string& authHeader,
                                    bool isApiV2) {
    std::string response;
    
    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    
    struct curl_slist* headers = nullptr;
    if (!authHeader.empty()) {
        headers = curl_slist_append(headers, authHeader.c_str());
    } else if (m_authenticated && !m_accessToken.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + m_accessToken).c_str());
    }
    if (isApiV2) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    
    if (!postData.empty()) {
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, postData.c_str());
    }
    
    if (headers) {
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // SSL settings
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    CURLcode res = curl_easy_perform(m_curl);
    
    if (headers) curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        LOG_ERROR("Dropbox: Request failed: %s", curl_easy_strerror(res));
        return "";
    }
    
    return response;
}

size_t Dropbox::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

size_t Dropbox::readCallback(void* ptr, size_t size, size_t nmemb, void* userp) {
    return fread(ptr, size, nmemb, static_cast<FILE*>(userp));
}

std::string Dropbox::urlEncode(const std::string& str) {
    char* encoded = curl_easy_escape(m_curl, str.c_str(), str.length());
    std::string result(encoded);
    curl_free(encoded);
    return result;
}

} // namespace network
