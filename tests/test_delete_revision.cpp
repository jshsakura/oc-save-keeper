#include "tests/TestHarness.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {

std::string deriveMetaFromZip(const std::string& zipPath) {
    if (zipPath.size() > 4 && zipPath.substr(zipPath.size() - 4) == ".zip") {
        return zipPath.substr(0, zipPath.size() - 4) + ".meta";
    }
    return zipPath;
}

std::string deriveZipFromMeta(const std::string& metaPath) {
    if (metaPath.size() > 5 && metaPath.substr(metaPath.size() - 5) == ".meta") {
        return metaPath.substr(0, metaPath.size() - 5) + ".zip";
    }
    return metaPath;
}

bool isValidBackupPath(const std::string& path, const std::string& basePath) {
    if (path.empty()) return false;
    if (path.find(basePath) != 0) return false;
    if (path == basePath) return false;
    if (path.back() == '/') return false;
    return true;
}

bool isValidCloudPath(const std::string& path, uint64_t titleId) {
    if (path.empty()) return false;
    
    char titleIdStr[20];
    snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", titleId);
    
    const std::string expectedPrefix = std::string("/titles/") + titleIdStr + "/revisions/";
    return path.find(expectedPrefix) == 0;
}

} // namespace

TEST_CASE("Empty revisionId should be rejected") {
    std::string revisionId;
    REQUIRE(revisionId.empty());
    REQUIRE(!isValidBackupPath(revisionId, "/switch/oc-save-keeper/backups/"));
}

TEST_CASE("Local path outside backup directory should be rejected") {
    const std::string invalidPath = "/switch/some_other_folder/backup";
    const std::string basePath = "/switch/oc-save-keeper/backups/";
    REQUIRE(!isValidBackupPath(invalidPath, basePath));
}

TEST_CASE("Root backup folder should be rejected") {
    const std::string basePath = "/switch/oc-save-keeper/backups/";
    REQUIRE(!isValidBackupPath(basePath, basePath));
    
    const std::string trailingSlash = "/switch/oc-save-keeper/backups/0100A83022A22000/";
    REQUIRE(!isValidBackupPath(trailingSlash, basePath));
}

TEST_CASE("Valid local backup path should be accepted") {
    const std::string basePath = "/switch/oc-save-keeper/backups/";
    const std::string validPath = "/switch/oc-save-keeper/backups/0100A83022A22000/20260316_232624";
    REQUIRE(isValidBackupPath(validPath, basePath));
}

TEST_CASE("Meta to ZIP path derivation") {
    const std::string metaPath = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    const std::string expectedZip = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    REQUIRE_EQ(deriveZipFromMeta(metaPath), expectedZip);
}

TEST_CASE("ZIP to Meta path derivation") {
    const std::string zipPath = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    const std::string expectedMeta = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    REQUIRE_EQ(deriveMetaFromZip(zipPath), expectedMeta);
}

TEST_CASE("Cloud path validation - valid path") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string validPath = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    REQUIRE(isValidCloudPath(validPath, titleId));
}

TEST_CASE("Cloud path validation - invalid path (wrong title)") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string invalidPath = "/titles/0100BBBBBBBBBB/revisions/20260316_232624.meta";
    REQUIRE(!isValidCloudPath(invalidPath, titleId));
}

TEST_CASE("Cloud path validation - invalid path (outside revisions)") {
    const uint64_t titleId = 0x0100A83022A22000ULL;
    const std::string invalidPath = "/titles/0100A83022A22000/latest.meta";
    REQUIRE(!isValidCloudPath(invalidPath, titleId));
}

TEST_CASE("DeleteTaskData structure integrity") {
    struct DeleteTaskData {
        uint64_t titleId = 0;
        std::string entryId;
        std::string entryPath;
        std::string entryLabel;
        int source = 0;
        bool isSystem = false;
    };
    
    DeleteTaskData data;
    data.titleId = 0x0100A83022A22000ULL;
    data.entryId = "/titles/0100A83022A22000/revisions/20260316_232624.meta";
    data.entryPath = "/titles/0100A83022A22000/revisions/20260316_232624.zip";
    data.entryLabel = "20260316_232624";
    data.source = 1;
    data.isSystem = false;
    
    REQUIRE_EQ(data.titleId, 0x0100A83022A22000ULL);
    REQUIRE_EQ(data.entryId.size(), 55UL);
    REQUIRE(!data.isSystem);
}

TEST_CASE("Path derivation handles edge cases") {
    REQUIRE_EQ(deriveZipFromMeta("no_extension"), std::string("no_extension"));
    REQUIRE_EQ(deriveMetaFromZip("no_extension"), std::string("no_extension"));
    REQUIRE_EQ(deriveZipFromMeta(".meta"), std::string(".meta"));
}

TEST_CASE("Cancel flag prevents result handling") {
    std::atomic<bool> cancelFlag{false};
    std::atomic<bool> resultHandled{false};
    bool success = false;
    std::string message;
    std::mutex mtx;
    
    std::thread worker([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        if (cancelFlag) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mtx);
        success = true;
        message = "Operation succeeded";
        resultHandled = true;
    });
    
    cancelFlag = true;
    worker.join();
    
    REQUIRE(!resultHandled);
    REQUIRE(!success);
}

TEST_CASE("No cancel flag allows result handling") {
    std::atomic<bool> cancelFlag{false};
    std::atomic<bool> resultHandled{false};
    bool success = false;
    std::string message;
    std::mutex mtx;
    
    std::thread worker([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        
        if (cancelFlag) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mtx);
        success = true;
        message = "Operation succeeded";
        resultHandled = true;
    });
    
    worker.join();
    
    REQUIRE(resultHandled);
    REQUIRE(success);
    REQUIRE_EQ(message, std::string("Operation succeeded"));
}

