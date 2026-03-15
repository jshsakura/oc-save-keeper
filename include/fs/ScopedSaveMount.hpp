/**
 * Drop-Keep - Dropbox Save Sync for Nintendo Switch
 * Scoped Save Mount - RAII wrapper for save data mounting
 * Based on JKSV's ScopedSaveMount pattern
 */

#pragma once

#include <string>
#include <switch.h>
#include "utils/Logger.hpp"

namespace fs {

/**
 * RAII wrapper for save data filesystem mounting
 * Automatically unmounts when scope exits
 */
class ScopedSaveMount {
public:
    /**
     * Mount a save data filesystem
     * @param mountName Name to use for mount point (e.g., "save")
     * @param titleId Title ID of the save
     * @param uid User account ID
     * @param log Enable logging
     */
    ScopedSaveMount(const std::string& mountName, uint64_t titleId, AccountUid uid, bool isDevice = false)
        : m_mountName(mountName)
        , m_isOpen(false)
        , m_log(true) {
        
#ifdef __SWITCH__
        // Force unmount first to avoid 0xffffffff conflict
        fsdevUnmountDevice(mountName.c_str());

        LOG_INFO("FS: Mounting %s. Title: %016lX, UID: %016lX%016lX, DeviceMode: %d", 
                 mountName.c_str(), titleId, uid.uid[1], uid.uid[0], isDevice);
                 
        FsFileSystem saveFs;
        Result rc;
        
        FsSaveDataAttribute attr = {0};
        attr.application_id = titleId;
        attr.uid = uid;
        attr.save_data_type = isDevice ? FsSaveDataType_Device : FsSaveDataType_Account;
        
        rc = fsOpenSaveDataFileSystem(&saveFs, FsSaveDataSpaceId_User, &attr);
        
        if (R_SUCCEEDED(rc)) {
            int ret = fsdevMountDevice(mountName.c_str(), saveFs);
            if (ret != -1) { // Success returns device ID (>= 0), failure returns -1
                m_isOpen = true;
                LOG_INFO("FS: Successfully mounted %s (dev id: %d)", mountName.c_str(), ret);
            } else {
                fsFsClose(&saveFs);
                LOG_ERROR("FS: fsdevMountDevice failed: %d, errno: %d", ret, errno);
            }
        } else {
            LOG_ERROR("FS: fsOpenSaveDataFileSystem failed: 0x%x", rc);
        }
#else
        // For non-Switch builds, simulate success
        m_isOpen = true;
#endif
    }
    
    /**
     * Destructor - automatically unmounts
     */
    ~ScopedSaveMount() {
        if (m_isOpen) {
#ifdef __SWITCH__
            fsdevUnmountDevice(m_mountName.c_str());
#endif
        }
    }
    
    // No copy
    ScopedSaveMount(const ScopedSaveMount&) = delete;
    ScopedSaveMount& operator=(const ScopedSaveMount&) = delete;
    
    // Move allowed
    ScopedSaveMount(ScopedSaveMount&& other) noexcept
        : m_mountName(std::move(other.m_mountName))
        , m_isOpen(other.m_isOpen)
        , m_log(other.m_log) {
        other.m_isOpen = false;
    }
    
    ScopedSaveMount& operator=(ScopedSaveMount&& other) noexcept {
        if (this != &other) {
            if (m_isOpen) {
#ifdef __SWITCH__
                fsdevUnmountDevice(m_mountName.c_str());
#endif
            }
            m_mountName = std::move(other.m_mountName);
            m_isOpen = other.m_isOpen;
            m_log = other.m_log;
            other.m_isOpen = false;
        }
        return *this;
    }
    
    /**
     * Check if mount was successful
     */
    bool isOpen() const { return m_isOpen; }
    
    /**
     * Get mount point path (e.g., "save:")
     */
    std::string getMountPath() const {
        return m_mountName + ":";
    }
    
    /**
     * Get full path for a file within the mount
     */
    std::string getPath(const std::string& relativePath) const {
        return m_mountName + ":/" + relativePath;
    }
    
    /**
     * Commit any changes to the save data
     * IMPORTANT: Must be called before unmounting if any writes were made
     */
    bool commit() {
        if (!m_isOpen) return false;
        
#ifdef __SWITCH__
        Result rc = fsdevCommitDevice(m_mountName.c_str());
        if (R_FAILED(rc)) {
            LOG_ERROR("fsdevCommitDevice failed: 0x%x", rc);
            return false;
        }
#endif
        return true;
    }

private:
    std::string m_mountName;
    bool m_isOpen;
    bool m_log;
};

/**
 * Helper to get journal size for a save
 */
inline int64_t getSaveJournalSize(uint64_t titleId) {
    (void)titleId;
    return 0x1000000; // Default 16MB
}

} // namespace fs
