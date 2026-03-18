/**
 * oc-save-keeper - Dropbox sync client
 * Dropbox implementation - Phone-friendly OAuth
 * Bridge URL configured via DROPBOX_BRIDGE_BASE env var
 */
#include "network/Dropbox.hpp"
#include "network/DropboxUtil.hpp"
#include "fs/FileUtil.hpp"
#include "utils/Paths.hpp"
#include <fstream>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <json-c/json.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace network {

// RAII wrappers from dropbox namespace
using dropbox::ScopedJson;
using dropbox::ScopedCurlSlist;
using fs::ScopedFile;

// Dropbox API endpoints
static constexpr const char* DROPBOX_AUTHORIZE_URL = "https://www.dropbox.com/oauth2/authorize";
static constexpr const char* DROPBOX_TOKEN_URL = "https://api.dropboxapi.com/oauth2/token";
static constexpr const char* DROPBOX_API_V2 = "https://api.dropboxapi.com/2";
static constexpr const char* DROPBOX_CONTENT = "https://content.dropboxapi.com/2";
static constexpr const char* DROPBOX_LIST_FOLDER = "https://api.dropboxapi.com/2/files/list_folder";
static constexpr const char* DROPBOX_LIST_FOLDER_CONTINUE = "https://api.dropboxapi.com/2/files/list_folder/continue";
static constexpr const char* DROPBOX_REDIRECT_URI = "https://localhost/oc-save-keeper/callback";
#ifndef DROPBOX_BRIDGE_BASE
#define DROPBOX_BRIDGE_BASE "https://example.yourdomain.com"
#endif
static constexpr const char* DROPBOX_BRIDGE_BASE_STR = DROPBOX_BRIDGE_BASE;
static constexpr const char* DROPBOX_AUTH_FILE = utils::paths::DROPBOX_AUTH_JSON;
static constexpr const char* DROPBOX_LEGACY_TOKEN_FILE = utils::paths::DROPBOX_LEGACY_TOKEN;
static constexpr const char* DROPBOX_APP_KEY_FILE = utils::paths::DROPBOX_APP_KEY_TXT;
static constexpr const char* DROPBOX_ROOT_ENV_FILE = utils::paths::ROOT_ENV;
static constexpr const char* DROPBOX_CONFIG_ENV_FILE = utils::paths::CONFIG_ENV;
static constexpr const char* DROPBOX_OLD_AUTH_FILE = "/oc-save-keeper/dropbox_auth.json";
static constexpr const char* DROPBOX_OLD_LEGACY_TOKEN_FILE = "/oc-save-keeper/dropbox_token.txt";

