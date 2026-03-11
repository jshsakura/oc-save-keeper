#pragma once

#include "core/SaveManager.hpp"

namespace core {

inline SyncDecision decideSyncByPolicy(SyncPriorityPolicy policy,
                                       const BackupMetadata* localMeta,
                                       const BackupMetadata& incomingMeta) {
    if (!localMeta) {
        return {true, "No local backup metadata found"};
    }

    if (!incomingMeta.userId.empty() && !localMeta->userId.empty() && incomingMeta.userId != localMeta->userId) {
        return {false, "Different user save detected; manual restore only"};
    }

    if (!incomingMeta.deviceId.empty() && !localMeta->deviceId.empty() && incomingMeta.deviceId != localMeta->deviceId) {
        if (policy == SyncPriorityPolicy::PreferPriority) {
            if (incomingMeta.devicePriority > localMeta->devicePriority) {
                return {true, "Different device save accepted by higher priority"};
            }
            if (incomingMeta.devicePriority < localMeta->devicePriority) {
                return {false, "Different device save kept out by local priority"};
            }
        }
    }

    if (policy == SyncPriorityPolicy::PreferPriority) {
        if (incomingMeta.devicePriority > localMeta->devicePriority) {
            return {true, "Incoming backup has higher device priority"};
        }
        if (incomingMeta.devicePriority < localMeta->devicePriority) {
            return {false, "Local backup has higher device priority"};
        }
    }

    if (incomingMeta.createdAt > localMeta->createdAt) {
        return {true, "Incoming backup is newer"};
    }
    if (incomingMeta.createdAt < localMeta->createdAt) {
        return {false, "Local backup is newer"};
    }

    if (policy == SyncPriorityPolicy::PreferNewest) {
        return {false, "Timestamps match; keeping local backup"};
    }

    return {false, "Priority and timestamp are tied; keeping local backup"};
}

} // namespace core
