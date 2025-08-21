#pragma once
#include <string>
#include <iostream>

class ErrorManager {
public:
    static void displayError(const std::string& message);
    static void logError(const std::string& message); // <-- FIX: Added missing declaration
    static void handleCriticalError(const std::string& message);
};