std::string trimCopy(std::string value) {
    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string loadAppKeyCandidate(const char* path) {
    std::ifstream file(path);
    if (!file) {
        return "";
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trimCopy(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        constexpr const char* key = "DROPBOX_APP_KEY=";
        if (line.rfind(key, 0) == 0) {
            std::string value = trimCopy(line.substr(std::strlen(key)));
            if (!value.empty() && value.front() == '"' && value.back() == '"' && value.size() >= 2) {
                value = value.substr(1, value.size() - 2);
            }
            return value;
        }
        return line;
    }

    return "";
}

Dropbox::Dropbox() 
    : m_clientId(loadClientId()), m_tokenExpiresAt(0), m_authenticated(false), m_curl(nullptr) {
    
    m_curl = curl_easy_init();
    
    // Try to load existing token
    if (loadToken()) {
        m_authenticated = true;
    }
}

Dropbox::~Dropbox() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

bool Dropbox::isInternetConnected() {
#ifdef __SWITCH__
    Result rc = nifmInitialize(NifmServiceType_User);
    if (R_SUCCEEDED(rc)) {
        NifmInternetConnectionType type;
        u32 wifiStrength;
        NifmInternetConnectionStatus status;
        rc = nifmGetInternetConnectionStatus(&type, &wifiStrength, &status);
        nifmExit();
        return R_SUCCEEDED(rc) && status == NifmInternetConnectionStatus_Connected;
    }
    return false;
#else
    return true;
#endif
}

bool Dropbox::needsAuthentication() const {
    return !m_authenticated;
}

bool Dropbox::isOAuthConfigured() const {
    return !m_clientId.empty();
}

bool Dropbox::isAuthenticated() const {
    return m_authenticated;
}

// Get the authorization URL - user just clicks this on their phone!
std::string Dropbox::getAuthorizeUrl() {
    if (!isOAuthConfigured()) {
        LOG_ERROR("Dropbox: DROPBOX_APP_KEY is not configured");
        return "";
    }

    m_csrfToken = dropbox::generateRandomHex(16);
    m_codeVerifier = dropbox::generateCodeVerifier(64);
    const std::string codeChallenge = dropbox::buildCodeChallengeS256(m_codeVerifier);

    std::string url = dropbox::buildAuthorizeUrl(
        DROPBOX_AUTHORIZE_URL,
        clientId(),
        DROPBOX_REDIRECT_URI,
        m_csrfToken,
        codeChallenge
    );

    return url;
}

// Poll for authentication - check if user clicked the link and logged in
bool Dropbox::checkAuthentication() {
    return ensureAccessToken();
}

bool Dropbox::startBridgeSession(DropboxBridgeSession& outSession) {
    const std::string url = std::string(DROPBOX_BRIDGE_BASE_STR) + "/v1/sessions/start";
    const std::string postData = "{\"device_id\":\"switch\"}";
    
    std::string response = performRequest(url, postData, "Content-Type: application/json", false);
    if (response.empty()) {
        return false;
    }

    ScopedJson scopedRoot(json_tokener_parse(response.c_str()));
    if (!scopedRoot) return false;
    json_object* root = scopedRoot.get();

    json_object* sidObj = nullptr;
    json_object* pollObj = nullptr;
    json_object* authUrlObj = nullptr;
    json_object* pollUrlObj = nullptr;

    if (json_object_object_get_ex(root, "session_id", &sidObj))
        outSession.sessionId = json_object_get_string(sidObj);
    if (json_object_object_get_ex(root, "poll_token", &pollObj))
        outSession.pollToken = json_object_get_string(pollObj);
    if (json_object_object_get_ex(root, "authorize_url", &authUrlObj))
        outSession.authorizeUrl = json_object_get_string(authUrlObj);
    if (json_object_object_get_ex(root, "poll_url", &pollUrlObj))
        outSession.pollUrl = json_object_get_string(pollUrlObj);

    outSession.active = !outSession.sessionId.empty() && !outSession.pollToken.empty();
    return outSession.active;
}

std::string Dropbox::pollBridgeSession(const DropboxBridgeSession& session) {
    if (!session.active) {
        LOG_ERROR("Dropbox: pollBridgeSession called with inactive session");
        return "expired";
    }

    const std::string url = session.pollUrl.empty() 
        ? std::string(DROPBOX_BRIDGE_BASE_STR) + "/v1/sessions/" + session.sessionId + "/status"
        : session.pollUrl;
    const std::string postData = "{\"poll_token\":\"" + session.pollToken + "\"}";

    LOG_DEBUG("Dropbox: Polling session status - URL: %s", url.c_str());
    std::string response = performRequest(url, postData, "Content-Type: application/json", false);
    
    if (response.empty()) {
        LOG_ERROR("Dropbox: Empty response from session status endpoint");
        return "failed";
    }
    
    LOG_DEBUG("Dropbox: Session status response: %s", response.c_str());

    ScopedJson scopedRoot(json_tokener_parse(response.c_str()));
    if (!scopedRoot) {
        LOG_ERROR("Dropbox: Failed to parse session status JSON: %s", response.c_str());
        return "failed";
    }
    json_object* root = scopedRoot.get();

    std::string status = "failed";
    json_object* statusObj = nullptr;
    if (json_object_object_get_ex(root, "status", &statusObj)) {
        status = json_object_get_string(statusObj);
    }
    
    LOG_DEBUG("Dropbox: Session status = %s", status.c_str());

    return status;
}

bool Dropbox::consumeBridgeSession(const DropboxBridgeSession& session) {
    if (!session.active) return false;

    const std::string url = std::string(DROPBOX_BRIDGE_BASE_STR) + "/v1/sessions/" + session.sessionId + "/consume";
    const std::string postData = "{\"poll_token\":\"" + session.pollToken + "\"}";

    std::string response = performRequest(url, postData, "Content-Type: application/json", false);
    if (response.empty()) return false;

    ScopedJson scopedRoot(json_tokener_parse(response.c_str()));
    if (!scopedRoot) return false;
    json_object* root = scopedRoot.get();

    json_object *codeObj, *verifierObj, *stateObj, *redirectObj, *endpointObj;
    bool ok = json_object_object_get_ex(root, "authorization_code", &codeObj) &&
              json_object_object_get_ex(root, "code_verifier", &verifierObj) &&
              json_object_object_get_ex(root, "state", &stateObj) &&
              json_object_object_get_ex(root, "redirect_uri", &redirectObj) &&
              json_object_object_get_ex(root, "token_endpoint", &endpointObj);

    if (!ok) {
        LOG_ERROR("Dropbox: Bridge consume response missing fields");
        return false;
    }

    // Now exchange the code for token
    std::string authCode = json_object_get_string(codeObj);
    std::string verifier = json_object_get_string(verifierObj);
    std::string redirectUri = json_object_get_string(redirectObj);
    std::string tokenEndpoint = json_object_get_string(endpointObj);

    LOG_INFO("Dropbox: Exchanging code with redirect_uri: %s", redirectUri.c_str());

    std::string tokenPostData =
        "grant_type=authorization_code"
        "&code=" + urlEncode(authCode) +
        "&code_verifier=" + urlEncode(verifier) +
        "&client_id=" + urlEncode(clientId()) +
        "&redirect_uri=" + urlEncode(redirectUri);

    std::string tokenResponse = performRequest(
        tokenEndpoint,
        tokenPostData,
        "Content-Type: application/x-www-form-urlencoded",
        false
    );

    // scopedRoot will be freed automatically

    if (tokenResponse.empty()) return false;

    json_object* tokenRoot = json_tokener_parse(tokenResponse.c_str());
    if (!tokenRoot) return false;

    json_object *accessTokenObj, *refreshTokenObj, *expiresInObj;
    const bool hasAccess = json_object_object_get_ex(tokenRoot, "access_token", &accessTokenObj);
    const bool hasRefresh = json_object_object_get_ex(tokenRoot, "refresh_token", &refreshTokenObj);

    if (!hasAccess || !hasRefresh) {
        json_object_put(tokenRoot);
        return false;
    }

    m_accessToken = json_object_get_string(accessTokenObj);
    m_refreshToken = json_object_get_string(refreshTokenObj);
    if (json_object_object_get_ex(tokenRoot, "expires_in", &expiresInObj)) {
        m_tokenExpiresAt = std::time(nullptr) + json_object_get_int64(expiresInObj);
    } else {
        m_tokenExpiresAt = 0;
    }

    json_object_put(tokenRoot);
    m_authenticated = !m_accessToken.empty();
    if (!m_authenticated) return false;
    
    return saveToken();
}

bool Dropbox::exchangeAuthorizationCode(const std::string& input) {
    if (!isOAuthConfigured()) {
        LOG_ERROR("Dropbox: DROPBOX_APP_KEY is not configured");
        return false;
    }

    if (m_codeVerifier.empty() || m_csrfToken.empty()) {
        LOG_ERROR("Dropbox: PKCE session not initialized");
        return false;
    }

    const std::string state = dropbox::extractQueryParam(input, "state");
    if (!state.empty() && state != m_csrfToken) {
        LOG_ERROR("Dropbox: OAuth state mismatch");
        return false;
    }

    const std::string authCode = dropbox::extractAuthorizationCode(input);
    if (authCode.empty()) {
        LOG_ERROR("Dropbox: Authorization code is empty");
        return false;
    }

    std::string postData =
        "grant_type=authorization_code"
        "&code=" + urlEncode(authCode) +
        "&code_verifier=" + urlEncode(m_codeVerifier) +
        "&client_id=" + urlEncode(clientId());

    if (std::strlen(DROPBOX_REDIRECT_URI) != 0) {
        postData += "&redirect_uri=" + urlEncode(DROPBOX_REDIRECT_URI);
    }

    std::string response = performRequest(
        DROPBOX_TOKEN_URL,
        postData,
        "Content-Type: application/x-www-form-urlencoded",
        false
    );
    if (response.empty()) {
        return false;
    }

    ScopedJson scopedRoot(json_tokener_parse(response.c_str()));
    if (!scopedRoot) {
        LOG_ERROR("Dropbox: OAuth token response parse failed");
        return false;
    }
    json_object* root = scopedRoot.get();

    json_object* accessTokenObj = nullptr;
    json_object* refreshTokenObj = nullptr;
    json_object* expiresInObj = nullptr;
    const bool hasAccess = json_object_object_get_ex(root, "access_token", &accessTokenObj);
    const bool hasRefresh = json_object_object_get_ex(root, "refresh_token", &refreshTokenObj);

    if (!hasAccess || !hasRefresh) {
        LOG_ERROR("Dropbox: OAuth token response missing required fields");
        return false;
    }

    m_accessToken = json_object_get_string(accessTokenObj);
    m_refreshToken = json_object_get_string(refreshTokenObj);
    if (json_object_object_get_ex(root, "expires_in", &expiresInObj)) {
        m_tokenExpiresAt = std::time(nullptr) + json_object_get_int64(expiresInObj);
    } else {
        m_tokenExpiresAt = 0;
    }

    m_authenticated = !m_accessToken.empty();
    m_codeVerifier.clear();
    m_csrfToken.clear();
    if (!m_authenticated) {
        return false;
    }
    return saveToken();
}

void Dropbox::cancelPendingAuthorization() {
    m_codeVerifier.clear();
    m_csrfToken.clear();
}

bool Dropbox::loadToken() {
    utils::paths::ensureBaseDirectories();
    
    // Try auth file (new or old location)
    {
        ScopedFile file(fopen(DROPBOX_AUTH_FILE, "r"));
        if (!file) {
            file = ScopedFile(fopen(DROPBOX_OLD_AUTH_FILE, "r"));
        }
        if (file) {
            std::string content;
            char buffer[512];
            while (fgets(buffer, sizeof(buffer), file.get())) {
                content += buffer;
            }

            ScopedJson root(json_tokener_parse(content.c_str()));
            if (root) {
                json_object* accessObj = nullptr;
                json_object* refreshObj = nullptr;
                json_object* expiresObj = nullptr;

                if (json_object_object_get_ex(root.get(), "access_token", &accessObj)) {
                    m_accessToken = json_object_get_string(accessObj);
                }
                if (json_object_object_get_ex(root.get(), "refresh_token", &refreshObj)) {
                    m_refreshToken = json_object_get_string(refreshObj);
                }
                if (json_object_object_get_ex(root.get(), "expires_at", &expiresObj)) {
                    m_tokenExpiresAt = static_cast<std::time_t>(json_object_get_int64(expiresObj));
                }

                if (!m_accessToken.empty()) {
                    return true;
                }
            }
        }
    }

    // Try legacy token file
    {
        ScopedFile file(fopen(DROPBOX_LEGACY_TOKEN_FILE, "r"));
        if (!file) {
            file = ScopedFile(fopen(DROPBOX_OLD_LEGACY_TOKEN_FILE, "r"));
        }
        if (!file) {
            return false;
        }

        char token[513];
        if (!fgets(token, sizeof(token), file.get())) {
            return false;
        }

        token[strcspn(token, "\n")] = 0;
        m_accessToken = token;
        m_refreshToken.clear();
        m_tokenExpiresAt = 0;
        if (!m_accessToken.empty()) {
            saveToken();
            remove(DROPBOX_LEGACY_TOKEN_FILE);
            remove(DROPBOX_OLD_LEGACY_TOKEN_FILE);
        }
        return !m_accessToken.empty();
    }
}

bool Dropbox::saveToken() {
    utils::paths::ensureBaseDirectories();
    ScopedJson root(json_object_new_object());
    if (!root) {
        return false;
    }
    json_object_object_add(root.get(), "access_token", json_object_new_string(m_accessToken.c_str()));
    json_object_object_add(root.get(), "refresh_token", json_object_new_string(m_refreshToken.c_str()));
    json_object_object_add(root.get(), "expires_at", json_object_new_int64(static_cast<long long>(m_tokenExpiresAt)));

    const char* text = json_object_to_json_string_ext(root.get(), JSON_C_TO_STRING_PLAIN);
    ScopedFile file(fopen(DROPBOX_AUTH_FILE, "w"));
    if (!file) {
        return false;
    }

    const int writeResult = fprintf(file.get(), "%s\n", text);
    return writeResult > 0;
}

void Dropbox::logout() {
    m_accessToken.clear();
    m_refreshToken.clear();
    m_tokenExpiresAt = 0;
    cancelPendingAuthorization();
    m_authenticated = false;
    remove(DROPBOX_AUTH_FILE);
    remove(DROPBOX_LEGACY_TOKEN_FILE);
    remove(DROPBOX_OLD_AUTH_FILE);
    remove(DROPBOX_OLD_LEGACY_TOKEN_FILE);
}

std::string Dropbox::clientId() const {
    return m_clientId;
}

std::string Dropbox::loadClientId() const {
    if (std::strlen(CLIENT_ID) != 0) {
        return CLIENT_ID;
    }

    const char* candidates[] = {
        DROPBOX_APP_KEY_FILE,
        DROPBOX_ROOT_ENV_FILE,
        DROPBOX_CONFIG_ENV_FILE,
    };
    for (const char* path : candidates) {
        const std::string value = loadAppKeyCandidate(path);
        if (!value.empty()) {
            return value;
        }
    }
    return "";
}

// Upload file to Dropbox
bool Dropbox::uploadFile(const std::string& localPath, 
                         const std::string& dropboxPath,
                         std::function<void(size_t, size_t)> progress) {
    (void)progress;
    
    LOG_INFO("Dropbox: uploadFile called: %s -> %s", localPath.c_str(), dropboxPath.c_str());
    
    if (!ensureAccessToken()) {
        LOG_ERROR("Dropbox: Not authenticated");
        return false;
    }
    
    // Read file
    ScopedFile file(fopen(localPath.c_str(), "rb"));
    if (!file) {
        LOG_ERROR("Dropbox: Cannot open file: %s", localPath.c_str());
        return false;
    }
    
    fseek(file.get(), 0, SEEK_END);
    long fileSize = ftell(file.get());
    fseek(file.get(), 0, SEEK_SET);
    
    // Build API arguments header
    std::string apiArg = dropbox::buildUploadArg(dropboxPath);
    
    // Setup curl for upload
    struct curl_slist* rawHeaders = nullptr;
    rawHeaders = curl_slist_append(rawHeaders, ("Authorization: Bearer " + m_accessToken).c_str());
    rawHeaders = curl_slist_append(rawHeaders, ("Dropbox-API-Arg: " + apiArg).c_str());
    rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/octet-stream");
    ScopedCurlSlist headers(rawHeaders);
    
    curl_easy_reset(m_curl);
    const std::string uploadUrl = std::string(DROPBOX_CONTENT) + "/files/upload";
    curl_easy_setopt(m_curl, CURLOPT_URL, uploadUrl.c_str());
    curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, file.get());
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)fileSize);
    
    std::string response;
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(m_curl);
    long httpCode = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    if (res != CURLE_OK) {
        LOG_ERROR("Dropbox: Upload failed: %s", curl_easy_strerror(res));
        return false;
    }

    if (!dropbox::isSuccessfulHttpStatus(httpCode)) {
        LOG_ERROR("Dropbox: Upload HTTP %ld response: %s", httpCode, response.c_str());
        return false;
    }
    
    LOG_INFO("Dropbox: Upload succeeded: %s -> %s", localPath.c_str(), dropboxPath.c_str());
    return true;
}

