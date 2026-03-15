/**
 * oc-save-keeper - Safe save backup and sync for Nintendo Switch
 * Logging utility
 */
#pragma once

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <string>

#include "utils/Paths.hpp"

namespace utils {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static void log(LogLevel level, const char* format, ...) {
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

// Convenience macros
#define LOG_DEBUG(...) utils::Logger::log(utils::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) utils::Logger::log(utils::LogLevel::INFO, __VA_ARGS__)
#define LOG_WARNING(...) utils::Logger::log(utils::LogLevel::WARNING, __VA_ARGS__)
#define LOG_ERROR(...) utils::Logger::log(utils::LogLevel::ERROR, __VA_ARGS__)
