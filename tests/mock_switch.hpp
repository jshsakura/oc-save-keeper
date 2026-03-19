#pragma once
#ifndef __SWITCH__

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef uint32_t Result;
typedef uint64_t AccountUid;
constexpr uint64_t INVALID_ACCOUNT_UID = uint64_t(-1);

struct FsFileSystem {
    bool isOpen;
    FsFileSystem() {}
    ~FsFileSystem() {}
    bool openFsFileSystem(FsFileSystem* fs) { fs->isOpen = false; return true; }
    void fsFsResolveCommit(FsFileSystem* fs) {}
    struct FsSaveDataInfo {
    uint64_t titleId;
    AccountUid userId;
    uint64_t saveSize;
    uint8_t saveType;
    uint8_t version;
    uint8_t reserved[6];
    uint64_t timestamp;
    bool isAvailable;
    bool isCloudAvailable;
    uint8_t deviceName[0];
    uint8_t userName[1];
};

inline uint64_t getAccountId() { return 1; }
inline bool isUserSelected() { return false; }
inline Result setInitialize() { return 0; }
inline void setExit() {}
inline Result fsdevGetDeviceFileSystem(const char* device, FsFileSystem* fs) {
    memset(fs, 0, sizeof(FsFileSystem));
    return 0;
}
inline void fsdevCommitDevice(const char* device) {}
inline Result fsdevUnmountDevice(const char* device) { return 0; }
inline Result fsOpenSaveData(FsSaveDataInfo* info, FsFileSystem* fs) {
    memset(info, 0, sizeof(FsSaveDataInfo));
    return 0;
}
inline void fsdevDeleteDeviceRecursively(const char* device) {}
inline void* malloc(size_t size) { return std::malloc(size); }
inline void free(void* ptr) { std::free(ptr); }
inline Result fsDevCreateFile(FILE* file, uint64_t offset, uint64_t size) { (void)file; (void)offset; (void)size; return 0; }
inline void fsFileClose(FILE* file) { (void)file; }
#else
#include <switch.h>
#endif