// Download file from Dropbox
bool Dropbox::downloadFile(const std::string& dropboxPath,
                           const std::string& localPath,
                           std::function<void(size_t, size_t)> progress) {
    (void)progress;
    
    if (!ensureAccessToken()) {
        LOG_ERROR("Dropbox: Not authenticated");
        return false;
    }
    
    // Build API arguments header
    std::string apiArg = dropbox::buildDownloadArg(dropboxPath);
    
    struct curl_slist* rawHeaders = nullptr;
    rawHeaders = curl_slist_append(rawHeaders, ("Authorization: Bearer " + m_accessToken).c_str());
    rawHeaders = curl_slist_append(rawHeaders, ("Dropbox-API-Arg: " + apiArg).c_str());
    ScopedCurlSlist headers(rawHeaders);
    
    ScopedFile file(fopen(localPath.c_str(), "wb"));
    if (!file) {
        LOG_ERROR("Dropbox: Cannot create file: %s", localPath.c_str());
        return false;
    }
    
    curl_easy_reset(m_curl);
    const std::string downloadUrl = std::string(DROPBOX_CONTENT) + "/files/download";
    curl_easy_setopt(m_curl, CURLOPT_URL, downloadUrl.c_str());
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, file.get());
    
    CURLcode res = curl_easy_perform(m_curl);
    long httpCode = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    if (res != CURLE_OK) {
        LOG_ERROR("Dropbox: Download failed: %s", curl_easy_strerror(res));
        remove(localPath.c_str());
        return false;
    }

    if (!dropbox::isSuccessfulHttpStatus(httpCode)) {
        LOG_ERROR("Dropbox: Download HTTP %ld for path %s", httpCode, dropboxPath.c_str());
        remove(localPath.c_str());
        return false;
    }

    // Log successful download with file size
    struct stat st;
    if (stat(localPath.c_str(), &st) == 0) {
        LOG_DEBUG("Download complete: %s -> %s (HTTP %ld, %ld bytes)", 
                  dropboxPath.c_str(), localPath.c_str(), httpCode, st.st_size);
    } else {
        LOG_DEBUG("Download complete: %s -> %s (HTTP %ld)", 
                  dropboxPath.c_str(), localPath.c_str(), httpCode);
    }
    
    return true;
}

