#include <SDL.h>
#include "Gui.h" // <-- FIX: Switched from Menu.h to Gui.h for Phase 2
#include "ErrorManager.h"
#include <iostream>
#include <stdexcept>
#include <string>

// The SDL.h header redefines main to SDL_main for the linker
int main(int argc, char* argv[]) {
    try {
        // Create and run the GUI application instead of the CLI menu
        GuiApplication app;
        app.run();
    } catch (const std::exception& e) {
        ErrorManager::handleCriticalError("A fatal error occurred: " + std::string(e.what()));
        return 1;
    }
    return 0;
}
