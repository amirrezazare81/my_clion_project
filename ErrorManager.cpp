#include "ErrorManager.h"
#include <iostream>

void ErrorManager::displayError(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
}

void ErrorManager::logError(const std::string& message) {
    std::cerr << "Log Error: " << message << std::endl;
}

void ErrorManager::handleCriticalError(const std::string& message) {
    std::cerr << "CRITICAL ERROR: " << message << ". Exiting." << std::endl;
    exit(1);
}