// List folder contents
std::vector<DropboxFile> Dropbox::listFolder(const std::string& path, bool recursive) {
    std::vector<DropboxFile> files;
    
    if (!ensureAccessToken()) {
        LOG_ERROR("Dropbox: Not authenticated");
        return files;
    }
    
    std::string response = performRequest(DROPBOX_LIST_FOLDER, dropbox::buildListFolderRequest(path, recursive), "", true);
    if (response.empty()) {
        return files;
    }

    std::string cursor;
    bool hasMore = false;
    if (!appendListFolderPage(response, files, &cursor, &hasMore)) {
        return {};
    }

    while (hasMore && !cursor.empty()) {
        const std::string continueRequest = std::string("{\"cursor\":\"") + cursor + "\"}";
        response = performRequest(DROPBOX_LIST_FOLDER_CONTINUE, continueRequest, "", true);
        if (response.empty()) {
            break;
        }
        if (!appendListFolderPage(response, files, &cursor, &hasMore)) {
            break;
        }
    }

    return files;
}

bool Dropbox::fileExists(const std::string& dropboxPath) {
    if (!dropbox::hasFileComponent(dropboxPath)) {
        return false;
    }

    auto files = listFolder(dropbox::parentPath(dropboxPath));
    std::string filename = dropbox::fileName(dropboxPath);
    
    for (const auto& file : files) {
        if (file.name == filename) return true;
    }
    return false;
}

