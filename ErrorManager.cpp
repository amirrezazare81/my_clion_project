#include "ErrorManager.h"
#include <iostream>
#include <fstream>
#include <SDL.h>

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

static std::string em_log_path;
static void em_write(const std::string& level, const std::string& message) {
    try {
        if (em_log_path.empty()) {
            char* base = SDL_GetBasePath();
            if (base) { em_log_path = std::string(base) + "circuit_log.txt"; SDL_free(base); }
            else { em_log_path = "circuit_log.txt"; }
        }
        std::ofstream ofs(em_log_path, std::ios::app);
        if (ofs.is_open()) { ofs << level << ": " << message << '\n'; }
    } catch (...) {}
}

void ErrorManager::info(const std::string& message) { em_write("INFO", message); }
void ErrorManager::warn(const std::string& message) { em_write("WARN", message); }
