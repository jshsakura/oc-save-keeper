#include "tests/TestHarness.hpp"

#include "core/SyncLogic.hpp"

namespace {

core::BackupMetadata makeMeta(const char* userId,
                              const char* deviceId,
                              int priority,
                              std::time_t createdAt) {
    core::BackupMetadata meta;
    meta.userId = userId ? userId : "";
    meta.deviceId = deviceId ? deviceId : "";
    meta.devicePriority = priority;
    meta.createdAt = createdAt;
    return meta;
}

} // namespace

TEST_CASE("Sync logic rejects mismatched users") {
    const core::BackupMetadata local = makeMeta("user-a", "device-a", 100, 1000);
    const core::BackupMetadata incoming = makeMeta("user-b", "device-b", 200, 2000);

    const core::SyncDecision decision =
        core::decideSyncByPolicy(core::SyncPriorityPolicy::PreferPriority, &local, incoming);

    REQUIRE_EQ(decision.useIncoming, false);
    REQUIRE_EQ(decision.reason, std::string("Different user save detected; manual restore only"));
}

TEST_CASE("Sync logic prefers higher device priority before timestamps") {
    const core::BackupMetadata local = makeMeta("user-a", "device-a", 100, 3000);
    const core::BackupMetadata incoming = makeMeta("user-a", "device-b", 200, 1000);

    const core::SyncDecision decision =
        core::decideSyncByPolicy(core::SyncPriorityPolicy::PreferPriority, &local, incoming);

    REQUIRE_EQ(decision.useIncoming, true);
    REQUIRE_EQ(decision.reason, std::string("Different device save accepted by higher priority"));
}

TEST_CASE("Sync logic falls back to timestamp when priority ties") {
    const core::BackupMetadata local = makeMeta("user-a", "device-a", 100, 1000);
    const core::BackupMetadata incoming = makeMeta("user-a", "device-a", 100, 2000);

    const core::SyncDecision decision =
        core::decideSyncByPolicy(core::SyncPriorityPolicy::PreferPriority, &local, incoming);

    REQUIRE_EQ(decision.useIncoming, true);
    REQUIRE_EQ(decision.reason, std::string("Incoming backup is newer"));
}

TEST_CASE("Sync logic keeps local when newest policy ties") {
    const core::BackupMetadata local = makeMeta("user-a", "device-a", 100, 1000);
    const core::BackupMetadata incoming = makeMeta("user-a", "device-a", 100, 1000);

    const core::SyncDecision decision =
        core::decideSyncByPolicy(core::SyncPriorityPolicy::PreferNewest, &local, incoming);

    REQUIRE_EQ(decision.useIncoming, false);
    REQUIRE_EQ(decision.reason, std::string("Timestamps match; keeping local backup"));
}