bool Dropbox::createFolder(const std::string& path) {
    if (!ensureAccessToken()) return false;
    
    std::string postData = dropbox::buildCreateFolderRequest(path);
    
    struct curl_slist* rawHeaders = nullptr;
    rawHeaders = curl_slist_append(rawHeaders, ("Authorization: Bearer " + m_accessToken).c_str());
    rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/json");
    ScopedCurlSlist headers(rawHeaders);
    
    std::string response = performRequest(std::string(DROPBOX_API_V2) + "/files/create_folder_v2", postData, "", true);
    
    return !response.empty();
}

bool Dropbox::deleteFile(const std::string& dropboxPath) {
    if (!ensureAccessToken()) return false;
    
    std::string postData = dropbox::buildDeleteRequest(dropboxPath);
    
    struct curl_slist* rawHeaders = nullptr;
    rawHeaders = curl_slist_append(rawHeaders, ("Authorization: Bearer " + m_accessToken).c_str());
    rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/json");
    ScopedCurlSlist headers(rawHeaders);
    
    std::string response = performRequest(std::string(DROPBOX_API_V2) + "/files/delete_v2", postData, "", true);
    
    return !response.empty();
}

bool Dropbox::ensureAccessToken() {
    if (!m_authenticated || m_accessToken.empty()) {
        return false;
    }

    if (m_refreshToken.empty() || m_tokenExpiresAt == 0) {
        return true;
    }

    const std::time_t now = std::time(nullptr);
    if (m_tokenExpiresAt - now > 60) {
        return true;
    }

    return refreshToken();
}

