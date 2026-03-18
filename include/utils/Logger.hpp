/**
 * oc-save-keeper - Safe save backup and sync for Nintendo Switch
 * Logging utility with file:line info and toggle support
 */
#pragma once

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstring>
#include <string>

#include "utils/Paths.hpp"
#include "utils/SettingsStore.hpp"

namespace utils {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    // Check if logging is enabled (can be toggled via settings)
    static bool isEnabled() {
        static bool cached = false;
        static bool checked = false;
        if (!checked) {
            cached = SettingsStore::getInt("logging_enabled", 1) != 0;
            checked = true;
        }
        return cached;
    }
    
    // Toggle logging on/off
    static void setEnabled(bool enabled) {
        SettingsStore::setInt("logging_enabled", enabled ? 1 : 0);
    }
    
    // Core logging function with file:line info
    static void logEx(LogLevel level, const char* file, int line, const char* format, ...) {
        if (!isEnabled() && level != LogLevel::ERROR) {
            return;  // Always log errors, but skip others if disabled
        }
        
        va_list args;
        va_start(args, format);
        va_list fileArgs;
        va_copy(fileArgs, args);
        
        const char* levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        
        // Extract just the filename from path
        const char* filename = file;
        const char* lastSlash = strrchr(file, '/');
        if (lastSlash) {
            filename = lastSlash + 1;
        }
        
        // Get timestamp
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", t);
        
        // Print to console with file:line info
        printf("[%s] [%s] [%s:%d] ", timeStr, levelStr[static_cast<int>(level)], filename, line);
        vprintf(format, args);
        printf("\n");
        
        // Also write to file
        utils::paths::ensureBaseDirectories();
        FILE* logFile = fopen(utils::paths::LOG_FILE, "a");
        if (logFile) {
            fprintf(logFile, "[%s] [%s] [%s:%d] ", timeStr, levelStr[static_cast<int>(level)], filename, line);
            vfprintf(logFile, format, fileArgs);
            fprintf(logFile, "\n");
            fclose(logFile);
        }
        
        va_end(fileArgs);
        va_end(args);
    }
    
    // Legacy log function for backward compatibility
    static void log(LogLevel level, const char* format, ...) {
        if (!isEnabled() && level != LogLevel::ERROR) {
            return;
        }
        
        va_list args;
        va_start(args, format);
        va_list fileArgs;
        va_copy(fileArgs, args);
        
        const char* levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        
        // Get timestamp
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", t);
        
        // Print to console
        printf("[%s] [%s] ", timeStr, levelStr[static_cast<int>(level)]);
        vprintf(format, args);
        printf("\n");
        
        // Also write to file
        utils::paths::ensureBaseDirectories();
        FILE* logFile = fopen(utils::paths::LOG_FILE, "a");
        if (logFile) {
            fprintf(logFile, "[%s] [%s] ", timeStr, levelStr[static_cast<int>(level)]);
            vfprintf(logFile, format, fileArgs);
            fprintf(logFile, "\n");
            fclose(logFile);
        }
        
        va_end(fileArgs);
        va_end(args);
    }
};

} // namespace utils

// Enhanced macros with file:line info
#define LOG_DEBUG_EX(...) utils::Logger::logEx(utils::LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO_EX(...) utils::Logger::logEx(utils::LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING_EX(...) utils::Logger::logEx(utils::LogLevel::WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR_EX(...) utils::Logger::logEx(utils::LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)

// Legacy macros (without file:line) for backward compatibility
#define LOG_DEBUG(...) utils::Logger::log(utils::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) utils::Logger::log(utils::LogLevel::INFO, __VA_ARGS__)
#define LOG_WARNING(...) utils::Logger::log(utils::LogLevel::WARNING, __VA_ARGS__)
#define LOG_ERROR(...) utils::Logger::log(utils::LogLevel::ERROR, __VA_ARGS__)
