/**
 * oc-save-keeper - Dropbox Save Sync for Nintendo Switch
 * Scoped Save Mount - RAII wrapper for save data mounting
 * 100% Aligned with JKSV's mounting and commitment logic
 */

#pragma once

#include <string>
#include <switch.h>
#include "utils/Logger.hpp"

namespace fs {

/**
 * RAII wrapper for save data filesystem mounting
 * Automatically unmounts when scope exits.
 * Implementation follows JKSV's fslib and ScopedSaveMount patterns exactly.
 */
class ScopedSaveMount {
public:
    /**
     * Mount a save data filesystem using JKSV's exact parameter sequence
     */
    ScopedSaveMount(const std::string& mountName, uint64_t titleId, AccountUid uid, bool isDevice = false)
        : m_mountName(mountName)
        , m_isOpen(false) {
        
#ifdef __SWITCH__
        // 1. Force unmount first to avoid "Device already exists" (0xffffffff) conflicts
        // JKSV does this to ensure a clean mount point.
        fsdevUnmountDevice(mountName.c_str());

        LOG_INFO("FS: Mounting %s (JKSV Style). Title: %016lX, UID: %016lX%016lX, Device: %d", 
                 mountName.c_str(), titleId, uid.uid[1], uid.uid[0], isDevice);
                 
        FsFileSystem saveFs;
        Result rc;
        
        // 2. Exact FsSaveDataAttribute configuration as seen in JKSV's save_data_functions.cpp
        FsSaveDataAttribute attr;
        std::memset(&attr, 0, sizeof(attr));
        attr.application_id = titleId;
        attr.uid = uid;
        attr.save_data_type = isDevice ? FsSaveDataType_Device : FsSaveDataType_Account;
        attr.save_data_rank = FsSaveDataRank_Primary; // JKSV default
        attr.save_data_index = 0;                     // JKSV default
        
        // 3. Open the filesystem. JKSV uses FsSaveDataSpaceId_User by default for NAND user saves.
        rc = fsOpenSaveDataFileSystem(&saveFs, FsSaveDataSpaceId_User, &attr);
        
        if (R_SUCCEEDED(rc)) {
            // 4. Mount the device to fsdev
            int ret = fsdevMountDevice(mountName.c_str(), saveFs);
            if (ret != -1) { 
                m_isOpen = true;
                LOG_INFO("FS: Successfully mounted %s", mountName.c_str());
            } else {
                fsFsClose(&saveFs);
                LOG_ERROR("FS: fsdevMountDevice failed for %s", mountName.c_str());
            }
        } else {
            LOG_ERROR("FS: fsOpenSaveDataFileSystem failed: 0x%x (Title: %016lX)", rc, titleId);
        }
#else
        m_isOpen = true;
#endif
    }
    
    ~ScopedSaveMount() {
        if (m_isOpen) {
#ifdef __SWITCH__
            // JKSV pattern: always commit before closing if something changed, 
            // but here we rely on explicit commit() calls for performance.
            fsdevUnmountDevice(m_mountName.c_str());
#endif
        }
    }
    
    // Disable copy
    ScopedSaveMount(const ScopedSaveMount&) = delete;
    ScopedSaveMount& operator=(const ScopedSaveMount&) = delete;
    
    // Enable move
    ScopedSaveMount(ScopedSaveMount&& other) noexcept
        : m_mountName(std::move(other.m_mountName))
        , m_isOpen(other.m_isOpen) {
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
            other.m_isOpen = false;
        }
        return *this;
    }
    
    bool isOpen() const { return m_isOpen; }
    
    std::string getMountPath() const {
        return m_mountName + ":";
    }
    
    /**
     * Physical Commit - 100% same as JKSV's fslib::commit_data_to_file_system
     * This ensures NAND writes are flushed and the journal is cleared.
     */
    bool commit() {
        if (!m_isOpen) return false;
        
#ifdef __SWITCH__
        Result rc = fsdevCommitDevice(m_mountName.c_str());
        if (R_FAILED(rc)) {
            LOG_ERROR("FS: Physical commit failed for %s: 0x%x", m_mountName.c_str(), rc);
            return false;
        }
#endif
        return true;
    }

    const std::string& getMountName() const { return m_mountName; }

private:
    std::string m_mountName;
    bool m_isOpen;
};

} // namespace fs