bool Dropbox::refreshToken() {
    if (!isOAuthConfigured()) {
        LOG_ERROR("Dropbox: DROPBOX_APP_KEY is not configured");
        return false;
    }

    if (m_refreshToken.empty()) {
        return false;
    }

    std::string postData =
        "grant_type=refresh_token"
        "&refresh_token=" + urlEncode(m_refreshToken) +
        "&client_id=" + urlEncode(CLIENT_ID);

    std::string response = performRequest(
        DROPBOX_TOKEN_URL,
        postData,
        "Content-Type: application/x-www-form-urlencoded",
        false
    );

    if (response.empty()) {
        return false;
    }

    json_object* root = json_tokener_parse(response.c_str());
    if (!root) {
        LOG_ERROR("Dropbox: Refresh token response parse failed");
        return false;
    }

    json_object* accessTokenObj = nullptr;
    json_object* expiresInObj = nullptr;
    json_object* refreshTokenObj = nullptr;
    const bool hasAccess = json_object_object_get_ex(root, "access_token", &accessTokenObj);
    if (!hasAccess) {
        json_object_put(root);
        return false;
    }

    m_accessToken = json_object_get_string(accessTokenObj);
    if (json_object_object_get_ex(root, "expires_in", &expiresInObj)) {
        m_tokenExpiresAt = std::time(nullptr) + json_object_get_int64(expiresInObj);
    }

    if (json_object_object_get_ex(root, "refresh_token", &refreshTokenObj)) {
        const std::string rotatedRefresh = json_object_get_string(refreshTokenObj);
        if (!rotatedRefresh.empty()) {
            m_refreshToken = rotatedRefresh;
        }
    }

    json_object_put(root);
    m_authenticated = !m_accessToken.empty();
    if (!m_authenticated) {
        return false;
    }
    return saveToken();
}

