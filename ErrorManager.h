#pragma once
#include <string>
#include <iostream>

class ErrorManager {
public:
    static void displayError(const std::string& message);
    static void logError(const std::string& message); // <-- FIX: Added missing declaration
    static void handleCriticalError(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
};

// --- Uniform logging macros ---
#define LOG_INFO(msg) do { ErrorManager::info(msg); } while(0)
#define LOG_WARN(msg) do { ErrorManager::warn(msg); } while(0)
#define LOG_ERR(msg)  do { ErrorManager::logError(msg); } while(0)