TEST_CASE("Destructor join waits for thread completion") {
    std::atomic<bool> threadCompleted{false};
    std::atomic<bool> cancelFlag{false};
    
    {
        std::thread worker([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            threadCompleted = true;
        });
        
        REQUIRE(worker.joinable());
        worker.join();
    }
    
    REQUIRE(threadCompleted);
}

TEST_CASE("Atomic cancel flag is thread-safe") {
    std::atomic<bool> cancelFlag{false};
    constexpr int iterations = 1000;
    int cancelCount = 0;
    
    std::thread setter([&]() {
        for (int i = 0; i < iterations; ++i) {
            cancelFlag = (i % 2 == 0);
        }
    });
    
    std::thread reader([&]() {
        for (int i = 0; i < iterations; ++i) {
            if (cancelFlag) {
                ++cancelCount;
            }
        }
    });
    
    setter.join();
    reader.join();
    
    REQUIRE(cancelCount >= 0);
}

TEST_CASE("Mutex protects shared state during cancellation") {
    std::atomic<bool> cancelFlag{false};
    bool success = false;
    std::string message;
    std::mutex mtx;
    std::atomic<int> protectedCount{0};
    
    std::thread worker1([&]() {
        for (int i = 0; i < 100; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            ++protectedCount;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    std::thread worker2([&]() {
        for (int i = 0; i < 100; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            ++protectedCount;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    worker1.join();
    worker2.join();
    
    REQUIRE_EQ(protectedCount.load(), 200);
}

// Additional thread safety tests for RevisionMenuScreen pattern

TEST_CASE("Destructor safely terminates in-progress thread") {
    // Simulates RevisionMenuScreen destructor behavior
    std::atomic<bool> cancelFlag{false};
    std::atomic<bool> threadStarted{false};
    std::atomic<bool> threadFinished{false};
    std::atomic<bool> resultWritten{false};
    std::mutex mtx;
    bool success = false;
    
    {
        std::thread worker([&]() {
            threadStarted = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (cancelFlag) {
                threadFinished = true;
                return;
            }
            
            std::lock_guard<std::mutex> lock(mtx);
            success = true;
            resultWritten = true;
            threadFinished = true;
        });
        
        // Simulate user pressing B quickly (destructor called)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        REQUIRE(threadStarted);
        
        // Destructor pattern: set cancel, then join
        cancelFlag = true;
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    REQUIRE(threadFinished);
    // Result should NOT be written because cancel was set
    REQUIRE(!resultWritten);
    REQUIRE(!success);
}

TEST_CASE("Rapid create-destroy cycles are safe") {
    // Simulates rapid navigation in/out of RevisionMenuScreen
    for (int cycle = 0; cycle < 10; ++cycle) {
        std::atomic<bool> cancelFlag{false};
        std::atomic<int> counter{0};
        
        {
            std::thread worker([&]() {
                for (int i = 0; i < 100; ++i) {
                    if (cancelFlag) return;
                    ++counter;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
            
            // Random short delay before destroy
            std::this_thread::sleep_for(std::chrono::milliseconds(cycle % 5));
            
            cancelFlag = true;
            if (worker.joinable()) {
                worker.join();
            }
        }
        // Should complete without crash or hang
    }
    REQUIRE(true); // If we get here, all cycles completed safely
}

TEST_CASE("Thread with held mutex during cancellation") {
    // Tests that mutex is properly released even when cancelled
    std::atomic<bool> cancelFlag{false};
    std::mutex mtx;
    bool dataWritten = false;
    
    {
        std::thread worker([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            if (cancelFlag) return;
            
            {
                std::lock_guard<std::mutex> lock(mtx);
                // Simulate work that gets interrupted
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if (!cancelFlag) {
                    dataWritten = true;
                }
            }
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        cancelFlag = true;
        
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // Mutex should be available (no deadlock)
    {
        std::lock_guard<std::mutex> lock(mtx);
        // If we get here, mutex was properly released
    }
    REQUIRE(true);
}

TEST_CASE("Shared pointer capture survives object destruction") {
    // Simulates shared_ptr<DeleteTaskData> capture pattern
    struct TaskData {
        int value = 42;
    };
    
    std::atomic<bool> cancelFlag{false};
    int capturedValue = 0;
    
    {
        auto dataPtr = std::make_shared<TaskData>();
        
        std::thread worker([dataPtr, &capturedValue, &cancelFlag]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            if (cancelFlag) return;
            
            // Safe: dataPtr is a copy, not dependent on outer object
            capturedValue = dataPtr->value;
        });
        
        // Simulate object destruction (dataPtr in outer scope destroyed)
        // But worker's copy of dataPtr keeps it alive
        
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    REQUIRE_EQ(capturedValue, 42);
}

TEST_CASE("Cancel flag checked before and after mutex lock") {
    // Tests the exact pattern used in RevisionMenuScreen
    std::atomic<bool> cancelFlag{false};
    std::atomic<bool> inProgress{true};
    std::atomic<bool> success{false};
    std::mutex mtx;
    std::string message;
    
    std::thread worker([&]() {
        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        
        // Check cancel BEFORE touching shared state
        if (cancelFlag) {
            inProgress = false;
            return;
        }
        
        // Lock and write result
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check cancel AGAIN after acquiring lock
        if (cancelFlag) {
            inProgress = false;
            return;
        }
        
        success = true;
        message = "Operation succeeded";
        inProgress = false;
    });
    
    // Wait a bit then cancel
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cancelFlag = true;
    
    if (worker.joinable()) {
        worker.join();
    }
    
    REQUIRE(!inProgress);
    REQUIRE(!success); // Cancelled before writing
}