bool Dropbox::appendListFolderPage(const std::string& response,
                                   std::vector<DropboxFile>& files,
                                   std::string* outCursor,
                                   bool* outHasMore) {
    json_object* root = json_tokener_parse(response.c_str());
    if (!root) {
        LOG_ERROR("Dropbox: listFolder JSON parse failed");
        return false;
    }

    json_object* entriesObj = nullptr;
    if (!json_object_object_get_ex(root, "entries", &entriesObj) ||
        !json_object_is_type(entriesObj, json_type_array)) {
        json_object_put(root);
        return false;
    }

    json_object* cursorObj = nullptr;
    json_object* hasMoreObj = nullptr;
    if (outCursor) {
        outCursor->clear();
        if (json_object_object_get_ex(root, "cursor", &cursorObj)) {
            *outCursor = json_object_get_string(cursorObj);
        }
    }
    if (outHasMore) {
        *outHasMore = json_object_object_get_ex(root, "has_more", &hasMoreObj) &&
                      json_object_get_boolean(hasMoreObj);
    }

    const int entryCount = json_object_array_length(entriesObj);
    files.reserve(files.size() + static_cast<std::size_t>(entryCount));
    for (int i = 0; i < entryCount; ++i) {
        json_object* entry = json_object_array_get_idx(entriesObj, i);
        if (!entry) {
            continue;
        }

        DropboxFile file{};
        json_object* tagObj = nullptr;
        json_object* pathObj = nullptr;
        json_object* nameObj = nullptr;
        json_object* revObj = nullptr;
        json_object* modifiedObj = nullptr;
        json_object* sizeObj = nullptr;

        if (json_object_object_get_ex(entry, ".tag", &tagObj)) {
            const char* tag = json_object_get_string(tagObj);
            file.isFolder = tag && std::strcmp(tag, "folder") == 0;
        }
        if (json_object_object_get_ex(entry, "path_display", &pathObj)) {
            file.path = json_object_get_string(pathObj);
        }
        if (json_object_object_get_ex(entry, "name", &nameObj)) {
            file.name = json_object_get_string(nameObj);
        }
        if (json_object_object_get_ex(entry, "rev", &revObj)) {
            file.rev = json_object_get_string(revObj);
        }
        if (json_object_object_get_ex(entry, "server_modified", &modifiedObj)) {
            file.modifiedTime = dropbox::parseDropboxTime(json_object_get_string(modifiedObj));
        }
        if (json_object_object_get_ex(entry, "size", &sizeObj)) {
            file.size = static_cast<size_t>(json_object_get_int64(sizeObj));
        }

        files.push_back(std::move(file));
    }

    json_object_put(root);
    return true;
}

std::string Dropbox::performRequest(const std::string& url, 
                                    const std::string& postData,
                                    const std::string& authHeader,
                                    bool isApiV2) {
    std::lock_guard<std::mutex> lock(m_curlMutex);
    std::string response;
    
    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    
    struct curl_slist* rawHeaders = nullptr;
    if (!authHeader.empty()) {
        rawHeaders = curl_slist_append(rawHeaders, authHeader.c_str());
    } else if (m_authenticated && !m_accessToken.empty()) {
        rawHeaders = curl_slist_append(rawHeaders, ("Authorization: Bearer " + m_accessToken).c_str());
    }
    if (isApiV2) {
        rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/json");
    }
    ScopedCurlSlist headers(rawHeaders);
    
    if (!postData.empty()) {
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, postData.c_str());
    }
    
    if (headers) {
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers.get());
    }
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "oc-save-keeper/1.0");
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 60L);
    
    CURLcode res = curl_easy_perform(m_curl);
    long httpCode = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    if (res != CURLE_OK) {
        LOG_ERROR("Dropbox: Request failed: %s (url: %s)", curl_easy_strerror(res), url.c_str());
        return "";
    }

    if (!dropbox::isSuccessfulHttpStatus(httpCode)) {
        if (response.find("insufficient_scope") != std::string::npos || response.find("not_permitted") != std::string::npos) {
            LOG_ERROR("Dropbox: PERMISSION ERROR. Please enable 'files.metadata.read' scope in Dropbox App Console.");
        }
        LOG_ERROR("Dropbox: Request HTTP %ld failed for URL: %s response: %s", httpCode, url.c_str(), response.c_str());
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
    if (!m_curl) {
        return str;
    }
    char* encoded = curl_easy_escape(m_curl, str.c_str(), str.length());
    if (!encoded) {
        return "";
    }
    std::string result(encoded);
    curl_free(encoded);
    return result;
}

} // namespace network
