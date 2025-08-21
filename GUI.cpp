

#include "Gui.h"
#include "ErrorManager.h"
#include "Analyzers.h"
#include "Solvers.h"
#include "Element.h"
#include "ProjectSerializer.h"
#include "SignalProcessor.h"
#include "SimpleAnalysis.h"
#include "Oscilloscope.h"

// Workaround for Windows byte type conflict with std::byte
#ifdef _WIN32
#define byte windows_byte
#endif
#include <SDL_image.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <limits>

// Utility function to parse values with unit prefixes
double parseValueWithPrefix(const std::string& value_str) {
    if (value_str.empty()) return 0.0;
    
    // Remove whitespace
    std::string trimmed = value_str;
    trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());
    
    if (trimmed.empty()) return 0.0;
    
    // Get the last character to check for prefix
    char last_char = std::tolower(trimmed.back());
    std::string number_part = trimmed;
    double multiplier = 1.0;
    
    // Check for unit prefixes
    switch (last_char) {
        case 'f': multiplier = 1e-15; break;  // femto
        case 'p': multiplier = 1e-12; break;  // pico
        case 'n': multiplier = 1e-9; break;   // nano
        case 'u': multiplier = 1e-6; break;   // micro
        case 'm': multiplier = 1e-3; break;   // milli
        case 'k': multiplier = 1e3; break;    // kilo
        case 'M': multiplier = 1e6; break;    // mega
        case 'G': multiplier = 1e9; break;    // giga
        case 'T': multiplier = 1e12; break;   // tera
        default:
            // No prefix, just parse the number
            return std::stod(trimmed);
    }
    
    // Remove the prefix character and parse the number
    number_part.pop_back();
    if (number_part.empty()) return 0.0;
    
    return std::stod(number_part) * multiplier;
}

// Utility function to format values with appropriate prefixes
std::string formatValueWithPrefix(double value) {
    if (value == 0.0) return "0";
    
    double abs_value = std::abs(value);
    std::string prefix = "";
    double multiplier = 1.0;
    
    if (abs_value >= 1e12) {
        prefix = "T"; multiplier = 1e-12;
    } else if (abs_value >= 1e9) {
        prefix = "G"; multiplier = 1e-9;
    } else if (abs_value >= 1e6) {
        prefix = "M"; multiplier = 1e-6;
    } else if (abs_value >= 1e3) {
        prefix = "k"; multiplier = 1e-3;
    } else if (abs_value >= 1.0) {
        // No prefix for values >= 1
    } else if (abs_value >= 1e-3) {
        prefix = "m"; multiplier = 1e3;
    } else if (abs_value >= 1e-6) {
        prefix = "u"; multiplier = 1e6;
    } else if (abs_value >= 1e-9) {
        prefix = "n"; multiplier = 1e9;
    } else if (abs_value >= 1e-12) {
        prefix = "p"; multiplier = 1e12;
    } else {
        prefix = "f"; multiplier = 1e15;
    }
    
    double scaled_value = value * multiplier;
    
    // Format with appropriate precision
    std::ostringstream oss;
    if (std::abs(scaled_value - std::round(scaled_value)) < 1e-6) {
        oss << std::fixed << std::setprecision(0) << scaled_value;
    } else {
        oss << std::fixed << std::setprecision(3) << scaled_value;
    }
    
    return oss.str() + prefix;
}
#include <stdexcept>
#include <limits>
#include <utility>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Button Implementation ---
// --- Minimal file logger ---
static std::string g_log_path;
static void logLine(const std::string& line) {
    const char* fallback = "circuit_log_fallback.txt";
    const std::string& path = g_log_path.empty() ? std::string("circuit_log.txt") : g_log_path;
    try {
        std::ofstream ofs(path, std::ios::app);
        if (ofs.is_open()) { ofs << line << '\n'; ofs.flush(); return; }
    } catch (...) {}
    try {
        std::ofstream ofs2(fallback, std::ios::app);
        if (ofs2.is_open()) { ofs2 << line << '\n'; ofs2.flush(); }
    } catch (...) {}
}

static void initLogger() {
    try {
        char* base = SDL_GetBasePath();
        if (base) {
            g_log_path = std::string(base) + "circuit_log.txt";
            SDL_free(base);
        } else {
            g_log_path = "circuit_log.txt";
        }

        // Clear the log file at startup
        try {
            std::ofstream clearFile(g_log_path, std::ios::trunc);
            if (clearFile.is_open()) {
                clearFile.close();
            }
        } catch (...) {}

        std::stringstream ss; ss << "[Logger] file: " << g_log_path;
        std::cout << ss.str() << std::endl; logLine(ss.str());
    } catch (...) {
        g_log_path.clear();
    }
}
Button::Button(int x, int y, int w, int h) : is_hovered(false), is_clicked(false) {
    rect = { x, y, w, h };
}

void Button::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        is_hovered = (mouseX >= rect.x && mouseX <= rect.x + rect.w &&
                      mouseY >= rect.y && mouseY <= rect.y + rect.h);
        if (is_hovered && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            is_clicked = true;
        } else if (is_clicked && event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            if (is_hovered) {
                doAction();
            }
            is_clicked = false;
        }
    }
}

// --- ActionButton Implementation ---
ActionButton::ActionButton(int x, int y, int w, int h, std::string button_text, TTF_Font* btn_font, std::function<void()> action)
    : Button(x, y, w, h), text(std::move(button_text)), font(btn_font), text_texture(nullptr), action_callback(std::move(action)) {}

ActionButton::~ActionButton() {
    if (text_texture) SDL_DestroyTexture(text_texture);
}

void ActionButton::createTextTexture(SDL_Renderer* renderer) {
    if (!font) return;
    if (text_texture) SDL_DestroyTexture(text_texture);
    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text.c_str(), {255, 255, 255, 255});
    if (text_surface) {
        text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        text_rect.w = text_surface->w;
        text_rect.h = text_surface->h;
        text_rect.x = rect.x + (rect.w - text_rect.w) / 2;
        text_rect.y = rect.y + (rect.h - text_rect.h) / 2;
        SDL_FreeSurface(text_surface);
    }
}

void ActionButton::doAction() {
    if (action_callback) action_callback();
}

void ActionButton::render(SDL_Renderer* renderer) {
    if (!text_texture && font) createTextTexture(renderer);

    if (is_clicked) SDL_SetRenderDrawColor(renderer, 60, 60, 180, 255);
    else if (is_hovered) SDL_SetRenderDrawColor(renderer, 100, 100, 220, 255);
    else SDL_SetRenderDrawColor(renderer, 80, 80, 200, 255);

    SDL_RenderFillRect(renderer, &rect);
    if (text_texture) SDL_RenderCopy(renderer, text_texture, nullptr, &text_rect);
}

// --- InputBox Implementation ---
InputBox::InputBox(int x, int y, int w, int h, TTF_Font* font, std::string default_text)
    : font(font), text(std::move(default_text)) {
    rect = { x, y, w, h };
}

void InputBox::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        is_active = (mx >= rect.x && mx <= rect.x + rect.w && my >= rect.y && my <= rect.y + rect.h);
    }
    if (is_active && event.type == SDL_TEXTINPUT) {
        text += event.text.text;
    }
    if (is_active && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE && !text.empty()) {
        text.pop_back();
    }
}

void InputBox::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, is_active ? 0 : 100, is_active ? 0 : 100, is_active ? 200 : 150, 255);
    SDL_RenderDrawRect(renderer, &rect);

    if (!text.empty() && font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), {0, 0, 0, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect text_rect = { rect.x + 5, rect.y + 5, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &text_rect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
}

std::string InputBox::getText() const { return text; }

void InputBox::setText(const std::string& new_text) { text = new_text; }

// --- SimulationSettingsPanel Implementation ---
SimulationSettingsPanel::SimulationSettingsPanel(int x, int y, int w, int h, TTF_Font* font) {
    panel_rect = { x, y, w, h };
    current_tab = TabType::AC;  // Default to AC tab
    
    // Transient Analysis inputs (adding start time)
    tran_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 55, 100, 25, font, "0"));       // Start Time
    tran_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 85, 100, 25, font, "1e-6"));    // Time Step
    tran_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 115, 100, 25, font, "0.01"));   // Stop Time
    
    // AC Analysis inputs  
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 155, 100, 25, font, "AC1"));      // Source
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 195, 100, 25, font, "1"));        // Start Freq
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 235, 100, 25, font, "100k"));     // Stop Freq
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 275, 100, 25, font, "100"));      // Points

    // Phase Analysis inputs
    phase_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 155, 100, 25, font, "AC1"));   // Source
    phase_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 195, 100, 25, font, "0"));     // Start Phase
    phase_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 235, 100, 25, font, "360"));   // End Phase
    phase_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 275, 100, 25, font, "1000"));  // Base Freq
    phase_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 315, 100, 25, font, "100"));   // Points
}

void SimulationSettingsPanel::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;

    // Handle tab switching with mouse clicks
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int mouse_x = event.button.x;
        int mouse_y = event.button.y;

        int tab_y = panel_rect.y + 320;
        SDL_Rect tran_tab = {panel_rect.x + 10, tab_y, 80, 30};
        SDL_Rect ac_tab = {panel_rect.x + 100, tab_y, 80, 30};
        SDL_Rect phase_tab = {panel_rect.x + 190, tab_y, 80, 30};

        if (mouse_x >= tran_tab.x && mouse_x <= tran_tab.x + tran_tab.w &&
            mouse_y >= tran_tab.y && mouse_y <= tran_tab.y + tran_tab.h) {
            current_tab = TabType::TRANSIENT;
            return;
        } else if (mouse_x >= ac_tab.x && mouse_x <= ac_tab.x + ac_tab.w &&
                   mouse_y >= ac_tab.y && mouse_y <= ac_tab.y + ac_tab.h) {
            current_tab = TabType::AC;
            return;
        } else if (mouse_x >= phase_tab.x && mouse_x <= phase_tab.x + phase_tab.w &&
                   mouse_y >= phase_tab.y && mouse_y <= phase_tab.y + phase_tab.h) {
            current_tab = TabType::PHASE;
            return;
        }
    }

    // Handle ENTER key to run analysis
    if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) {
        // Signal that we want to run analysis - this will be handled by the main application
        // We'll use a callback mechanism to notify the main application
        if (on_enter_callback) {
            on_enter_callback(current_tab);
        }
        return;
    }

    // Handle input box events based on current tab
    if (current_tab == TabType::TRANSIENT) {
    for (auto& box : tran_inputs) box->handleEvent(event);
    } else if (current_tab == TabType::AC) {
    for (auto& box : ac_inputs) box->handleEvent(event);
    } else if (current_tab == TabType::PHASE) {
        for (auto& box : phase_inputs) box->handleEvent(event);
    }
}

void SimulationSettingsPanel::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    
    // Draw panel background
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 220);
    SDL_RenderFillRect(renderer, &panel_rect);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &panel_rect);
    
    // Get font from input boxes
    TTF_Font* font = nullptr;
    if (!tran_inputs.empty() && tran_inputs[0]) {
        font = tran_inputs[0]->getFont();
    }

    // Draw title
    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Simulation Settings", {255, 255, 255, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect title_rect = {panel_rect.x + 10, panel_rect.y + 5, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &title_rect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // Draw tab buttons
    int tab_y = panel_rect.y + 320;
    SDL_Rect tran_tab = {panel_rect.x + 10, tab_y, 80, 30};
    SDL_Rect ac_tab = {panel_rect.x + 100, tab_y, 80, 30};
    SDL_Rect phase_tab = {panel_rect.x + 190, tab_y, 80, 30};

    // Draw tabs with different colors based on selection
    SDL_SetRenderDrawColor(renderer, current_tab == TabType::TRANSIENT ? 100 : 70, 100, 100, 255);
    SDL_RenderFillRect(renderer, &tran_tab);
    SDL_SetRenderDrawColor(renderer, current_tab == TabType::AC ? 100 : 70, 100, 100, 255);
    SDL_RenderFillRect(renderer, &ac_tab);
    SDL_SetRenderDrawColor(renderer, current_tab == TabType::PHASE ? 100 : 70, 100, 100, 255);
    SDL_RenderFillRect(renderer, &phase_tab);

    // Draw tab borders
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_RenderDrawRect(renderer, &tran_tab);
    SDL_RenderDrawRect(renderer, &ac_tab);
    SDL_RenderDrawRect(renderer, &phase_tab);

    // Draw tab labels
    if (font) {
        SDL_Surface* tran_surface = TTF_RenderText_Blended(font, "TRAN", {255, 255, 255, 255});
        if (tran_surface) {
            SDL_Texture* tran_texture = SDL_CreateTextureFromSurface(renderer, tran_surface);
            SDL_Rect tran_label_rect = {tran_tab.x + 5, tran_tab.y + 5, tran_surface->w, tran_surface->h};
            SDL_RenderCopy(renderer, tran_texture, nullptr, &tran_label_rect);
            SDL_DestroyTexture(tran_texture);
            SDL_FreeSurface(tran_surface);
        }

        SDL_Surface* ac_surface = TTF_RenderText_Blended(font, "AC", {255, 255, 255, 255});
        if (ac_surface) {
            SDL_Texture* ac_texture = SDL_CreateTextureFromSurface(renderer, ac_surface);
            SDL_Rect ac_label_rect = {ac_tab.x + 20, ac_tab.y + 5, ac_surface->w, ac_surface->h};
            SDL_RenderCopy(renderer, ac_texture, nullptr, &ac_label_rect);
            SDL_DestroyTexture(ac_texture);
            SDL_FreeSurface(ac_surface);
        }

        SDL_Surface* phase_surface = TTF_RenderText_Blended(font, "PHASE", {255, 255, 255, 255});
        if (phase_surface) {
            SDL_Texture* phase_texture = SDL_CreateTextureFromSurface(renderer, phase_surface);
            SDL_Rect phase_label_rect = {phase_tab.x + 5, phase_tab.y + 5, phase_surface->w, phase_surface->h};
            SDL_RenderCopy(renderer, phase_texture, nullptr, &phase_label_rect);
            SDL_DestroyTexture(phase_texture);
            SDL_FreeSurface(phase_surface);
        }
    }

    // Render content based on current tab
    if (current_tab == TabType::TRANSIENT) {
        // Draw TRAN labels and inputs
        if (font) {
            SDL_Surface* tstart_surface = TTF_RenderText_Blended(font, "Start Time (s):", {255, 255, 255, 255});
            if (tstart_surface) {
                SDL_Texture* tstart_texture = SDL_CreateTextureFromSurface(renderer, tstart_surface);
                SDL_Rect tstart_label_rect = {panel_rect.x + 10, panel_rect.y + 35, tstart_surface->w, tstart_surface->h};
                SDL_RenderCopy(renderer, tstart_texture, nullptr, &tstart_label_rect);
                SDL_DestroyTexture(tstart_texture);
                SDL_FreeSurface(tstart_surface);
            }

            SDL_Surface* tstep_surface = TTF_RenderText_Blended(font, "Time Step (s):", {255, 255, 255, 255});
            if (tstep_surface) {
                SDL_Texture* tstep_texture = SDL_CreateTextureFromSurface(renderer, tstep_surface);
                SDL_Rect tstep_label_rect = {panel_rect.x + 10, panel_rect.y + 65, tstep_surface->w, tstep_surface->h};
                SDL_RenderCopy(renderer, tstep_texture, nullptr, &tstep_label_rect);
                SDL_DestroyTexture(tstep_texture);
                SDL_FreeSurface(tstep_surface);
            }

            SDL_Surface* tstop_surface = TTF_RenderText_Blended(font, "Stop Time (s):", {255, 255, 255, 255});
            if (tstop_surface) {
                SDL_Texture* tstop_texture = SDL_CreateTextureFromSurface(renderer, tstop_surface);
                SDL_Rect tstop_label_rect = {panel_rect.x + 10, panel_rect.y + 95, tstop_surface->w, tstop_surface->h};
                SDL_RenderCopy(renderer, tstop_texture, nullptr, &tstop_label_rect);
                SDL_DestroyTexture(tstop_texture);
                SDL_FreeSurface(tstop_surface);
            }

            // Add hint text for Enter key
            SDL_Surface* hint_surface = TTF_RenderText_Blended(font, "Press ENTER to run TRAN analysis", {200, 200, 200, 255});
            if (hint_surface) {
                SDL_Texture* hint_texture = SDL_CreateTextureFromSurface(renderer, hint_surface);
                SDL_Rect hint_label_rect = {panel_rect.x + 10, panel_rect.y + 130, hint_surface->w, hint_surface->h};
                SDL_RenderCopy(renderer, hint_texture, nullptr, &hint_label_rect);
                SDL_DestroyTexture(hint_texture);
                SDL_FreeSurface(hint_surface);
            }
        }

        for (auto& input : tran_inputs) {
            input->render(renderer);
        }
    } else if (current_tab == TabType::AC) {
        // Draw AC labels and inputs
        if (font) {
            SDL_Surface* source_surface = TTF_RenderText_Blended(font, "AC Source:", {255, 255, 255, 255});
            if (source_surface) {
                SDL_Texture* source_texture = SDL_CreateTextureFromSurface(renderer, source_surface);
                SDL_Rect source_label_rect = {panel_rect.x + 10, panel_rect.y + 35, source_surface->w, source_surface->h};
                SDL_RenderCopy(renderer, source_texture, nullptr, &source_label_rect);
                SDL_DestroyTexture(source_texture);
                SDL_FreeSurface(source_surface);
            }

            SDL_Surface* start_surface = TTF_RenderText_Blended(font, "Start Freq (Hz):", {255, 255, 255, 255});
            if (start_surface) {
                SDL_Texture* start_texture = SDL_CreateTextureFromSurface(renderer, start_surface);
                SDL_Rect start_label_rect = {panel_rect.x + 10, panel_rect.y + 75, start_surface->w, start_surface->h};
                SDL_RenderCopy(renderer, start_texture, nullptr, &start_label_rect);
                SDL_DestroyTexture(start_texture);
                SDL_FreeSurface(start_surface);
            }

            SDL_Surface* stop_surface = TTF_RenderText_Blended(font, "Stop Freq (Hz):", {255, 255, 255, 255});
            if (stop_surface) {
                SDL_Texture* stop_texture = SDL_CreateTextureFromSurface(renderer, stop_surface);
                SDL_Rect stop_label_rect = {panel_rect.x + 10, panel_rect.y + 115, stop_surface->w, stop_surface->h};
                SDL_RenderCopy(renderer, stop_texture, nullptr, &stop_label_rect);
                SDL_DestroyTexture(stop_texture);
                SDL_FreeSurface(stop_surface);
            }

            SDL_Surface* points_surface = TTF_RenderText_Blended(font, "Points:", {255, 255, 255, 255});
            if (points_surface) {
                SDL_Texture* points_texture = SDL_CreateTextureFromSurface(renderer, points_surface);
                SDL_Rect points_label_rect = {panel_rect.x + 10, panel_rect.y + 155, points_surface->w, points_surface->h};
                SDL_RenderCopy(renderer, points_texture, nullptr, &points_label_rect);
                SDL_DestroyTexture(points_texture);
                SDL_FreeSurface(points_surface);
            }

            // Add hint text for Enter key
            SDL_Surface* hint_surface = TTF_RenderText_Blended(font, "Press ENTER to run AC analysis", {200, 200, 200, 255});
            if (hint_surface) {
                SDL_Texture* hint_texture = SDL_CreateTextureFromSurface(renderer, hint_surface);
                SDL_Rect hint_label_rect = {panel_rect.x + 10, panel_rect.y + 190, hint_surface->w, hint_surface->h};
                SDL_RenderCopy(renderer, hint_texture, nullptr, &hint_label_rect);
                SDL_DestroyTexture(hint_texture);
                SDL_FreeSurface(hint_surface);
            }
        }

        for (auto& input : ac_inputs) {
            input->render(renderer);
        }
    } else if (current_tab == TabType::PHASE) {
        // Draw PHASE labels and inputs
        if (font) {
            SDL_Surface* source_surface = TTF_RenderText_Blended(font, "AC Source:", {255, 255, 255, 255});
            if (source_surface) {
                SDL_Texture* source_texture = SDL_CreateTextureFromSurface(renderer, source_surface);
                SDL_Rect source_label_rect = {panel_rect.x + 10, panel_rect.y + 35, source_surface->w, source_surface->h};
                SDL_RenderCopy(renderer, source_texture, nullptr, &source_label_rect);
                SDL_DestroyTexture(source_texture);
                SDL_FreeSurface(source_surface);
            }

            SDL_Surface* start_phase_surface = TTF_RenderText_Blended(font, "Start Phase (°):", {255, 255, 255, 255});
            if (start_phase_surface) {
                SDL_Texture* start_phase_texture = SDL_CreateTextureFromSurface(renderer, start_phase_surface);
                SDL_Rect start_phase_label_rect = {panel_rect.x + 10, panel_rect.y + 75, start_phase_surface->w, start_phase_surface->h};
                SDL_RenderCopy(renderer, start_phase_texture, nullptr, &start_phase_label_rect);
                SDL_DestroyTexture(start_phase_texture);
                SDL_FreeSurface(start_phase_surface);
            }

            SDL_Surface* end_phase_surface = TTF_RenderText_Blended(font, "End Phase (°):", {255, 255, 255, 255});
            if (end_phase_surface) {
                SDL_Texture* end_phase_texture = SDL_CreateTextureFromSurface(renderer, end_phase_surface);
                SDL_Rect end_phase_label_rect = {panel_rect.x + 10, panel_rect.y + 115, end_phase_surface->w, end_phase_surface->h};
                SDL_RenderCopy(renderer, end_phase_texture, nullptr, &end_phase_label_rect);
                SDL_DestroyTexture(end_phase_texture);
                SDL_FreeSurface(end_phase_surface);
            }

            SDL_Surface* freq_surface = TTF_RenderText_Blended(font, "Base Freq (Hz):", {255, 255, 255, 255});
            if (freq_surface) {
                SDL_Texture* freq_texture = SDL_CreateTextureFromSurface(renderer, freq_surface);
                SDL_Rect freq_label_rect = {panel_rect.x + 10, panel_rect.y + 155, freq_surface->w, freq_surface->h};
                SDL_RenderCopy(renderer, freq_texture, nullptr, &freq_label_rect);
                SDL_DestroyTexture(freq_texture);
                SDL_FreeSurface(freq_surface);
            }

            SDL_Surface* points_surface = TTF_RenderText_Blended(font, "Points:", {255, 255, 255, 255});
            if (points_surface) {
                SDL_Texture* points_texture = SDL_CreateTextureFromSurface(renderer, points_surface);
                SDL_Rect points_label_rect = {panel_rect.x + 10, panel_rect.y + 195, points_surface->w, points_surface->h};
                SDL_RenderCopy(renderer, points_texture, nullptr, &points_label_rect);
                SDL_DestroyTexture(points_texture);
                SDL_FreeSurface(points_surface);
            }

            // Add hint text for Enter key
            SDL_Surface* hint_surface = TTF_RenderText_Blended(font, "Press ENTER to run PHASE analysis", {200, 200, 200, 255});
            if (hint_surface) {
                SDL_Texture* hint_texture = SDL_CreateTextureFromSurface(renderer, hint_surface);
                SDL_Rect hint_label_rect = {panel_rect.x + 10, panel_rect.y + 230, hint_surface->w, hint_surface->h};
                SDL_RenderCopy(renderer, hint_texture, nullptr, &hint_label_rect);
                SDL_DestroyTexture(hint_texture);
                SDL_FreeSurface(hint_surface);
            }
        }

        for (auto& input : phase_inputs) {
            input->render(renderer);
        }
    }
}

void SimulationSettingsPanel::toggleVisibility() { is_visible = !is_visible; }
double SimulationSettingsPanel::getTranTStart() const { try { return std::stod(tran_inputs[0]->getText()); } catch(...) { return 0.0; } }
double SimulationSettingsPanel::getTranTStep() const { try { return std::stod(tran_inputs[1]->getText()); } catch(...) { return 1e-6; } }
double SimulationSettingsPanel::getTranTStop() const { try { return std::stod(tran_inputs[2]->getText()); } catch(...) { return 0.01; } }
std::string SimulationSettingsPanel::getACSource() const { return ac_inputs[0]->getText(); }
double SimulationSettingsPanel::getACStartFreq() const { try { return std::stod(ac_inputs[1]->getText()); } catch(...) { return 1.0; } }
double SimulationSettingsPanel::getACStopFreq() const { try { return std::stod(ac_inputs[2]->getText()); } catch(...) { return 100e3; } }
int SimulationSettingsPanel::getACPoints() const { try { return std::stoi(ac_inputs[3]->getText()); } catch(...) { return 100; } }
SimulationSettingsPanel::TabType SimulationSettingsPanel::getCurrentTab() const { return current_tab; }

// --- TransientAnalysisDialog Implementation ---
TransientAnalysisDialog::TransientAnalysisDialog(int x, int y, int w, int h, TTF_Font* font,
                                               std::function<void(double, double, double)> run_callback,
                                               std::function<void()> cancel_callback)
    : dialog_rect({x, y, w, h}), font(font), on_run(run_callback), on_cancel(cancel_callback) {
    
    // Create input boxes for transient parameters with unit examples
    tstart_input = std::make_unique<InputBox>(x + 120, y + 60, 120, 30, font, "0");
    tstep_input = std::make_unique<InputBox>(x + 120, y + 100, 120, 30, font, "1e-6");
    tstop_input = std::make_unique<InputBox>(x + 120, y + 140, 120, 30, font, "0.01");
}

void TransientAnalysisDialog::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;
    
    tstart_input->handleEvent(event);
    tstep_input->handleEvent(event);
    tstop_input->handleEvent(event);
    
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
            // Run simulation with current values - parse units
            try {
                // Try parsing as scientific notation first (faster and more reliable)
                double tstart = 0.0, tstep = 1e-6, tstop = 0.01;
                try { tstart = std::stod(tstart_input->getText()); } catch(...) {}
                try { tstep = std::stod(tstep_input->getText()); } catch(...) {}
                try { tstop = std::stod(tstop_input->getText()); } catch(...) {}

                // Fallback to unit parsing if scientific notation fails
                if (tstep == 1e-6 && tstep_input->getText().find("us") != std::string::npos) {
                    tstep = parseTimeWithUnits(tstep_input->getText());
                }
                if (tstop == 0.01 && tstop_input->getText().find("ms") != std::string::npos) {
                    tstop = parseTimeWithUnits(tstop_input->getText());
                }

                if (tstep <= 0 || tstop <= tstart || tstep >= tstop) {
                    ErrorManager::displayError("Invalid time parameters: tstep > 0, tstop > tstart, and tstep < tstop required");
                    return;
                }

                // Additional validation
                int expected_steps = static_cast<int>((tstop - tstart) / tstep);
                if (expected_steps < 2) {
                    std::string msg = "Warning: Very few time steps (" + std::to_string(expected_steps) +
                                    "). Consider reducing time step or increasing total time.";
                    ErrorManager::displayError(msg);
                    return;
                }
                if (expected_steps > 10000) {
                    std::string msg = "Warning: Very many time steps (" + std::to_string(expected_steps) +
                                    "). This may take a long time. Consider increasing time step.";
                    ErrorManager::displayError(msg);
                    // Don't return, just warn
                }
                
                std::cout << "[TRAN UNITS] Parsed times: tstart=" << tstart << "s, tstep=" << tstep << "s, tstop=" << tstop << "s" << std::endl;
                std::cout << "[TRAN UNITS] Raw inputs: tstart='" << tstart_input->getText()
                          << "', tstep='" << tstep_input->getText()
                          << "', tstop='" << tstop_input->getText() << "'" << std::endl;

                hide();
                on_run(tstart, tstep, tstop);
            } catch (const std::exception& e) {
                ErrorManager::displayError("Time parsing error: " + std::string(e.what()));
            }
        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
            hide();
            on_cancel();
        }
    }
}

void TransientAnalysisDialog::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    
    // Draw dialog background
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &dialog_rect);
    
    // Draw border
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &dialog_rect);
    
    // Draw title and labels
    if (font) {
        // Title
        SDL_Surface* title_surface = TTF_RenderText_Blended(font, "Transient Analysis Parameters", {0, 0, 0, 255});
        if (title_surface) {
            SDL_Texture* title_texture = SDL_CreateTextureFromSurface(renderer, title_surface);
            SDL_Rect title_rect = {dialog_rect.x + 10, dialog_rect.y + 10, title_surface->w, title_surface->h};
            SDL_RenderCopy(renderer, title_texture, NULL, &title_rect);
            SDL_DestroyTexture(title_texture);
            SDL_FreeSurface(title_surface);
        }
        
        // Labels
        const char* labels[] = {"Start Time (s):", "Time Step (s):", "Stop Time (s):"};
        int y_positions[] = {dialog_rect.y + 60, dialog_rect.y + 100, dialog_rect.y + 140};
        
        for (int i = 0; i < 3; i++) {
            SDL_Surface* label_surface = TTF_RenderText_Blended(font, labels[i], {40, 40, 40, 255});
            if (label_surface) {
                SDL_Texture* label_texture = SDL_CreateTextureFromSurface(renderer, label_surface);
                SDL_Rect label_rect = {dialog_rect.x + 10, y_positions[i], label_surface->w, label_surface->h};
                SDL_RenderCopy(renderer, label_texture, NULL, &label_rect);
                SDL_DestroyTexture(label_texture);
                SDL_FreeSurface(label_surface);
            }
        }
        
        // Units help
        SDL_Surface* units_surface = TTF_RenderText_Blended(font, "Scientific notation: 1e-6, 0.01, 1.0", {60, 60, 60, 255});
        if (units_surface) {
            SDL_Texture* units_texture = SDL_CreateTextureFromSurface(renderer, units_surface);
            SDL_Rect units_rect = {dialog_rect.x + 10, dialog_rect.y + 175, units_surface->w, units_surface->h};
            SDL_RenderCopy(renderer, units_texture, NULL, &units_rect);
            SDL_DestroyTexture(units_texture);
            SDL_FreeSurface(units_surface);
        }
        
        // Instructions
        SDL_Surface* instr_surface = TTF_RenderText_Blended(font, "Press ENTER to run, ESC to cancel", {80, 80, 80, 255});
        if (instr_surface) {
            SDL_Texture* instr_texture = SDL_CreateTextureFromSurface(renderer, instr_surface);
            SDL_Rect instr_rect = {dialog_rect.x + 10, dialog_rect.y + 195, instr_surface->w, instr_surface->h};
            SDL_RenderCopy(renderer, instr_texture, NULL, &instr_rect);
            SDL_DestroyTexture(instr_texture);
            SDL_FreeSurface(instr_surface);
        }
    }
    
    // Render input boxes
    tstart_input->render(renderer);
    tstep_input->render(renderer);
    tstop_input->render(renderer);
}

double TransientAnalysisDialog::parseTimeWithUnits(const std::string& time_str) const {
    if (time_str.empty()) return 0.0;
    
    std::string str = time_str;
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    
    // Extract the numeric part and unit part
    double value = 0.0;
    std::string unit = "";
    
    size_t unit_pos = str.find_first_not_of("0123456789.-+e");
    if (unit_pos != std::string::npos) {
        try {
            value = std::stod(str.substr(0, unit_pos));
            unit = str.substr(unit_pos);
        } catch (...) {
            throw std::runtime_error("Invalid number format: " + time_str);
        }
    } else {
        try {
            value = std::stod(str);
            unit = "s"; // Default to seconds if no unit specified
        } catch (...) {
            throw std::runtime_error("Invalid number format: " + time_str);
        }
    }
    
    // Convert to seconds based on unit
    double multiplier = 1.0; // default seconds
    if (unit == "ps" || unit == "psec") {
        multiplier = 1e-12;
    } else if (unit == "ns" || unit == "nsec") {
        multiplier = 1e-9;
    } else if (unit == "us" || unit == "usec" || unit == "μs" || unit == "microsec") {
        multiplier = 1e-6;
    } else if (unit == "ms" || unit == "msec") {
        multiplier = 1e-3;
    } else if (unit == "s" || unit == "sec" || unit == "") {
        multiplier = 1.0;
    } else {
        throw std::runtime_error("Unknown time unit: " + unit + ". Supported: ps, ns, us, ms, s");
    }
    
    return value * multiplier;
}

void TransientAnalysisDialog::show() { is_visible = true; }
void TransientAnalysisDialog::hide() { is_visible = false; }

// --- ComponentSelector Implementation ---
ComponentSelector::ComponentSelector(int x, int y, int w, int h, TTF_Font* font, std::function<void(const std::string&)> on_select) {
    panel_rect = {x, y, w, h};
    
    // Main component buttons - smaller and better spaced
    int current_y = y + 10;
    int button_height = 30;  // Reduced from 40
    int button_spacing = 35; // Reduced from 50
    
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Wire", font, [on_select](){ on_select("Wire"); }));
    current_y += button_spacing;
    
    // Add dedicated wire creation button for pin-based wiring
    wire_button = std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Pin Wire", font, [this](){ 
        // This will enable pin-based wire creation mode
        std::cout << "Pin-based wire creation mode enabled. Click on pins to create wires." << std::endl;
    });
    current_y += button_spacing;
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Resistor", font, [on_select](){ on_select("Resistor"); }));
    current_y += button_spacing;
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Capacitor", font, [on_select](){ on_select("Capacitor"); }));
    current_y += button_spacing;
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Inductor", font, [on_select](){ on_select("Inductor"); }));
    current_y += button_spacing;
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Diode", font, [on_select](){ on_select("Diode"); }));
    current_y += button_spacing;
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Ground", font, [on_select](){ on_select("Ground"); }));
    current_y += button_spacing;
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Sources", font, [this](){ this->toggleSourceMenu(); }));
    current_y += button_spacing;
    component_buttons.push_back(std::make_unique<ActionButton>(x + 5, current_y, w - 10, button_height, "Dep. Sources", font, [this](){ this->toggleDependentSourceMenu(); }));

    // Source menu - positioned to the right with proper boundaries
    int source_y = y + 10;
    int source_width = w - 10;
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "DC Source", font, [on_select](){ on_select("IndependentVoltageSource"); }));
    source_y += button_spacing;
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "Pulse Source", font, [on_select](){ on_select("PulseVoltageSource"); }));
    source_y += button_spacing;
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "Sine Source", font, [on_select](){ on_select("SinusoidalVoltageSource"); }));
    source_y += button_spacing;
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "Waveform Src", font, [on_select](){ on_select("WaveformVoltageSource"); }));
    source_y += button_spacing;
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "Phase Src", font, [on_select](){ on_select("PhaseVoltageSource"); }));
    source_y += button_spacing;
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "Current Src", font, [on_select](){ on_select("IndependentCurrentSource"); }));
    source_y += button_spacing;
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "Pulse Current", font, [on_select](){ on_select("PulseCurrentSource"); }));
    source_y += button_spacing;


    // Dependent source menu - positioned below sources
    int dep_source_y = y + 10;
    int dep_source_width = w - 10;
    dependent_source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, dep_source_y, dep_source_width, button_height, "VCVS", font, [on_select](){ on_select("VoltageControlledVoltageSource"); }));
    dep_source_y += button_spacing;
    dependent_source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, dep_source_y, dep_source_width, button_height, "VCCS", font, [on_select](){ on_select("VoltageControlledCurrentSource"); }));
    dep_source_y += button_spacing;
    dependent_source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, dep_source_y, dep_source_width, button_height, "CCVS", font, [on_select](){ on_select("CurrentControlledVoltageSource"); }));
    dep_source_y += button_spacing;
    dependent_source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, dep_source_y, dep_source_width, button_height, "CCCS", font, [on_select](){ on_select("CurrentControlledCurrentSource"); }));
}

void ComponentSelector::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;

    if (show_sources) {
        for (auto& btn : source_buttons) btn->handleEvent(event);
    } else if (show_dependent_sources) {
        for (auto& btn : dependent_source_buttons) btn->handleEvent(event);
    } else {
        for (auto& btn : component_buttons) btn->handleEvent(event);
        wire_button->handleEvent(event); // Handle wire button events
    }
}

void ComponentSelector::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    
    // Draw main panel background
    SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
    SDL_RenderFillRect(renderer, &panel_rect);
    
    // Draw panel border
    SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
    SDL_RenderDrawRect(renderer, &panel_rect);

    if (show_sources) {
        // Draw source submenu background
        SDL_Rect source_rect = {panel_rect.x + panel_rect.w + 5, panel_rect.y, panel_rect.w - 10, panel_rect.h};
        SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
        SDL_RenderFillRect(renderer, &source_rect);
        SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
        SDL_RenderDrawRect(renderer, &source_rect);
        
        for (auto& btn : source_buttons) btn->render(renderer);
    } else if (show_dependent_sources) {
        // Draw dependent source submenu background
        SDL_Rect dep_rect = {panel_rect.x + panel_rect.w + 5, panel_rect.y, panel_rect.w - 10, panel_rect.h};
        SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
        SDL_RenderFillRect(renderer, &dep_rect);
        SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
        SDL_RenderDrawRect(renderer, &dep_rect);
        
        for (auto& btn : dependent_source_buttons) btn->render(renderer);
    } else {
        for (auto& btn : component_buttons) btn->render(renderer);
        wire_button->render(renderer); // Render wire button
    }
}

void ComponentSelector::toggleVisibility() {
    is_visible = !is_visible;
    show_sources = false;
    show_dependent_sources = false;
}

void ComponentSelector::toggleSourceMenu() {
    show_sources = !show_sources;
    show_dependent_sources = false;
}

void ComponentSelector::toggleDependentSourceMenu() {
    show_dependent_sources = !show_dependent_sources;
    show_sources = false;
}

// --- SchematicView Implementation ---
SchematicView::SchematicView(int x, int y, int w, int h, Circuit& circuit, std::map<std::string, SDL_Texture*>& textures, TTF_Font* font)
    : circuit_backend(circuit), component_textures(textures), font(font) {
    view_area = { x, y, w, h };
    updatePinPositions();
}

void SchematicView::handleEvent(const SDL_Event& event) {}

// Helper method to calculate optimal scaling for a component (DEPRECATED - now using fixed 100px size)
double SchematicView::calculateOptimalScale(int original_w, int original_h) {
    // This method is no longer used - all components are now fixed at 100x100 pixels
    return 1.0;
}

SDL_Point SchematicView::snapToGrid(int x, int y) {
    SDL_Point point = {x, y};
    if (!SDL_PointInRect(&point, &view_area)) {
        return {x, y}; // Don't snap if outside view area
    }
    
    int relative_x = x - view_area.x;
    int relative_y = y - view_area.y;
    
    int grid_x = (relative_x + GRID_SIZE / 2) / GRID_SIZE;
    int grid_y = (relative_y + GRID_SIZE / 2) / GRID_SIZE;
    
    return {
        view_area.x + grid_x * GRID_SIZE,
        view_area.y + grid_y * GRID_SIZE
    };
}

std::string SchematicView::getNodeAtGridPos(int grid_x, int grid_y) {
    return "N" + std::to_string(grid_y * 100 + grid_x);
}

std::string SchematicView::getNodeAt(int mouse_x, int mouse_y) {
    SDL_Point mousePoint = {mouse_x, mouse_y};
    if (SDL_PointInRect(&mousePoint, &view_area)) {
        int grid_x = (mouse_x - view_area.x + GRID_SIZE / 2) / GRID_SIZE;
        int grid_y = (mouse_y - view_area.y + GRID_SIZE / 2) / GRID_SIZE;
        return getNodeAtGridPos(grid_x, grid_y);
    }
    return "";
}

SDL_Point SchematicView::getNodePosition(const std::string& node_id) {
    if (node_id.empty()) return { view_area.x, view_area.y };
    if (node_id[0] == 'N') {
        try {
            unsigned long num = std::stoul(node_id.substr(1));
            return { view_area.x + (int)(num % 100) * GRID_SIZE, view_area.y + (int)(num / 100) * GRID_SIZE };
        } catch (...) {}
    }
    return { static_cast<int>(view_area.x + (std::hash<std::string>{}(node_id) % 30) * GRID_SIZE),
             static_cast<int>(view_area.y + (std::hash<std::string>{}(node_id) % 20) * GRID_SIZE) };
}

void SchematicView::drawElementSymbol(SDL_Renderer* renderer, const Element& elem) {
    SDL_Point p1 = getNodePosition(elem.getNode1Id());
    SDL_Point p2 = getNodePosition(elem.getNode2Id());
    std::string type = elem.getType();
    SDL_Texture* texture = nullptr;

    // Do not render backend wires here; GUI wires are rendered in drawWires()
    if (type == "Wire" || type == "CircuitWire" || type == "GuiWire") {
        return;
    }

    // Map element types to texture files
    if (type == "Resistor") texture = component_textures["resistor.png"];
    else if (type == "Capacitor") texture = component_textures["capacitor.png"];
    else if (type == "Inductor") texture = component_textures["inductor.png"];
    else if (type == "Diode") texture = component_textures["diode.png"];
    else if (type == "Ground") texture = component_textures["gnd.png"];
    else if (type == "IndependentVoltageSource") texture = component_textures["dc_v_source.png"];
    else if (type == "PulseVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (type == "SinusoidalVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (type == "ACVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (type == "WaveformVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (type == "PhaseVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (type == "IndependentCurrentSource") texture = component_textures["dc_c_source.png"];
    else if (type == "PulseCurrentSource") texture = component_textures["ac_c_source.png"];

    else if (type == "VoltageControlledVoltageSource") texture = component_textures["dep_v_source.png"];
    else if (type == "VoltageControlledCurrentSource") texture = component_textures["dep_c_source.png"];
    else if (type == "CurrentControlledVoltageSource") texture = component_textures["dep_v_source.png"];
    else if (type == "CurrentControlledCurrentSource") texture = component_textures["dep_c_source.png"];

    if (texture) {
        // Read actual image dimensions dynamically
        int original_w, original_h;
        SDL_QueryTexture(texture, NULL, NULL, &original_w, &original_h);
        
        // Dynamic sizing based on original aspect ratio but with consistent maximum size
        const int MAX_SIZE = 80;
        const int MIN_SIZE = 40;
        
        // Calculate size while maintaining aspect ratio
        double aspect_ratio = static_cast<double>(original_w) / original_h;
        int scaled_w, scaled_h;
        
        if (aspect_ratio > 1.0) {
            // Wider than tall
            scaled_w = MAX_SIZE;
            scaled_h = static_cast<int>(MAX_SIZE / aspect_ratio);
        } else {
            // Taller than wide or square
            scaled_h = MAX_SIZE;
            scaled_w = static_cast<int>(MAX_SIZE * aspect_ratio);
        }
        
        // Ensure minimum size
        scaled_w = std::max(scaled_w, MIN_SIZE);
        scaled_h = std::max(scaled_h, MIN_SIZE);
        
        // Calculate center position between the two nodes.
        // For Ground, anchor at node1 position to sit exactly on its grid node.
        int center_x = (elem.getType() == "Ground") ? p1.x : (p1.x + p2.x) / 2;
        int center_y = (elem.getType() == "Ground") ? p1.y : (p1.y + p2.y) / 2;
        
        // Position component at center
        int dest_x = center_x - scaled_w / 2;
        int dest_y = center_y - scaled_h / 2;
        
        // Ensure component stays within canvas bounds
        int padding = 2;
        dest_x = std::max(view_area.x + padding, 
                         std::min(dest_x, view_area.x + view_area.w - scaled_w - padding));
        dest_y = std::max(view_area.y + padding, 
                         std::min(dest_y, view_area.y + view_area.h - scaled_h - padding));
        
        SDL_Rect dest = { dest_x, dest_y, scaled_w, scaled_h };
        
        // Render the fixed-size component
        SDL_RenderCopy(renderer, texture, nullptr, &dest);
        
        // Draw a small border around the component to make it more visible
        SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255); // Blue border
        SDL_RenderDrawRect(renderer, &dest);
        
        // Debug: Print component info
        std::cout << "Rendering " << type << " at (" << dest_x << "," << dest_y << ") size " << scaled_w << "x" << scaled_h 
                  << " (original: " << original_w << "x" << original_h << ")" << std::endl;
    }
}

void SchematicView::drawNodeLabels(SDL_Renderer* renderer) {
    if (!font) return;
    
    // Draw custom node labels (like VCC, GND, etc.)
    for (const auto& pair : circuit_backend.getNodeLabels()) {
        SDL_Point pos = getNodePosition(pair.first);
        SDL_Surface* surface = TTF_RenderText_Blended(font, pair.second.c_str(), {0, 0, 0, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { pos.x + 5, pos.y - 20, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // Draw voltage values if available and enabled
    if (show_voltages && !latest_results.empty()) {
        std::set<std::string> drawn_voltage_nodes;

        // Debug: Print available signals
        static bool debug_printed = false;
        if (!debug_printed) {
            std::cout << "[VOLTAGE DISPLAY] Available signals: ";
            for (const auto& pair : latest_results) {
                std::cout << pair.first << " ";
            }
            std::cout << std::endl;
            debug_printed = true;
        }

        // Check if voltage display is enabled
        static bool voltage_display_debug = false;
        if (!voltage_display_debug) {
            std::cout << "[VOLTAGE DISPLAY] show_voltages = " << (show_voltages ? "true" : "false") << std::endl;
            std::cout << "[VOLTAGE DISPLAY] latest_results.size() = " << latest_results.size() << std::endl;
            voltage_display_debug = true;
        }

        for (const auto& element : circuit_backend.getElements()) {
            if (!element) continue;

            std::string node1 = element->getNode1Id();
            std::string node2 = element->getNode2Id();

            auto drawVoltageAtNode = [&](const std::string& node_id) {
                if (node_id.empty() || node_id == "0" || drawn_voltage_nodes.find(node_id) != drawn_voltage_nodes.end()) return;

                // Look for voltage signal in results (format: V(node_id))
                std::string voltage_signal = "V(" + node_id + ")";
                if (latest_results.count(voltage_signal) && !latest_results.at(voltage_signal).empty()) {
                    // Get the last voltage value (most recent time point)
                    double voltage = latest_results.at(voltage_signal).back();

                    // Debug: Print found voltage
                    static std::set<std::string> debug_nodes;
                    if (debug_nodes.find(node_id) == debug_nodes.end()) {
                        std::cout << "[VOLTAGE DISPLAY] Found voltage for node " << node_id << ": " << voltage << "V" << std::endl;
                        debug_nodes.insert(node_id);
                    }

                    SDL_Point pos = getNodePosition(node_id);

                    // Format voltage with appropriate precision
                    std::stringstream ss;
                    if (std::abs(voltage) < 0.001) {
                        ss << std::fixed << std::setprecision(6) << voltage << "V";
                    } else if (std::abs(voltage) < 0.01) {
                        ss << std::fixed << std::setprecision(4) << voltage << "V";
                    } else if (std::abs(voltage) < 1.0) {
                        ss << std::fixed << std::setprecision(3) << voltage << "V";
                    } else {
                        ss << std::fixed << std::setprecision(2) << voltage << "V";
                    }

                    std::string voltage_text = ss.str();
                    SDL_Surface* surface = TTF_RenderText_Blended(font, voltage_text.c_str(), {255, 100, 100, 255});
                    if (surface) {
                        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

                        // Position voltage below the node (accounting for existing labels)
                        SDL_Rect dest = {
                            pos.x - surface->w/2,
                            pos.y + 15,
                            surface->w,
                            surface->h
                        };

                        // Add background for better readability
                        SDL_Rect bg_rect = {dest.x - 3, dest.y - 2, dest.w + 6, dest.h + 4};
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
                        SDL_RenderFillRect(renderer, &bg_rect);

                        SDL_RenderCopy(renderer, texture, nullptr, &dest);
                        SDL_DestroyTexture(texture);
                        SDL_FreeSurface(surface);
                    }
                } else {
                    // Debug: Print when voltage signal not found
                    static std::set<std::string> debug_missing;
                    if (debug_missing.find(node_id) == debug_missing.end()) {
                        std::cout << "[VOLTAGE DISPLAY] No voltage signal found for node " << node_id << " (searched for: " << voltage_signal << ")" << std::endl;
                        debug_missing.insert(node_id);
                    }
                }
                drawn_voltage_nodes.insert(node_id);
            };

            drawVoltageAtNode(node1);
            drawVoltageAtNode(node2);
        }
    }
    
    // Draw all node names when show_node_names is enabled (after analysis)
    if (show_node_names) {
        // Clear previous node name positions
        clearNodeNamePositions();
        
        std::set<std::string> drawn_nodes; // Avoid drawing duplicate nodes
        std::map<std::pair<int, int>, int> grid_usage; // Track grid position usage for smart spacing
        
        for (const auto& element : circuit_backend.getElements()) {
            if (!element) continue;
            
            // Draw node names for all nodes used by elements
            std::string node1 = element->getNode1Id();
            std::string node2 = element->getNode2Id();
            
            auto drawNodeWithSmartPlacement = [&](const std::string& node_id) {
                if (node_id.empty() || node_id == "0" || drawn_nodes.find(node_id) != drawn_nodes.end()) return;
                
                SDL_Point pos = getNodePosition(node_id);
                int grid_x = (pos.x - view_area.x) / GRID_SIZE;
                int grid_y = (pos.y - view_area.y) / GRID_SIZE;
                std::pair<int, int> grid_pos = {grid_x, grid_y};
                
                // Calculate smart offset based on usage count in this grid area
                int usage_count = grid_usage[grid_pos]++;
                int offset_x = 0;
                int offset_y = -35 - (usage_count * 25); // More spacing between node labels
                
                // Keep labels within reasonable bounds - allow more space above
                offset_y = std::max(-120, std::min(offset_y, -15));
                offset_x = std::max(-50, std::min(offset_x, 50));
                
                // Create label with better formatting - shorter node names
                std::string display_name = node_id;
                if (node_id.length() > 8) {
                    display_name = node_id.substr(0, 6) + "..";
                }
                
                SDL_Surface* surface = TTF_RenderText_Blended(font, display_name.c_str(), {20, 60, 120, 255});
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                    
                    // Center the text horizontally on the node
                    SDL_Rect dest = { 
                        pos.x - surface->w/2 + offset_x, 
                        pos.y + offset_y, 
                        surface->w, 
                        surface->h 
                    };
                    
                    // Add background rectangle for better readability
                    SDL_Rect bg_rect = {dest.x - 2, dest.y - 1, dest.w + 4, dest.h + 2};
                    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 200);
                    SDL_RenderFillRect(renderer, &bg_rect);
                    
                    // Store node name position for hover detection (with 0.5cm radius = ~19 pixels)
                    SDL_Rect hover_bounds = {
                        dest.x - 19,  // Expand bounds by 19 pixels (0.5cm) on each side
                        dest.y - 19,
                        dest.w + 38,  // 19 pixels on each side
                        dest.h + 38
                    };
                    addNodeNamePosition(node_id, hover_bounds, display_name);
                    
                    SDL_RenderCopy(renderer, texture, nullptr, &dest);
                    SDL_DestroyTexture(texture);
                    SDL_FreeSurface(surface);
                }
                drawn_nodes.insert(node_id);
            };
            
            drawNodeWithSmartPlacement(node1);
            drawNodeWithSmartPlacement(node2);
        }
    }
}

void SchematicView::drawPins(SDL_Renderer* renderer) {
    for (const auto& pin : pins) {
        if (!pin) continue;

        SDL_Point pos = pin->getPosition();

        // Determine pin color based on state
        if (pin->is_hovered) {
            SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255); // Bright orange for hovered pins
        } else if (pin->isConnected()) {
            SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); // Dark green for connected pins
        } else {
            SDL_SetRenderDrawColor(renderer, 200, 150, 0, 255); // Dark yellow for unconnected pins
        }

        // Draw pin circle with glow effect for hovered pins
        const int PIN_RADIUS = pin->is_hovered ? 6 : 4;  // Larger radius for hovered pins
        for (int x = -PIN_RADIUS; x <= PIN_RADIUS; x++) {
            for (int y = -PIN_RADIUS; y <= PIN_RADIUS; y++) {
                if (x * x + y * y <= PIN_RADIUS * PIN_RADIUS) {
                    SDL_RenderDrawPoint(renderer, pos.x + x, pos.y + y);
                }
            }
        }

        // Add glow effect for hovered pins
        if (pin->is_hovered) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 100, 150); // Semi-transparent glow
            const int GLOW_RADIUS = 8;
            for (int x = -GLOW_RADIUS; x <= GLOW_RADIUS; x++) {
                for (int y = -GLOW_RADIUS; y <= GLOW_RADIUS; y++) {
                    if (x * x + y * y <= GLOW_RADIUS * GLOW_RADIUS && x * x + y * y > PIN_RADIUS * PIN_RADIUS) {
                        SDL_RenderDrawPoint(renderer, pos.x + x, pos.y + y);
                    }
                }
            }
        }

        // Draw pin border
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawPoint(renderer, pos.x, pos.y);
    }
}

void SchematicView::drawWires(SDL_Renderer* renderer) {
    for (const auto& wire : wires) {
        if (!wire) continue;
        
        auto start_pin = wire->getStartPin();
        auto end_pin = wire->getEndPin();
        
        if (start_pin && end_pin) {
            SDL_Point start = start_pin->getPosition();
            SDL_Point end = end_pin->getPosition();
            
            // Draw wire line
            if (wire->isSelected()) {
                SDL_SetRenderDrawColor(renderer, 200, 150, 0, 255); // Dark yellow for selected
            } else {
                SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255); // Dark blue for normal
            }
            
            SDL_RenderDrawLine(renderer, start.x, start.y, end.x, end.y);
            
            // Draw waypoints if any
            const auto& waypoints = wire->getWaypoints();
            for (size_t i = 0; i < waypoints.size(); ++i) {
                SDL_Point wp = waypoints[i];
                SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
                SDL_RenderDrawPoint(renderer, wp.x, wp.y);
                
                // Connect waypoints
                if (i == 0) {
                    SDL_RenderDrawLine(renderer, start.x, start.y, wp.x, wp.y);
                } else {
                    SDL_RenderDrawLine(renderer, waypoints[i-1].x, waypoints[i-1].y, wp.x, wp.y);
                }
            }
            
            // Connect last waypoint to end
            if (!waypoints.empty()) {
                SDL_RenderDrawLine(renderer, waypoints.back().x, waypoints.back().y, end.x, end.y);
            }
        }
    }
}

void SchematicView::updatePinPositions() {
    // Clear both pins vector and pin_index to prevent duplicates
    pins.clear();
    pin_index.clear();
    
    // Rebuild pins from scratch to ensure no duplicates
    for (const auto& element : circuit_backend.getElements()) {
        if (!element) continue;
        
        std::string elem_name = element->getName();
        std::string elem_type = element->getType();
        std::string node1_id = element->getNode1Id();
        std::string node2_id = element->getNode2Id();
        
        // Skip wire elements as they don't need pins (they connect existing pins)
        if (elem_type == "CircuitWire") continue;
        
        // Get element positions based on their nodes
        SDL_Point node1_pos = getNodePosition(node1_id);
        SDL_Point node2_pos = getNodePosition(node2_id);
        
        // For now, use simplified pin positioning directly at node positions
        // This ensures pins are exactly where components are placed
        SDL_Point pin1_pos = node1_pos;
        SDL_Point pin2_pos = node2_pos;
        
        // Adjust for visualization - offset pins slightly from exact node position
        const int PIN_OFFSET = 20;
        
        if (elem_type == "Ground") {
            // Ground only has one pin exactly at the node position
            pin1_pos = node1_pos;
        } else {
            // For two-terminal elements, place pins at the nodes with small offsets
            pin1_pos = {node1_pos.x - PIN_OFFSET, node1_pos.y};
            pin2_pos = {node2_pos.x + PIN_OFFSET, node2_pos.y};
        }
        
        auto key1 = elem_name + ".1";
        auto key2 = elem_name + ".2";
        
        // Create pin1
        auto pin1 = std::make_shared<Pin>(key1, elem_name, 1, pin1_pos);
        pin1->setNodeId(node1_id);
        pin_index[key1] = pin1;
        pins.push_back(pin1);
        
        // Create pin2 for non-ground elements
        if (elem_type != "Ground" && !node2_id.empty()) {
            auto pin2 = std::make_shared<Pin>(key2, elem_name, 2, pin2_pos);
            pin2->setNodeId(node2_id);
            pin_index[key2] = pin2;
            pins.push_back(pin2);
        }
    }
    
    std::cout << "Updated pin positions. Total pins: " << pins.size() << std::endl;
    
    // Debug: Check for duplicate pins
    std::set<std::string> pin_ids;
    for (const auto& pin : pins) {
        if (pin) {
            std::string pin_id = pin->getFullId();
            if (pin_ids.count(pin_id) > 0) {
                std::cout << "[WARNING] Duplicate pin detected: " << pin_id << std::endl;
            } else {
                pin_ids.insert(pin_id);
            }
        }
    }
    
    // Debug: Print all pin positions
    for (const auto& pin : pins) {
        if (pin) {
            SDL_Point pos = pin->getPosition();
            std::cout << "Pin " << pin->getFullId() << " at (" << pos.x << ", " << pos.y << ") node: " << pin->getNodeId() << std::endl;
        }
    }
}

void SchematicView::clearWires() {
    for (auto& wire : wires) {
        if (!wire) continue;
        if (auto s = wire->getStartPin()) s->removeWire(wire);
        if (auto e = wire->getEndPin()) e->removeWire(wire);
    }
    wires.clear();
}

void SchematicView::createWire(std::shared_ptr<Pin> start_pin, std::shared_ptr<Pin> end_pin) {
    if (start_pin && end_pin && start_pin != end_pin) {
        std::string wire_id = "wire_" + std::to_string(wires.size());
        auto gui_wire = std::make_shared<GuiWire>(wire_id, start_pin, end_pin);
        wires.push_back(gui_wire);
        LOG_INFO(std::string("[GUI] create GUI wire ") + wire_id + " from " + start_pin->getFullId() + " to " + end_pin->getFullId());
        
        // Update pin connection status
        start_pin->addWire(gui_wire);
        end_pin->addWire(gui_wire);
        start_pin->updateConnectionStatus();
        end_pin->updateConnectionStatus();
        
        // Also create the electrical circuit wire for analysis (only if nodes differ)
        if (start_pin->getNodeId() != end_pin->getNodeId()) {
            std::string circuit_wire_id = "W" + std::to_string(circuit_backend.getElements().size() + 1);
            auto circuit_wire = std::make_unique<CircuitWire>(circuit_wire_id, start_pin->getNodeId(), end_pin->getNodeId());
            circuit_backend.addElement(std::move(circuit_wire));
            LOG_INFO(std::string("[GUI] create backend wire ") + circuit_wire_id + " " + start_pin->getNodeId() + "->" + end_pin->getNodeId());
        }
        
        std::cout << "Created GUI wire and circuit wire between " << start_pin->getNodeId() << " and " << end_pin->getNodeId() << std::endl;
    }
}

void SchematicView::createGuiWireOnly(std::shared_ptr<Pin> start_pin, std::shared_ptr<Pin> end_pin) {
    if (start_pin && end_pin && start_pin != end_pin) {
        std::string wire_id = "wire_" + std::to_string(wires.size());
        auto gui_wire = std::make_shared<GuiWire>(wire_id, start_pin, end_pin);
        wires.push_back(gui_wire);
        start_pin->addWire(gui_wire);
        end_pin->addWire(gui_wire);
        start_pin->updateConnectionStatus();
        end_pin->updateConnectionStatus();
    }
}

void SchematicView::rebuildWiresFromCircuit() {
    // Clear existing GUI wires
    clearWires();
    
    // Rebuild pins first to ensure they exist and are not duplicated
    updatePinPositions();
    
    std::cout << "[SchematicView] Rebuilding wires from circuit data..." << std::endl;
    std::cout << "[SchematicView] Available pins: " << pins.size() << std::endl;
    
    int wire_count = 0;
    int skipped_count = 0;
    
    // Find all wire elements in the circuit
    for (const auto& elem : circuit_backend.getElements()) {
        if (elem->getType() == "Wire") {
            std::string node1 = elem->getNode1Id();
            std::string node2 = elem->getNode2Id();
            
            if (node1.empty() || node2.empty() || node1 == node2) {
                skipped_count++;
                continue; // Skip invalid wires
            }
            
            // Find pins connected to these nodes
            std::shared_ptr<Pin> pin1 = nullptr;
            std::shared_ptr<Pin> pin2 = nullptr;
            
            for (const auto& pin : pins) {
                if (pin && pin->getNodeId() == node1) {
                    pin1 = pin;
                } else if (pin && pin->getNodeId() == node2) {
                    pin2 = pin;
                }
            }
            
            // If we found both pins, create a GUI wire
            if (pin1 && pin2) {
                createGuiWireOnly(pin1, pin2);
                wire_count++;
                std::cout << "[SchematicView] Rebuilt wire: " << node1 << " -> " << node2 << std::endl;
            } else {
                skipped_count++;
                // Only show warning for non-ground connections
                if (node1 != circuit_backend.getGroundNodeId() && node2 != circuit_backend.getGroundNodeId()) {
                    std::cout << "[SchematicView] Warning: Could not find pins for wire " << node1 << " -> " << node2 << std::endl;
                    if (!pin1) std::cout << "  Missing pin for node: " << node1 << std::endl;
                    if (!pin2) std::cout << "  Missing pin for node: " << node2 << std::endl;
                }
            }
        }
    }
    
    std::cout << "[SchematicView] Wire rebuild complete: " << wire_count << " wires rebuilt, " << skipped_count << " skipped" << std::endl;
}

void SchematicView::deleteWire(const std::string& wire_id) {
    auto it = std::remove_if(wires.begin(), wires.end(),
        [&](const std::shared_ptr<GuiWire>& wire) { return wire->getId() == wire_id; });
    
    if (it != wires.end()) {
        LOG_INFO(std::string("[GUI] delete GUI wire ") + wire_id);
        // Remove wire from pins
        (*it)->getStartPin()->removeWire(*it);
        (*it)->getEndPin()->removeWire(*it);
        (*it)->getStartPin()->updateConnectionStatus();
        (*it)->getEndPin()->updateConnectionStatus();
        wires.erase(it, wires.end());
    }
}

std::shared_ptr<Pin> SchematicView::getPinAt(int x, int y) const {
    std::shared_ptr<Pin> closest_pin = nullptr;
    double min_distance = std::numeric_limits<double>::max();

    for (const auto& pin : pins) {
        if (pin && pin->isAtPosition(x, y)) {
            // Calculate distance from click to pin center
            double dx = x - pin->getPosition().x;
            double dy = y - pin->getPosition().y;
            double distance = std::sqrt(dx * dx + dy * dy);

            // If this pin is closer than previous closest, select it
            if (distance < min_distance) {
                min_distance = distance;
                closest_pin = pin;
            }
        }
    }
    return closest_pin;
}

std::shared_ptr<Pin> SchematicView::getPinNear(int x, int y, int hover_radius) const {
    std::shared_ptr<Pin> closest_pin = nullptr;
    double min_distance = std::numeric_limits<double>::max();

    for (const auto& pin : pins) {
        if (pin && pin->isNearPosition(x, y, hover_radius)) {
            // Calculate distance from click to pin center
            double dx = x - pin->getPosition().x;
            double dy = y - pin->getPosition().y;
            double distance = std::sqrt(dx * dx + dy * dy);

            // If this pin is closer than previous closest, select it
            if (distance < min_distance) {
                min_distance = distance;
                closest_pin = pin;
            }
        }
    }
    return closest_pin;
}

void SchematicView::updatePinHoverStates(int mouse_x, int mouse_y, bool is_wire_mode, bool is_creating_wire) {
    for (auto& pin : pins) {
        if (pin) {
            // Reset hover state first
            bool was = pin->is_hovered;
            pin->is_hovered = false;

            // Only highlight pins in wire mode or when a wire is being drawn
            if (is_wire_mode || is_creating_wire) {
                if (pin->isNearPosition(mouse_x, mouse_y)) {
                    pin->is_hovered = true;
                }
            }
            if (was != pin->is_hovered) {
                LOG_INFO(std::string("[GUI] pin hover ") + pin->getFullId() + (pin->is_hovered?" ON":" OFF"));
            }
        }
    }
}

void SchematicView::render(SDL_Renderer* renderer) {
    // Draw background
    SDL_SetRenderDrawColor(renderer, 245, 245, 250, 255);
    SDL_RenderFillRect(renderer, &view_area);
    
    // Draw grid with different intensities for major and minor lines
    SDL_SetRenderDrawColor(renderer, 220, 220, 225, 255);
    for (int x = view_area.x; x < view_area.x + view_area.w; x += GRID_SIZE) {
        SDL_RenderDrawLine(renderer, x, view_area.y, x, view_area.y + view_area.h);
    }
    for (int y = view_area.y; y < view_area.y + view_area.h; y += GRID_SIZE) {
        SDL_RenderDrawLine(renderer, view_area.x, y, view_area.x + view_area.w, y);
    }
    
    // Draw major grid lines (every 5th line) with higher intensity
    SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
    for (int x = view_area.x; x < view_area.x + view_area.w; x += GRID_SIZE * 5) {
        SDL_RenderDrawLine(renderer, x, view_area.y, x, view_area.y + view_area.h);
    }
    for (int y = view_area.y; y < view_area.y + view_area.h; y += GRID_SIZE * 5) {
        SDL_RenderDrawLine(renderer, view_area.x, y, view_area.x + view_area.w, y);
    }
    
    // Draw border
    SDL_SetRenderDrawColor(renderer, 180, 180, 190, 255);
    SDL_RenderDrawRect(renderer, &view_area);
    
    // Draw all circuit elements
    for (const auto& el : circuit_backend.getElements()) {
        if (el) drawElementSymbol(renderer, *el);
    }
    
    // Draw wires first (behind pins)
    drawWires(renderer);
    
    // Draw pins on top
    drawPins(renderer);
    
    // Draw node labels
    drawNodeLabels(renderer);
}

bool SchematicView::isPointOnWire(int x, int y) {
    // Check if point is near any wire
    for (const auto& wire : wires) {
        if (!wire) continue;
        
        SDL_Point start = wire->getStartPin()->getPosition();
        SDL_Point end = wire->getEndPin()->getPosition();
        
        // Simple distance check - if point is within 10 pixels of wire line
        double distance = pointToLineDistance(x, y, start.x, start.y, end.x, end.y);
        if (distance < 10.0) {
            return true;
        }
    }
    return false;
}

std::string SchematicView::getClosestNodeToPoint(int x, int y) {
    std::string closest_node = "";
    double min_distance = 50.0; // Maximum distance to consider
    
    // Check all nodes used by elements
    std::set<std::string> all_nodes;
    for (const auto& element : circuit_backend.getElements()) {
        if (!element) continue;
        all_nodes.insert(element->getNode1Id());
        all_nodes.insert(element->getNode2Id());
    }
    
    for (const auto& node_id : all_nodes) {
        if (node_id.empty()) continue;
        
        SDL_Point node_pos = getNodePosition(node_id);
        double distance = sqrt((x - node_pos.x) * (x - node_pos.x) + (y - node_pos.y) * (y - node_pos.y));
        
        if (distance < min_distance) {
            min_distance = distance;
            closest_node = node_id;
        }
    }
    
    return closest_node;
}

double SchematicView::pointToLineDistance(int px, int py, int x1, int y1, int x2, int y2) {
    // Calculate distance from point to line segment
    double A = px - x1;
    double B = py - y1;
    double C = x2 - x1;
    double D = y2 - y1;
    
    double dot = A * C + B * D;
    double lenSq = C * C + D * D;
    
    if (lenSq == 0) {
        // Line is actually a point
        return sqrt(A * A + B * B);
    }
    
    double param = dot / lenSq;
    
    double xx, yy;
    if (param < 0) {
        xx = x1;
        yy = y1;
    } else if (param > 1) {
        xx = x2;
        yy = y2;
    } else {
        xx = x1 + param * C;
        yy = y1 + param * D;
    }
    
    double dx = px - xx;
    double dy = py - yy;
    return sqrt(dx * dx + dy * dy);
}

// Node name hover detection methods implementation
std::string SchematicView::getNodeNameAt(int mouse_x, int mouse_y) const {
    for (const auto& node_info : node_name_positions) {
        if (mouse_x >= node_info.bounds.x && mouse_x <= node_info.bounds.x + node_info.bounds.w &&
            mouse_y >= node_info.bounds.y && mouse_y <= node_info.bounds.y + node_info.bounds.h) {
            return node_info.node_id;
        }
    }
    return "";
}

void SchematicView::clearNodeNamePositions() {
    node_name_positions.clear();
}

void SchematicView::addNodeNamePosition(const std::string& node_id, const SDL_Rect& bounds, const std::string& display_name) {
    NodeNameInfo info;
    info.node_id = node_id;
    info.bounds = bounds;
    info.display_name = display_name;
    node_name_positions.push_back(info);
}

// --- Oscilloscope Integration ---
// PlotView constructor replaced with Oscilloscope
// The rest of PlotView implementation is commented out
/*
PlotView::PlotView(int x, int y, int w, int h, TTF_Font* font) : font(font) {
    view_area = { x, y, w, h };
    
    // Initialize cursor manager
    cursor_manager.setPlotArea(view_area);
    cursor_manager.addCursor("C1", {255, 255, 0, 255}); // Yellow cursor
}

void PlotView::setData(const std::vector<double>& time_points, const std::map<std::string, std::vector<double>>& analysis_results) {
    current_mode = PlotMode::TRANSIENT;
    signals.clear();
    x_values = time_points;

    // Debug output
    std::cout << "[PLOT SETDATA] Called with " << time_points.size() << " time points, " << analysis_results.size() << " signals" << std::endl;
    for (const auto& pair : analysis_results) {
        std::cout << "[PLOT SETDATA]   Available signal: '" << pair.first << "' (" << pair.second.size() << " points)" << std::endl;
    }

    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};
    int i = 0;
    int voltage_signals = 0;
    int current_signals = 0;

    for (const auto& pair : analysis_results) {
        if (pair.first.find("V(") != std::string::npos) {
            signals.push_back({pair.first, pair.second, colors[i++ % colors.size()]});
            voltage_signals++;
            std::cout << "[PLOT SETDATA]   Added voltage signal: '" << pair.first << "'" << std::endl;
        } else if (pair.first.find("I(") != std::string::npos) {
            // Also include current signals for completeness
            signals.push_back({pair.first, pair.second, colors[i++ % colors.size()]});
            current_signals++;
            std::cout << "[PLOT SETDATA]   Added current signal: '" << pair.first << "'" << std::endl;
        } else {
            std::cout << "[PLOT SETDATA]   Skipped unknown signal: '" << pair.first << "'" << std::endl;
        }
    }

    std::cout << "[PLOT SETDATA] Signal summary: " << voltage_signals << " voltages, " << current_signals << " currents" << std::endl;

    std::cout << "[PLOT SETDATA] Final result: " << x_values.size() << " time points, " << signals.size() << " signals to plot" << std::endl;

    if (!signals.empty()) {
    autoZoom();
    updateCursorManager();
    } else {
        std::cout << "[PLOT SETDATA] WARNING: No signals to plot after filtering!" << std::endl;
    }
}

void PlotView::setDataFiltered(const std::vector<double>& time_points, const std::map<std::string, std::vector<double>>& analysis_results, const std::set<std::string>& selected_names) {
    current_mode = PlotMode::TRANSIENT;
    signals.clear();
    x_values = time_points;
    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};

    // Debug: Check if we're getting empty data
    {
        std::cout << "[PLOT DEBUG] setDataFiltered called with:" << std::endl;
        std::cout << "[PLOT DEBUG]   time_points.size() = " << time_points.size() << std::endl;
        std::cout << "[PLOT DEBUG]   analysis_results.size() = " << analysis_results.size() << std::endl;
        std::cout << "[PLOT DEBUG]   selected_names.size() = " << selected_names.size() << std::endl;

        if (!time_points.empty()) {
            std::cout << "[PLOT DEBUG]   time range: " << time_points.front() << " to " << time_points.back() << std::endl;
        }

        for (const auto& pair : analysis_results) {
            std::cout << "[PLOT DEBUG]   signal '" << pair.first << "' has " << pair.second.size() << " points" << std::endl;
            if (!pair.second.empty()) {
                std::cout << "[PLOT DEBUG]     value range: " << *std::min_element(pair.second.begin(), pair.second.end())
                          << " to " << *std::max_element(pair.second.begin(), pair.second.end()) << std::endl;
            }
        }

        std::cout << "[PLOT DEBUG]   selected_names: ";
        for (const auto& name : selected_names) {
            std::cout << name << " ";
        }
        std::cout << std::endl;

        // Check for matching signals
        std::cout << "[PLOT DEBUG]   Matching signals: ";
        for (const auto& pair : analysis_results) {
            if (selected_names.find(pair.first) != selected_names.end()) {
                std::cout << pair.first << " ";
            }
        }
        std::cout << std::endl;
    }

    int i = 0;
    int matched_signals = 0;
    for (const auto& pair : analysis_results) {
        if (!selected_names.empty() && selected_names.find(pair.first) == selected_names.end()) {
            continue; // Skip signals not in selection
        }
        signals.push_back({pair.first, pair.second, colors[i++ % colors.size()]});
        matched_signals++;
    }

    // If no signals matched the selection, show all available signals as fallback
    if (matched_signals == 0 && !analysis_results.empty()) {
        std::cout << "[PLOT DEBUG] No signals matched selection, showing all available signals" << std::endl;
        signals.clear();
        i = 0;
        for (const auto& pair : analysis_results) {
            signals.push_back({pair.first, pair.second, colors[i++ % colors.size()]});
        }
    }
    {
        std::stringstream ss; ss << "PlotView::setDataFiltered -> x:" << x_values.size() << ", traces:" << signals.size();
        std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());

        // Debug: Print some sample data
        if (!x_values.empty() && !signals.empty()) {
            std::cout << "[PLOT DEBUG] First few time points: ";
            for (size_t j = 0; j < std::min(size_t(5), x_values.size()); ++j) {
                std::cout << x_values[j] << " ";
            }
            std::cout << std::endl;

            std::cout << "[PLOT DEBUG] First signal '" << signals[0].name << "' first few values: ";
            for (size_t j = 0; j < std::min(size_t(5), signals[0].y_values.size()); ++j) {
                std::cout << signals[0].y_values[j] << " ";
            }
            std::cout << std::endl;
        }
    }
    autoZoom();
    updateCursorManager();
}

void PlotView::setDataAC(const std::vector<double>& freq_points, const std::map<std::string, std::vector<Complex>>& ac_results) {
    current_mode = PlotMode::AC_MAGNITUDE;
    signals.clear();
    x_values = freq_points;

    std::cout << "[PLOTVIEW DEBUG] setDataAC called with " << freq_points.size() << " frequency points" << std::endl;
    std::cout << "[PLOTVIEW DEBUG] AC results has " << ac_results.size() << " signals" << std::endl;

    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};
    int i = 0;
    for (const auto& pair : ac_results) {
        std::cout << "[PLOTVIEW DEBUG] Processing signal: " << pair.first << std::endl;
        if (pair.first.find("V(") != std::string::npos) {
            std::vector<double> magnitudes;
            for(const auto& val : pair.second) {
                magnitudes.push_back(std::abs(val));
            }
            signals.push_back({pair.first, magnitudes, colors[i++ % colors.size()]});
            std::cout << "[PLOTVIEW DEBUG] Added voltage signal: " << pair.first << " with " << magnitudes.size() << " points" << std::endl;
        }
    }

    std::cout << "[PLOTVIEW DEBUG] Total signals added: " << signals.size() << std::endl;
    autoZoom();
    updateCursorManager();
}

void PlotView::setDataPhase(const std::vector<double>& phase_points, const std::map<std::string, std::vector<Complex>>& phase_results) {
    current_mode = PlotMode::PHASE_MAGNITUDE;
    signals.clear();
    x_values = phase_points;

    std::cout << "[PLOTVIEW DEBUG] setDataPhase called with " << phase_points.size() << " phase points" << std::endl;
    std::cout << "[PLOTVIEW DEBUG] Phase results has " << phase_results.size() << " signals" << std::endl;

    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};
    int i = 0;
    for (const auto& pair : phase_results) {
        std::cout << "[PLOTVIEW DEBUG] Processing signal: " << pair.first << std::endl;
        if (pair.first.find("V(") != std::string::npos) {
            std::vector<double> magnitudes;
            for(const auto& val : pair.second) {
                magnitudes.push_back(std::abs(val));
            }
            signals.push_back({pair.first, magnitudes, colors[i++ % colors.size()]});
            std::cout << "[PLOTVIEW DEBUG] Added voltage signal: " << pair.first << " with " << magnitudes.size() << " points" << std::endl;
        }
    }

    std::cout << "[PLOTVIEW DEBUG] Total signals added: " << signals.size() << std::endl;
    autoZoom();
    updateCursorManager();
}


void PlotView::autoZoom() {
    if (x_values.empty() || signals.empty()) return;
    double min_x = x_values.front(), max_x = x_values.back();
    double min_y = std::numeric_limits<double>::max(), max_y = std::numeric_limits<double>::lowest();
    
    // Find the actual data range across all signals
    for (const auto& sig : signals) {
        for (double val : sig.y_values) {
            if (std::isfinite(val)) { // Skip NaN and infinity values
                min_y = std::min(min_y, val);
                max_y = std::max(max_y, val);
            }
        }
    }
    
    // Handle edge cases for y-axis (constant signals)
    if (min_y == max_y) { 
        if (std::abs(min_y) < 1e-12) {
            // For very small constant values (near zero), show a reasonable range
            min_y = -1.0; max_y = 1.0; 
        } else {
            // For non-zero constant values, add 10% margin around the value
            double abs_val = std::abs(min_y);
            double margin = std::max(abs_val * 0.1, 0.1); // At least 0.1 margin
            min_y = min_y - margin;
            max_y = max_y + margin;
        }
    }
    
    // Handle cases where we have very small y-range (essentially constant)
    double y_range = max_y - min_y;
    if (y_range < 1e-9) {
        double center = (min_y + max_y) / 2.0;
        double expand = std::max(std::abs(center) * 0.1, 1.0);
        min_y = center - expand;
        max_y = center + expand;
        y_range = max_y - min_y; // Recalculate after expansion
    }
    
    // Smart margin calculation based on data range
    double margin_y;
    if (y_range > 1000) {
        margin_y = y_range * 0.02; // Smaller margin for large values
    } else if (y_range > 1) {
        margin_y = y_range * 0.05; // Medium margin for normal values
    } else {
        margin_y = y_range * 0.1; // Larger margin for small values
    }
    
    min_y -= margin_y; 
    max_y += margin_y;
    
    // Ensure we have valid ranges
    if (max_x - min_x == 0) {
        max_x = min_x + 1.0;
    }
    if (max_y - min_y == 0) {
        // For constant signals, create a meaningful Y range around the value
        double center = (max_y + min_y) / 2.0;
        if (std::abs(center) > 1e-6) {
            // For non-zero signals, create range as percentage of signal value
            double range = std::max(0.1, std::abs(center) * 0.1);
            min_y = center - range;
            max_y = center + range;
        } else {
            // For zero signals, create small symmetric range
            min_y = -0.1;
            max_y = 0.1;
        }
    }
    
    // Calculate scaling to fit the plot area (reserve more space for axis labels)
    double plot_width = view_area.w - 120; // More space for y-axis labels
    double plot_height = view_area.h - 80; // More space for x-axis labels
    
    scale_x = plot_width / (max_x - min_x);
    scale_y = plot_height / (max_y - min_y);
    offset_x = -min_x;
    offset_y = -min_y;
    
    // Log the auto-zoom results for debugging
    std::stringstream ss;
    ss << "[Plot] Auto-zoom: x=[" << min_x << ", " << max_x << "], y=[" << min_y << ", " << max_y << "], scale=(" << scale_x << ", " << scale_y << ")";
    ErrorManager::info(ss.str());
}

void PlotView::handleEvent(const SDL_Event& event) {
    int mouse_x = 0, mouse_y = 0;
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONUP) {
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            mouse_x = event.button.x;
            mouse_y = event.button.y;
        } else if (event.type == SDL_MOUSEMOTION) {
            mouse_x = event.motion.x;
            mouse_y = event.motion.y;
        } else {
            mouse_x = event.button.x;
            mouse_y = event.button.y;
        }
    }
    
    // Check if in resize handle (bottom-right corner)
    bool in_resize_handle = (mouse_x >= view_area.x + view_area.w - resize_handle_size &&
                            mouse_x <= view_area.x + view_area.w &&
                            mouse_y >= view_area.y + view_area.h - resize_handle_size &&
                            mouse_y <= view_area.y + view_area.h);
    
    // Check if in title bar (top 20 pixels for dragging)
    bool in_title_bar = (mouse_x >= view_area.x && mouse_x <= view_area.x + view_area.w &&
                         mouse_y >= view_area.y && mouse_y <= view_area.y + 20);
    
    // Check if in plot area
    bool in_plot_area = (mouse_x >= view_area.x && mouse_x <= view_area.x + view_area.w &&
                        mouse_y >= view_area.y + 20 && mouse_y <= view_area.y + view_area.h - 20);
    
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (in_resize_handle) {
            is_resizing = true;
            drag_start_x = mouse_x;
            drag_start_y = mouse_y;
            ErrorManager::info("[Plot] Resize mode activated");
        } else if (in_title_bar) {
            is_dragging = true;
            drag_start_x = mouse_x - view_area.x;
            drag_start_y = mouse_y - view_area.y;
            ErrorManager::info("[Plot] Drag mode activated");
        } else if (in_plot_area) {
            // Normal cursor placement
            cursor_manager.addCursor("C1", {255, 255, 0, 255});
            cursor1.visible = true;
            cursor1.screen_x = mouse_x;
            updateCursorValue(cursor1);
            ErrorManager::info("[Cursor] Cursor 1 placed");
        }
    } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        if (is_dragging) {
            is_dragging = false;
            ErrorManager::info("[Plot] Drag completed");
        }
        if (is_resizing) {
            is_resizing = false;
            ErrorManager::info("[Plot] Resize completed");
        }
    } else if (event.type == SDL_MOUSEMOTION) {
        if (is_dragging) {
            view_area.x = mouse_x - drag_start_x;
            view_area.y = mouse_y - drag_start_y;
            cursor_manager.setPlotArea(view_area);
        } else if (is_resizing) {
            int new_width = std::max(200, view_area.w + (mouse_x - drag_start_x));
            int new_height = std::max(150, view_area.h + (mouse_y - drag_start_y));
            view_area.w = new_width;
            view_area.h = new_height;
            drag_start_x = mouse_x;
            drag_start_y = mouse_y;
            cursor_manager.setPlotArea(view_area);
        }
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT && in_plot_area) {
        // Right click for cursor 2
        cursor_manager.addCursor("C2", {0, 255, 255, 255});
        cursor2.visible = true;
        cursor2.screen_x = mouse_x;
        updateCursorValue(cursor2);
        ErrorManager::info("[Cursor] Cursor 2 placed");
    } else if (event.type == SDL_KEYDOWN) {
        cursor_manager.handleKeyPress(event.key.keysym.scancode);
    }
    
    // Handle zoom and pan
    if (event.type == SDL_MOUSEWHEEL && in_plot_area) {
        double old_scale_x = scale_x;
        double old_scale_y = scale_y;
        if (event.wheel.y > 0) { // Zoom in
            scale_x *= 1.1;
            scale_y *= 1.1;
        } else { // Zoom out
            scale_x /= 1.1;
            scale_y /= 1.1;
        }
        offset_x = (offset_x * old_scale_x + (mouse_x - view_area.x) * (old_scale_x - scale_x)) / scale_x;
        offset_y = (offset_y * old_scale_y + (view_area.h - (mouse_y - view_area.y)) * (old_scale_y - scale_y)) / scale_y;
        updateCursorManager(); // Update cursor manager after zoom
    }
}

void PlotView::updateCursorValue(Cursor& cursor) {
    if (x_values.empty() || signals.empty()) return;
    SDL_Point world_coords = toWorldCoords(cursor.screen_x, 0);
    cursor.world_x = world_coords.x;

    auto it = std::lower_bound(x_values.begin(), x_values.end(), cursor.world_x);
    if (it == x_values.end()) it = x_values.end() - 1;
    size_t index = std::distance(x_values.begin(), it);

    if (index < signals[0].y_values.size()) {
        cursor.world_y = signals[0].y_values[index];
    }
}

SDL_Point PlotView::toWorldCoords(int screen_x, int screen_y) {
    // Account for space reserved for axis labels
    int label_margin_left = 70;  // Space for y-axis labels
    int label_margin_bottom = 50; // Space for x-axis labels
    
    return { static_cast<int>((screen_x - view_area.x - label_margin_left) / scale_x - offset_x),
             static_cast<int>((view_area.h - label_margin_bottom - (screen_y - view_area.y)) / scale_y - offset_y) };
}

SDL_Point PlotView::toScreenCoords(double world_x, double world_y) {
    // Account for space reserved for axis labels
    int label_margin_left = 70;  // Space for y-axis labels
    int label_margin_bottom = 50; // Space for x-axis labels
    
    SDL_Point result = { view_area.x + label_margin_left + static_cast<int>((world_x + offset_x) * scale_x),
             view_area.y + view_area.h - label_margin_bottom - static_cast<int>((world_y + offset_y) * scale_y) };

    // Debug output for first few calls
    static int debug_count = 0;
    if (debug_count < 5) {
        std::stringstream ss;
        ss << "[COORD TRANSFORM] world(" << world_x << "," << world_y << ") -> screen(" << result.x << "," << result.y << ")";
        ss << " scale_x=" << scale_x << " scale_y=" << scale_y << " offset_x=" << offset_x << " offset_y=" << offset_y;
        std::cout << ss.str() << std::endl;
        logLine(ss.str());
        debug_count++;
    }

    return result;
}

void PlotView::renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect dest = { x, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
}

void PlotView::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 22, 255);
    SDL_RenderFillRect(renderer, &view_area);
    SDL_RenderSetClipRect(renderer, &view_area);

    // Debug output
    {
        std::stringstream ss;
        ss << "[PLOT RENDER] view_area: (" << view_area.x << "," << view_area.y << "," << view_area.w << "," << view_area.h << ")";
        ss << " x_values.size=" << x_values.size() << " signals.size=" << signals.size();
        std::cout << ss.str() << std::endl;
        logLine(ss.str());
    }

    if (!x_values.empty() && !signals.empty()) {
        for (const auto& sig : signals) {
            std::cout << "[PLOT RENDER] Processing signal '" << sig.name << "' with " << sig.y_values.size() << " points" << std::endl;

            if (sig.y_values.size() < 2) continue;
            if (sig.y_values.size() != x_values.size()) {
                // Guard against mismatched lengths
                size_t n = std::min(x_values.size(), sig.y_values.size());
                SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, sig.color.a);
                for (size_t i = 0; i + 1 < n; ++i) {
                    SDL_Point p1 = toScreenCoords(x_values[i], sig.y_values[i]);
                    SDL_Point p2 = toScreenCoords(x_values[i+1], sig.y_values[i+1]);
                    SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);

                    // Debug first few points
                    if (i < 3) {
                        std::cout << "[PLOT RENDER] Point " << i << ": world(" << x_values[i] << "," << sig.y_values[i] << ") -> screen(" << p1.x << "," << p1.y << ")" << std::endl;
                    }
                }
                continue;
            }
            SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, sig.color.a);
            for (size_t i = 0; i < x_values.size() - 1; ++i) {
                SDL_Point p1 = toScreenCoords(x_values[i], sig.y_values[i]);
                SDL_Point p2 = toScreenCoords(x_values[i+1], sig.y_values[i+1]);
                SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);

                // Debug first few points
                if (i < 3) {
                    std::cout << "[PLOT RENDER] Point " << i << ": world(" << x_values[i] << "," << sig.y_values[i] << ") -> screen(" << p1.x << "," << p1.y << ")" << std::endl;
                }
            }
        }
    }

    if (cursor1.visible) {
        SDL_SetRenderDrawColor(renderer, 200, 150, 0, 255);
        SDL_RenderDrawLine(renderer, cursor1.screen_x, view_area.y, cursor1.screen_x, view_area.y + view_area.h);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << "C1: (t=" << cursor1.world_x << ", y=" << cursor1.world_y << ")";
        renderText(renderer, ss.str(), view_area.x + 5, view_area.y + 5, {200, 150, 0, 255});
    }
    if (cursor2.visible) {
        SDL_SetRenderDrawColor(renderer, 0, 150, 200, 255);
        SDL_RenderDrawLine(renderer, cursor2.screen_x, view_area.y, cursor2.screen_x, view_area.y + view_area.h);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << "C2: (t=" << cursor2.world_x << ", y=" << cursor2.world_y << ")";
        renderText(renderer, ss.str(), view_area.x + 5, view_area.y + 25, {0, 150, 200, 255});
    }

    // Draw axis labels and tick marks
    drawAxisLabels(renderer);
    
    // Render enhanced cursors
    renderCursors(renderer);
    
    // Title bar for dragging (above the plot)
    SDL_Rect title_bar = {view_area.x, view_area.y - 20, view_area.w, 18};
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(renderer, &title_bar);
    
    // Title text
    renderText(renderer, "Signal Plot (drag title, resize corner)", view_area.x + 5, view_area.y - 18, {255, 255, 255, 255});
    
    // Resize handle (bottom-right corner)
    SDL_Rect resize_handle = {view_area.x + view_area.w - resize_handle_size, 
                             view_area.y + view_area.h - resize_handle_size, 
                             resize_handle_size, resize_handle_size};
    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    SDL_RenderFillRect(renderer, &resize_handle);
    
    // Resize handle visual hint (diagonal lines)
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    for (int i = 0; i < 3; i++) {
        SDL_RenderDrawLine(renderer, 
                          resize_handle.x + 2 + i*3, resize_handle.y + resize_handle.h - 2,
                          resize_handle.x + resize_handle.w - 2, resize_handle.y + 2 + i*3);
    }
    
    SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
    SDL_RenderDrawRect(renderer, &view_area);
    SDL_RenderSetClipRect(renderer, nullptr);
}

void PlotView::drawAxisLabels(SDL_Renderer* renderer) {
    if (x_values.empty() || signals.empty()) return;
    
    // Use the same margins as coordinate transformation
    int label_margin_left = 70;
    int label_margin_bottom = 50;
    
    // Calculate value ranges
    double min_x = x_values.front(), max_x = x_values.back();
    double min_y = std::numeric_limits<double>::max(), max_y = std::numeric_limits<double>::lowest();
    for (const auto& sig : signals) {
        for (double val : sig.y_values) {
            min_y = std::min(min_y, val);
            max_y = std::max(max_y, val);
        }
    }
    if (min_y == max_y) { min_y -= 1.0; max_y += 1.0; }
    double margin_y = (max_y - min_y) * 0.05; // Reduced margin for better use of space
    min_y -= margin_y; max_y += margin_y;
    
    // Draw grid lines first (behind everything)
    SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255); // Dark grid
    int num_y_ticks = 8; // More grid lines for better precision
    int num_x_ticks = 10;
    
    // Calculate plot area boundaries
    int plot_left = view_area.x + label_margin_left;
    int plot_right = view_area.x + view_area.w;
    int plot_top = view_area.y;
    int plot_bottom = view_area.y + view_area.h - label_margin_bottom;
    
    // Vertical grid lines
    for (int i = 0; i <= num_x_ticks; ++i) {
        double x_value = min_x + (max_x - min_x) * i / num_x_ticks;
        SDL_Point screen_pos = toScreenCoords(x_value, min_y);
        if (i > 0 && i < num_x_ticks) { // Skip border lines
            SDL_RenderDrawLine(renderer, screen_pos.x, plot_top, screen_pos.x, plot_bottom);
        }
    }
    
    // Horizontal grid lines
    for (int i = 0; i <= num_y_ticks; ++i) {
        double y_value = min_y + (max_y - min_y) * i / num_y_ticks;
        SDL_Point screen_pos = toScreenCoords(min_x, y_value);
        if (i > 0 && i < num_y_ticks) { // Skip border lines
            SDL_RenderDrawLine(renderer, plot_left, screen_pos.y, plot_right, screen_pos.y);
        }
    }
    
    // Draw major axis lines (darker)
    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    SDL_RenderDrawLine(renderer, plot_left, plot_top, plot_left, plot_bottom); // Y-axis
    SDL_RenderDrawLine(renderer, plot_left, plot_bottom, plot_right, plot_bottom); // X-axis
    
    // Draw y-axis tick marks and labels with better formatting
    SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255);
    int num_y_labels = 6; // Fewer labels for clarity
    for (int i = 0; i <= num_y_labels; ++i) {
        double y_value = min_y + (max_y - min_y) * i / num_y_labels;
        SDL_Point screen_pos = toScreenCoords(min_x, y_value);
        
        // Draw tick mark
        SDL_RenderDrawLine(renderer, plot_left - 8, screen_pos.y, plot_left - 2, screen_pos.y);
        
        // Smart number formatting for y-axis
        std::stringstream ss;
        if (std::abs(y_value) < 1e-12) {
            ss << "0";
        } else if (std::abs(y_value) >= 1e6) {
            ss << std::fixed << std::setprecision(1) << y_value / 1e6 << "M";
        } else if (std::abs(y_value) >= 1e3) {
            ss << std::fixed << std::setprecision(1) << y_value / 1e3 << "k";
        } else if (std::abs(y_value) >= 1) {
            ss << std::fixed << std::setprecision(2) << y_value;
        } else if (std::abs(y_value) >= 1e-3) {
            ss << std::fixed << std::setprecision(1) << y_value * 1e3 << "m";
        } else if (std::abs(y_value) >= 1e-6) {
            ss << std::fixed << std::setprecision(1) << y_value * 1e6 << "μ";
        } else if (std::abs(y_value) >= 1e-9) {
            ss << std::fixed << std::setprecision(1) << y_value * 1e9 << "n";
        } else {
            ss << std::scientific << std::setprecision(1) << y_value;
        }
        
        renderText(renderer, ss.str(), plot_left - 60, screen_pos.y - 8, {180, 180, 180, 255});
    }
    
    // Draw x-axis tick marks and labels with better formatting
    int num_x_labels = 8; // More labels for time precision
    for (int i = 0; i <= num_x_labels; ++i) {
        double x_value = min_x + (max_x - min_x) * i / num_x_labels;
        SDL_Point screen_pos = toScreenCoords(x_value, min_y);
        
        // Draw tick mark
        SDL_RenderDrawLine(renderer, screen_pos.x, plot_bottom + 2, screen_pos.x, plot_bottom + 8);
        
        // Smart time formatting for x-axis
        std::stringstream ss;
        if (current_mode == PlotMode::TRANSIENT) {
            if (x_value >= 1.0) {
                ss << std::fixed << std::setprecision(2) << x_value << "s";
            } else if (x_value >= 1e-3) {
                ss << std::fixed << std::setprecision(1) << x_value * 1000.0 << "ms";
            } else if (x_value >= 1e-6) {
                ss << std::fixed << std::setprecision(1) << x_value * 1e6 << "μs";
            } else if (x_value >= 1e-9) {
                ss << std::fixed << std::setprecision(1) << x_value * 1e9 << "ns";
            } else {
                ss << std::scientific << std::setprecision(1) << x_value << "s";
            }
        } else if (current_mode == PlotMode::AC_MAGNITUDE) {
            if (x_value >= 1e6) {
                ss << std::fixed << std::setprecision(1) << x_value / 1e6 << "MHz";
            } else if (x_value >= 1e3) {
                ss << std::fixed << std::setprecision(1) << x_value / 1e3 << "kHz";
            } else {
                ss << std::fixed << std::setprecision(1) << x_value << "Hz";
            }
        } else {
            ss << std::fixed << std::setprecision(1) << x_value;
        }
        
        renderText(renderer, ss.str(), screen_pos.x - 25, plot_bottom + 15, {180, 180, 180, 255});
    }
    
    // Add axis titles
    std::string x_title, y_title;
    if (current_mode == PlotMode::TRANSIENT) {
        x_title = "Time";
        y_title = "Voltage (V) / Current (A)";
    } else if (current_mode == PlotMode::AC_MAGNITUDE) {
        x_title = "Frequency";
        y_title = "Magnitude";
    } else if (current_mode == PlotMode::PHASE_MAGNITUDE) {
        x_title = "Phase";
        y_title = "Magnitude";
    }
    
    // Draw signal trace labels at the top with their colors
    if (!signals.empty()) {
        int legend_x = plot_left + 10;
        int legend_y = plot_top + 10;
        int legend_spacing = 150; // Space between legend items
        
        for (size_t i = 0; i < signals.size(); ++i) {
            const auto& sig = signals[i];
            
            // Draw colored line indicator
            SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, sig.color.a);
            SDL_RenderDrawLine(renderer, legend_x, legend_y + 8, legend_x + 20, legend_y + 8);
            SDL_RenderDrawLine(renderer, legend_x, legend_y + 7, legend_x + 20, legend_y + 7); // Thicker line
            SDL_RenderDrawLine(renderer, legend_x, legend_y + 9, legend_x + 20, legend_y + 9);
            
            // Draw signal name in matching color
            renderText(renderer, sig.name, legend_x + 25, legend_y, {sig.color.r, sig.color.g, sig.color.b, 255});
            
            // Move to next position (wrap to new line if needed)
            legend_x += legend_spacing;
            if (legend_x + legend_spacing > plot_right - 50) {
                legend_x = plot_left + 10;
                legend_y += 25; // Next line
            }
        }
    }
    
    // Draw axis titles
    if (!x_title.empty()) {
        renderText(renderer, x_title, plot_left + (plot_right - plot_left)/2 - 30, plot_bottom + 40, {200, 200, 200, 255});
    }
    if (!y_title.empty()) {
        // For y-axis title, we'd need to rotate text, so just put it at the top for now
        renderText(renderer, y_title, plot_left - 60, plot_top - 10, {200, 200, 200, 255});
    }
}

void PlotView::updateCursorManager() {
    if (!x_values.empty() && !signals.empty()) {
        // Calculate current axis ranges
        double min_x = *std::min_element(x_values.begin(), x_values.end());
        double max_x = *std::max_element(x_values.begin(), x_values.end());
        
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        for (const auto& sig : signals) {
            if (!sig.y_values.empty()) {
                auto [sig_min, sig_max] = std::minmax_element(sig.y_values.begin(), sig.y_values.end());
                min_y = std::min(min_y, *sig_min);
                max_y = std::max(max_y, *sig_max);
            }
        }
        
        // Update cursor manager with current data and ranges
        cursor_manager.setAxisRanges(min_x, max_x, min_y, max_y);
        
        // Prepare signal data for cursor manager
        std::map<std::string, std::vector<double>> signal_map;
        for (const auto& sig : signals) {
            signal_map[sig.name] = sig.y_values;
        }
        cursor_manager.setData(x_values, signal_map);
        cursor_manager.updateScreenPositions();
    }
}

void PlotView::renderCursors(SDL_Renderer* renderer) {
    auto enabled_cursors = cursor_manager.getEnabledCursors();
    
    for (size_t i = 0; i < enabled_cursors.size(); ++i) {
        auto* cursor = enabled_cursors[i];
        if (!cursor) continue;
        
        // Draw cursor line
        SDL_Color color = cursor->getColor();
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        
        int screen_x = cursor->getScreenX();
        SDL_RenderDrawLine(renderer, screen_x, view_area.y, screen_x, view_area.y + view_area.h);
        
        // Draw cursor info
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) 
           << cursor->getLabel() << ": (t=" << cursor->getXPosition() 
           << ", y=" << cursor->getYPosition() << ")";
        
        renderText(renderer, ss.str(), view_area.x + 5, view_area.y + 25 + i * 20, color);
    }
    
    // Show measurement info if in double cursor mode
    if (cursor_manager.isDoubleCursorMode() && cursor_manager.getCursorCount() >= 2) {
        auto measurement = cursor_manager.getMeasurement();
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) 
           << "ΔX=" << measurement.delta_x 
           << ", ΔY=" << measurement.delta_y
           << ", f=" << measurement.frequency << "Hz";
        
        renderText(renderer, ss.str(), view_area.x + 5, view_area.y + view_area.h - 30, {255, 255, 255, 255});
    }
}

// ComponentEditDialog implementation moved outside comment block
/*
ComponentEditDialog::ComponentEditDialog(int x, int y, int w, int h, TTF_Font* font, std::function<void()> on_apply, std::function<void()> on_cancel)
    : font(font), on_apply_callback(std::move(on_apply)), on_cancel_callback(std::move(on_cancel)) {
    dialog_rect = {x, y, w, h};
    // Create input box for value editing
    value_input = new InputBox(x + 10, y + 60, w - 20, 30, font);
}

void ComponentEditDialog::setTargetElement(Element* element) {
    target_element = element;
    param_inputs.clear();
    param_labels.clear();
    
    if (element) {
        std::string type = element->getType();
        
        if (type == "PulseVoltageSource") {
            auto* pulse = static_cast<PulseVoltageSource*>(element);
            
            // Resize dialog for multiple inputs
            dialog_rect.h = 400;
            
            // Create inputs for all pulse parameters
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 50, 150, 25, font, std::to_string(pulse->getV1())));
            param_labels.push_back("V1 (Initial V):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 85, 150, 25, font, std::to_string(pulse->getV2())));
            param_labels.push_back("V2 (Pulse V):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 120, 150, 25, font, std::to_string(pulse->getTd())));
            param_labels.push_back("Delay (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 155, 150, 25, font, std::to_string(pulse->getTr())));
            param_labels.push_back("Rise Time (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 190, 150, 25, font, std::to_string(pulse->getTf())));
            param_labels.push_back("Fall Time (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 225, 150, 25, font, std::to_string(pulse->getPw())));
            param_labels.push_back("Pulse Width (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 260, 150, 25, font, std::to_string(pulse->getPer())));
            param_labels.push_back("Period (s):");
            
        } else if (type == "PulseCurrentSource") {
            auto* pulse = static_cast<PulseCurrentSource*>(element);
            
            dialog_rect.h = 400;
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 50, 150, 25, font, std::to_string(pulse->getI1())));
            param_labels.push_back("I1 (Initial A):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 85, 150, 25, font, std::to_string(pulse->getI2())));
            param_labels.push_back("I2 (Pulse A):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 120, 150, 25, font, std::to_string(pulse->getTd())));
            param_labels.push_back("Delay (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 155, 150, 25, font, std::to_string(pulse->getTr())));
            param_labels.push_back("Rise Time (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 190, 150, 25, font, std::to_string(pulse->getTf())));
            param_labels.push_back("Fall Time (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 225, 150, 25, font, std::to_string(pulse->getPw())));
            param_labels.push_back("Pulse Width (s):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 260, 150, 25, font, std::to_string(pulse->getPer())));
            param_labels.push_back("Period (s):");
            
        } else if (type == "SinusoidalVoltageSource") {
            auto* sin = static_cast<SinusoidalVoltageSource*>(element);
            
            // Increase dialog size for better visibility
            dialog_rect.w = 400;  // Make wider
            dialog_rect.h = 300;  // Make taller
            dialog_rect.x = dialog_rect.x - 50;  // Adjust position for wider dialog
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 150, dialog_rect.y + 50, 200, 30, font, std::to_string(sin->getFrequency())));
            param_labels.push_back("Frequency (Hz):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 150, dialog_rect.y + 100, 200, 30, font, std::to_string(sin->getAmplitude())));
            param_labels.push_back("Amplitude (V):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 150, dialog_rect.y + 150, 200, 30, font, std::to_string(sin->getDCOffset())));
            param_labels.push_back("DC Offset (V):");
            
        } else if (type == "VoltageControlledVoltageSource") {
            auto* vcvs = static_cast<VoltageControlledVoltageSource*>(element);
            
            dialog_rect.h = 300;
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 50, 150, 25, font, vcvs->getControlNode1Id()));
            param_labels.push_back("Control Node 1:");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 85, 150, 25, font, vcvs->getControlNode2Id()));
            param_labels.push_back("Control Node 2:");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 120, 150, 25, font, std::to_string(vcvs->getGain())));
            param_labels.push_back("Gain (V/V):");
            
        } else if (type == "VoltageControlledCurrentSource") {
            auto* vccs = static_cast<VoltageControlledCurrentSource*>(element);
            
            dialog_rect.h = 300;
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 50, 150, 25, font, vccs->getControlNode1Id()));
            param_labels.push_back("Control Node 1:");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 85, 150, 25, font, vccs->getControlNode2Id()));
            param_labels.push_back("Control Node 2:");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 120, 150, 25, font, std::to_string(vccs->getTransconductance())));
            param_labels.push_back("Transconductance (A/V):");
            
        } else if (type == "CurrentControlledVoltageSource") {
            auto* ccvs = static_cast<CurrentControlledVoltageSource*>(element);
            
            dialog_rect.h = 250;
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 50, 150, 25, font, ccvs->getControllingBranchName()));
            param_labels.push_back("Controlling Branch:");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 85, 150, 25, font, std::to_string(ccvs->getTransresistance())));
            param_labels.push_back("Transresistance (V/A):");
            
        } else if (type == "CurrentControlledCurrentSource") {
            auto* cccs = static_cast<CurrentControlledCurrentSource*>(element);
            
            dialog_rect.h = 250;
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 50, 150, 25, font, cccs->getControllingBranchName()));
            param_labels.push_back("Controlling Branch:");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 85, 150, 25, font, std::to_string(cccs->getGain())));
            param_labels.push_back("Current Gain (A/A):");
            
        } else {
            // Generic single-value input for other components
            dialog_rect.h = 150;
            current_value = formatValueWithPrefix(element->getValue());
            if (value_input) {
                value_input->setText(current_value);
            }
        }
    }
}

void ComponentEditDialog::show() {
    is_visible = true;
}

void ComponentEditDialog::hide() {
    is_visible = false;
    target_element = nullptr;
}

void ComponentEditDialog::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;
    
    // Handle value input for simple components
    if (value_input) {
        value_input->handleEvent(event);
    }
    
    // Handle parameter inputs for complex components
    for (auto& input : param_inputs) {
        input->handleEvent(event);
    }
    
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        
        // Check Apply button
        SDL_Rect apply_btn = {dialog_rect.x + 10, dialog_rect.y + dialog_rect.h - 40, 80, 30};
        if (mx >= apply_btn.x && mx <= apply_btn.x + apply_btn.w && my >= apply_btn.y && my <= apply_btn.y + apply_btn.h) {
            if (target_element) {
                try {
                    if (!param_inputs.empty()) {
                        // Handle complex components with multiple parameters
                        std::string type = target_element->getType();
                        
                        if (type == "PulseVoltageSource") {
                            auto* pulse = static_cast<PulseVoltageSource*>(target_element);
                            double v1 = parseValueWithPrefix(param_inputs[0]->getText());
                            double v2 = parseValueWithPrefix(param_inputs[1]->getText());
                            double td = parseValueWithPrefix(param_inputs[2]->getText());
                            double tr = parseValueWithPrefix(param_inputs[3]->getText());
                            double tf = parseValueWithPrefix(param_inputs[4]->getText());
                            double pw = parseValueWithPrefix(param_inputs[5]->getText());
                            double per = parseValueWithPrefix(param_inputs[6]->getText());
                            
                            pulse->setV1(v1);
                            pulse->setV2(v2);
                            pulse->setTd(td);
                            pulse->setTr(tr);
                            pulse->setTf(tf);
                            pulse->setPw(pw);
                            pulse->setPer(per);
                            
                            std::cout << "[PULSE EDIT] Updated " << pulse->getName() << ": V1=" << v1 << "V, V2=" << v2 << "V, td=" << td << "s, tr=" << tr << "s, tf=" << tf << "s, pw=" << pw << "s, per=" << per << "s" << std::endl;
                            ErrorManager::info("[PULSE] Parameters updated successfully");
                            
                        } else if (type == "PulseCurrentSource") {
                            auto* pulse = static_cast<PulseCurrentSource*>(target_element);
                            pulse->setI1(parseValueWithPrefix(param_inputs[0]->getText()));
                            pulse->setI2(parseValueWithPrefix(param_inputs[1]->getText()));
                            pulse->setTd(parseValueWithPrefix(param_inputs[2]->getText()));
                            pulse->setTr(parseValueWithPrefix(param_inputs[3]->getText()));
                            pulse->setTf(parseValueWithPrefix(param_inputs[4]->getText()));
                            pulse->setPw(parseValueWithPrefix(param_inputs[5]->getText()));
                            pulse->setPer(parseValueWithPrefix(param_inputs[6]->getText()));
                            std::cout << "Updated PulseCurrentSource parameters" << std::endl;
                            
                        } else if (type == "SinusoidalVoltageSource") {
                            auto* sin = static_cast<SinusoidalVoltageSource*>(target_element);
                            double frequency = parseValueWithPrefix(param_inputs[0]->getText());
                            double amplitude = parseValueWithPrefix(param_inputs[1]->getText());
                            double offset = parseValueWithPrefix(param_inputs[2]->getText());
                            
                            sin->setFrequency(frequency);
                            sin->setAmplitude(amplitude);
                            sin->setDCOffset(offset);
                            
                            std::cout << "[SIN EDIT] Updated " << sin->getName() << ": Frequency=" << frequency << "Hz, Amplitude=" << amplitude << "V, DC Offset=" << offset << "V" << std::endl;
                            ErrorManager::info("[SIN] Parameters updated successfully");
                            
                        } else if (type == "VoltageControlledVoltageSource") {
                            auto* vcvs = static_cast<VoltageControlledVoltageSource*>(target_element);
                            std::string ctrl_node1 = param_inputs[0]->getText();
                            std::string ctrl_node2 = param_inputs[1]->getText();
                            double gain = parseValueWithPrefix(param_inputs[2]->getText());
                            
                            // Note: We can't change control nodes after creation, only gain
                            vcvs->setValue(gain); // This should update the gain
                            
                            std::cout << "[VCVS EDIT] Updated " << vcvs->getName() << ": Gain=" << gain << "V/V (Control: " << ctrl_node1 << "-" << ctrl_node2 << ")" << std::endl;
                            ErrorManager::info("[VCVS] Gain updated successfully");
                            
                        } else if (type == "VoltageControlledCurrentSource") {
                            auto* vccs = static_cast<VoltageControlledCurrentSource*>(target_element);
                            std::string ctrl_node1 = param_inputs[0]->getText();
                            std::string ctrl_node2 = param_inputs[1]->getText();
                            double transconductance = parseValueWithPrefix(param_inputs[2]->getText());
                            
                            vccs->setValue(transconductance); // This should update the transconductance
                            
                            std::cout << "[VCCS EDIT] Updated " << vccs->getName() << ": Transconductance=" << transconductance << "A/V (Control: " << ctrl_node1 << "-" << ctrl_node2 << ")" << std::endl;
                            ErrorManager::info("[VCCS] Transconductance updated successfully");
                            
                        } else if (type == "CurrentControlledVoltageSource") {
                            auto* ccvs = static_cast<CurrentControlledVoltageSource*>(target_element);
                            std::string ctrl_branch = param_inputs[0]->getText();
                            double transresistance = parseValueWithPrefix(param_inputs[1]->getText());
                            
                            ccvs->setValue(transresistance); // This should update the transresistance
                            
                            std::cout << "[CCVS EDIT] Updated " << ccvs->getName() << ": Transresistance=" << transresistance << "V/A (Control: " << ctrl_branch << ")" << std::endl;
                            ErrorManager::info("[CCVS] Transresistance updated successfully");
                            
                        } else if (type == "CurrentControlledCurrentSource") {
                            auto* cccs = static_cast<CurrentControlledCurrentSource*>(target_element);
                            std::string ctrl_branch = param_inputs[0]->getText();
                            double current_gain = parseValueWithPrefix(param_inputs[1]->getText());
                            
                            cccs->setValue(current_gain); // This should update the current gain
                            
                            std::cout << "[CCCS EDIT] Updated " << cccs->getName() << ": Current Gain=" << current_gain << "A/A (Control: " << ctrl_branch << ")" << std::endl;
                            ErrorManager::info("[CCCS] Current gain updated successfully");
                        }
                    } else if (value_input) {
                        // Handle simple components with single value
                        double new_value = parseValueWithPrefix(value_input->getText());
                        target_element->setValue(new_value);
                        std::cout << "Updated " << target_element->getName() << " value to " << new_value << " (from " << value_input->getText() << ")" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << "Invalid value: " << e.what() << std::endl;
                    ErrorManager::displayError("Invalid parameter values: " + std::string(e.what()));
                }
            }
            if (on_apply_callback) on_apply_callback();
            hide();
            return;
        }
        
        // Check Cancel button
        SDL_Rect cancel_btn = {dialog_rect.x + 100, dialog_rect.y + dialog_rect.h - 40, 80, 30};
        if (mx >= cancel_btn.x && mx <= cancel_btn.x + cancel_btn.w && my >= cancel_btn.y && my <= cancel_btn.y + cancel_btn.h) {
            if (on_cancel_callback) on_cancel_callback();
            hide();
            return;
        }
    }
    
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        hide();
    }
}

void ComponentEditDialog::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    
    // Draw dialog background
    SDL_SetRenderDrawColor(renderer, 250, 250, 255, 240);
    SDL_RenderFillRect(renderer, &dialog_rect);
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &dialog_rect);
    
    // Draw title
    if (font && target_element) {
        std::string title = "Edit " + target_element->getName() + " (" + target_element->getType() + ")";
        SDL_Surface* surface = TTF_RenderText_Blended(font, title.c_str(), {20, 20, 20, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = {dialog_rect.x + 10, dialog_rect.y + 10, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
    
    // Render parameter inputs for complex components
    if (!param_inputs.empty()) {
        for (size_t i = 0; i < param_inputs.size() && i < param_labels.size(); ++i) {
            // Draw parameter label
            if (font) {
                SDL_Surface* surface = TTF_RenderText_Blended(font, param_labels[i].c_str(), {20, 20, 20, 255});
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                    SDL_Rect dest = {dialog_rect.x + 10, dialog_rect.y + 50 + static_cast<int>(i * 35), surface->w, surface->h};
                    SDL_RenderCopy(renderer, texture, nullptr, &dest);
                    SDL_DestroyTexture(texture);
                    SDL_FreeSurface(surface);
                }
            }
            
            // Render input box
            param_inputs[i]->render(renderer);
        }
    } else {
        // Draw value label for simple components
        if (font) {
            SDL_Surface* surface = TTF_RenderText_Blended(font, "Value:", {20, 20, 20, 255});
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect dest = {dialog_rect.x + 10, dialog_rect.y + 40, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
            }
        }
        
        // Render input box
        if (value_input) {
            value_input->render(renderer);
        }
    }
    
    // Draw Apply button
    SDL_Rect apply_btn = {dialog_rect.x + 10, dialog_rect.y + dialog_rect.h - 40, 80, 30};
    SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
    SDL_RenderFillRect(renderer, &apply_btn);
    SDL_SetRenderDrawColor(renderer, 80, 160, 80, 255);
    SDL_RenderDrawRect(renderer, &apply_btn);
    
    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Apply", {255, 255, 255, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = {apply_btn.x + 20, apply_btn.y + 6, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
    
    // Draw Cancel button
    SDL_Rect cancel_btn = {dialog_rect.x + 100, dialog_rect.y + dialog_rect.h - 40, 80, 30};
    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
    SDL_RenderFillRect(renderer, &cancel_btn);
    SDL_SetRenderDrawColor(renderer, 160, 80, 80, 255);
    SDL_RenderDrawRect(renderer, &cancel_btn);
    
    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Cancel", {255, 255, 255, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = {cancel_btn.x + 15, cancel_btn.y + 6, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
}
*/

// --- ProbePanel Implementation ---
ProbePanel::ProbePanel(int x, int y, int w, int h, TTF_Font* font, std::function<void(const std::set<std::string>&)> on_apply)
    : font(font), on_apply(std::move(on_apply)) {
    panel_rect = {x, y, w, h};
}

void ProbePanel::setSignalsFromResults(const std::map<std::string, std::vector<double>>& results) {
    available_signals.clear();
    for (const auto& p : results) available_signals.push_back(p.first);
    // Preserve any previously selected names that still exist
    std::set<std::string> still_valid;
    for (const auto& name : selected) if (results.count(name)) still_valid.insert(name);
    selected.swap(still_valid);
    
    // Reset scroll when signals change
    scroll_offset = 0;
    updateScrollLimits();
}

void ProbePanel::addSignal(const std::string& signal_name) {
    // Add the signal to the selected set
    selected.insert(signal_name);
    
    // Make sure the signal is in available_signals if it's not already
    if (std::find(available_signals.begin(), available_signals.end(), signal_name) == available_signals.end()) {
        available_signals.push_back(signal_name);
    }
    
    // Trigger the apply callback to update the display
    if (on_apply) {
        on_apply(selected);
    }
}

void ProbePanel::autoSelectVoltageSignals() {
    // Clear existing selections
    selected.clear();
    
    // Auto-select the first few voltage signals
    int voltage_count = 0;
    for (const auto& signal : available_signals) {
        if (signal.find("V(") != std::string::npos && voltage_count < 3) {
            selected.insert(signal);
            voltage_count++;
        }
    }
    
    // If we found voltage signals, trigger the apply callback
    if (!selected.empty() && on_apply) {
        on_apply(selected);
    }
}

void ProbePanel::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        if (mx < panel_rect.x || mx > panel_rect.x + panel_rect.w || my < panel_rect.y || my > panel_rect.y + panel_rect.h) return;

        // Check Apply first so clicks near bottom don't toggle signals accidentally
        SDL_Rect apply_btn = {panel_rect.x + 10, panel_rect.y + panel_rect.h - 35, 80, 25};
        if (mx >= apply_btn.x && mx <= apply_btn.x + apply_btn.w && my >= apply_btn.y && my <= apply_btn.y + apply_btn.h) {
            if (on_apply) on_apply(selected);
            return;
        }

        // Toggle selection by clicking list items (with scroll offset)
        int list_top = panel_rect.y + 40;
        int list_bottom = apply_btn.y - 10;
        if (my >= list_top && my <= list_bottom) {
            int line_h = item_height;
            int visible_idx = (my - list_top) / line_h;
            int actual_idx = visible_idx + scroll_offset;
            
            if (actual_idx >= 0 && actual_idx < (int)available_signals.size()) {
                const std::string& name = available_signals[actual_idx];
                if (selected.count(name)) selected.erase(name);
                else selected.insert(name);
            }
        }
    }
    
    // Handle mouse wheel scrolling
    if (event.type == SDL_MOUSEWHEEL) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        
        // Check if mouse is over the panel
        if (mx >= panel_rect.x && mx <= panel_rect.x + panel_rect.w && 
            my >= panel_rect.y && my <= panel_rect.y + panel_rect.h) {
            
            if (event.wheel.y > 0) {
                // Scroll up
                scroll_offset = std::max(0, scroll_offset - 1);
            } else if (event.wheel.y < 0) {
                // Scroll down
                int max_scroll = std::max(0, (int)available_signals.size() - max_visible_items);
                scroll_offset = std::min(max_scroll, scroll_offset + 1);
            }
        }
    }
}

void ProbePanel::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    SDL_SetRenderDrawColor(renderer, 235, 235, 240, 240);
    SDL_RenderFillRect(renderer, &panel_rect);
    SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
    SDL_RenderDrawRect(renderer, &panel_rect);

    // Title
    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Probe Signals (click to toggle)", {20,20,20,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 10, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // List signals (with scrolling)
    int y = panel_rect.y + 40;
    int visible_count = 0;
    int start_idx = scroll_offset;
    int end_idx = std::min((int)available_signals.size(), start_idx + max_visible_items);
    
    for (int i = start_idx; i < end_idx && visible_count < max_visible_items; i++) {
        const std::string& name = available_signals[i];
        SDL_Color color = selected.count(name) ? SDL_Color{0,130,0,255} : SDL_Color{130,0,0,255};
        if (font) {
            SDL_Surface* surface = TTF_RenderText_Blended(font, name.c_str(), color);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect dest = { panel_rect.x + 10, y, surface->w, surface->h };
                SDL_RenderCopy(renderer, texture, nullptr, &dest);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
            }
        }
        y += item_height;
        visible_count++;
    }
    
    // Draw scroll indicator if there are more items
    if (available_signals.size() > max_visible_items) {
        int scroll_bar_width = 8;
        int scroll_bar_x = panel_rect.x + panel_rect.w - scroll_bar_width - 5;
        int scroll_bar_height = (max_visible_items * item_height) - 10;
        int scroll_bar_y = panel_rect.y + 45;
        
        // Background of scroll bar
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_Rect scroll_bg = {scroll_bar_x, scroll_bar_y, scroll_bar_width, scroll_bar_height};
        SDL_RenderFillRect(renderer, &scroll_bg);
        
        // Scroll thumb
        double scroll_ratio = (double)scroll_offset / (double)(available_signals.size() - max_visible_items);
        int thumb_height = std::max(10, (int)(scroll_bar_height * max_visible_items / available_signals.size()));
        int thumb_y = scroll_bar_y + (int)((scroll_bar_height - thumb_height) * scroll_ratio);
        
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect scroll_thumb = {scroll_bar_x, thumb_y, scroll_bar_width, thumb_height};
        SDL_RenderFillRect(renderer, &scroll_thumb);
    }

    // Apply button
    SDL_Rect apply_btn = {panel_rect.x + 10, panel_rect.y + panel_rect.h - 35, 80, 25};
    SDL_SetRenderDrawColor(renderer, 80, 80, 200, 255);
    SDL_RenderFillRect(renderer, &apply_btn);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Apply", {255,255,255,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { apply_btn.x + 15, apply_btn.y + 4, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
}

void ProbePanel::updateScrollLimits() {
    int max_scroll = std::max(0, (int)available_signals.size() - max_visible_items);
    scroll_offset = std::min(scroll_offset, max_scroll);
    scroll_offset = std::max(0, scroll_offset);
}



// --- GuiApplication Implementation ---
GuiApplication::GuiApplication() {
    initialize();
}
GuiApplication::~GuiApplication() {
    cleanup();
}

SDL_Texture* GuiApplication::loadTexture(const std::string& path) {
    SDL_Texture* tex = IMG_LoadTexture(renderer, path.c_str());
    if (!tex) ErrorManager::displayError("Failed to load texture " + path + ": " + IMG_GetError());
    return tex;
}

void GuiApplication::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) throw std::runtime_error("SDL init failed");
    initLogger();
    if (TTF_Init() == -1) throw std::runtime_error("TTF init failed");
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) throw std::runtime_error("IMG init failed");

    // Get display dimensions for fullscreen
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);
    int screen_width = display_mode.w;
    int screen_height = display_mode.h;
    
    window = SDL_CreateWindow("Circuit Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    font = TTF_OpenFont("C:/Windows/Fonts/Arial.ttf", 16);

    if (!window || !renderer || !font) throw std::runtime_error("GUI creation failed");

    component_textures["resistor.png"] = loadTexture("assets/resistor.png");
    component_textures["capacitor.png"] = loadTexture("assets/capacitor.png");
    component_textures["inductor.png"] = loadTexture("assets/inductor.png");
    component_textures["diode.png"] = loadTexture("assets/diode.png");
    component_textures["gnd.png"] = loadTexture("assets/gnd.png");
    component_textures["dc_v_source.png"] = loadTexture("assets/dc_v_source.png");
    component_textures["ac_v_source.png"] = loadTexture("assets/ac_v_source.png");
    component_textures["dc_c_source.png"] = loadTexture("assets/dc_c_source.png");
    component_textures["ac_c_source.png"] = loadTexture("assets/ac_c_source.png");
    component_textures["dep_v_source.png"] = loadTexture("assets/dep_v_source.png");
    component_textures["dep_c_source.png"] = loadTexture("assets/dep_c_source.png");

    // Debug: Print all loaded texture sizes
    std::cout << "=== Loaded Texture Sizes ===" << std::endl;
    for (const auto& [name, texture] : component_textures) {
        if (texture) {
            int w, h;
            SDL_QueryTexture(texture, NULL, NULL, &w, &h);
            std::cout << name << ": " << w << "x" << h << " pixels" << std::endl;
        }
    }
    std::cout << "===========================" << std::endl;

    circuit = std::make_unique<Circuit>();

    // Calculate layout based on screen dimensions
    int menu_height = 30; // Reserve space for top menu bar
    int sidebar_width = 0; // No sidebar - moving to menu bar
    int schematic_height = (screen_height - menu_height) * 0.40; // 40% for schematic (reduced)
    int plot_height = (screen_height - menu_height) * 0.60; // 60% for plot (increased significantly)
    
    auto schematic = std::make_unique<SchematicView>(sidebar_width, menu_height, screen_width - sidebar_width, schematic_height, *circuit, component_textures, font);
    schematic_view = schematic.get();
    ui_elements.push_back(std::move(schematic));

    ui_elements.push_back(std::make_unique<Oscilloscope>(sidebar_width, menu_height + schematic_height, screen_width - sidebar_width, plot_height, font));

    // Component selector for sources and components
    auto selector = std::make_unique<ComponentSelector>(screen_width/6, screen_height/4, 200, 400, font, [this](const std::string& type){ this->selectComponentToPlace(type); });
    component_selector = selector.get();
    ui_elements.push_back(std::move(selector));

    auto settings = std::make_unique<SimulationSettingsPanel>(screen_width/4, screen_height/6, 400, 320, font);
    settings_panel = settings.get();
    // Set up callback for ENTER key handling
    settings_panel->setOnEnterCallback([this](SimulationSettingsPanel::TabType tab) {
        if (tab == SimulationSettingsPanel::TabType::AC) {
            std::cout << "[ENTER] Running AC analysis from settings panel" << std::endl;
            onRunACAnalysisClicked();
        } else if (tab == SimulationSettingsPanel::TabType::TRANSIENT) {
            std::cout << "[ENTER] Running TRAN analysis from settings panel" << std::endl;
            runTransientAnalysis(settings_panel->getTranTStart(),
                               settings_panel->getTranTStep(),
                               settings_panel->getTranTStop());
        } else if (tab == SimulationSettingsPanel::TabType::PHASE) {
            std::cout << "[ENTER] Running PHASE analysis from settings panel" << std::endl;
            onRunPhaseAnalysisClicked();
        }
    });
    ui_elements.push_back(std::move(settings));

    // Probe panel
    auto probe = std::make_unique<ProbePanel>(screen_width/3, screen_height/6, 450, 400, font, [this](const std::set<std::string>& selected){
        selected_signals = selected;
        {
            std::stringstream ss; ss << "[Probe] Apply clicked. Selected signals:";
            for (const auto& s : selected_signals) ss << " " << s;
            std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());
        }
        for (auto& el : ui_elements) {
            if (auto* scope = dynamic_cast<Oscilloscope*>(el.get())) {
                if (!latest_time_points.empty() && !latest_tran_results.empty()) {
                    std::cout << "[PROBE] Updating oscilloscope with " << selected_signals.size() << " selected signals" << std::endl;

                    // Clear existing data and set time points
                    scope->clearData();
                    scope->setTimePoints(latest_time_points);

                    // If no signals are selected, show all signals
                    if (selected_signals.empty()) {
                        std::cout << "[PROBE] No signals selected, showing all signals" << std::endl;
                        for (const auto& pair : latest_tran_results) {
                            scope->addSignal(pair.first, pair.second);
                        }
                    } else {
                        // Show only selected signals
                        bool any_selected = false;
                        for (const auto& signal_name : selected_signals) {
                            auto it = latest_tran_results.find(signal_name);
                            if (it != latest_tran_results.end()) {
                                scope->addSignal(it->first, it->second);
                                any_selected = true;
                                std::cout << "[PROBE] Added selected signal: " << signal_name << std::endl;
                            } else {
                                std::cout << "[PROBE] Selected signal not found: " << signal_name << std::endl;
                            }
                        }

                        // If none of the selected signals were found, fall back to all signals
                        if (!any_selected) {
                            std::cout << "[PROBE] No selected signals found, showing all signals as fallback" << std::endl;
                            for (const auto& pair : latest_tran_results) {
                                scope->addSignal(pair.first, pair.second);
                            }
                        }
                    }

                    // Auto-scale to fit the data
                    scope->autoScale();
                } else {
                    std::cout << "[PROBE] WARNING: No transient data available for oscilloscope" << std::endl;
                }
                break;
            }
        }
        // Hide panel after applying to reveal the plot
        if (probe_panel && probe_panel->isVisible()) probe_panel->toggleVisibility();
    });
    probe_panel = probe.get();
    ui_elements.push_back(std::move(probe));
    
    // Component Edit Dialog
    auto edit_dlg = std::make_unique<ComponentEditDialog>(screen_width/2 - 150, screen_height/2 - 75, 300, 150, font, 
        [this]() { 
            std::cout << "Component edit applied" << std::endl; 
        },
        [this]() { 
            std::cout << "Component edit cancelled" << std::endl; 
        });
    edit_dialog = edit_dlg.get();
    ui_elements.push_back(std::move(edit_dlg));
    
    // Transient Analysis Dialog
    auto tran_dlg = std::make_unique<TransientAnalysisDialog>(screen_width/2 - 150, screen_height/2 - 100, 300, 220, font,
        [this](double tstart, double tstep, double tstop) {
            // Run transient analysis with user parameters
            this->runTransientAnalysis(tstart, tstep, tstop);
        },
        [this]() {
            std::cout << "Transient analysis cancelled" << std::endl;
        });
    tran_dialog = tran_dlg.get();
    ui_elements.push_back(std::move(tran_dlg));
    
    // Add top menu bar buttons
    int button_width = 100;
    int button_height = 25;
    int button_y = 2;
    int button_spacing = 5;
    int current_x = 5;
    
    // File menu
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, button_width, button_height, "Save Project", font, [this]() { this->onSaveProjectClicked(); }));
    current_x += button_width + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, button_width, button_height, "Load Project", font, [this]() { this->onLoadProjectClicked(); }));
    current_x += button_width + button_spacing;
    
    // Edit menu
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, button_width, button_height, "Undo", font, [this]() {
        if (!undo_stack.empty()) {
            try {
                const std::string path = "__redo_snapshot.json";
                ProjectSerializer::save(*circuit, path);
                std::ifstream ifs(path, std::ios::in | std::ios::binary);
                std::stringstream buffer; buffer << ifs.rdbuf();
                redo_stack.push_back(buffer.str());
            } catch (...) {
                std::cout << "Warning: Failed to save redo snapshot" << std::endl;
            }

            try {
                auto snap = undo_stack.back();
                undo_stack.pop_back();
            applySnapshot(snap);
                std::cout << "Undo performed successfully" << std::endl;
            } catch (...) {
                std::cout << "Error: Failed to apply undo snapshot" << std::endl;
            }
        } else {
            std::cout << "Nothing to undo" << std::endl;
        }
    }));
    current_x += button_width + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, button_width, button_height, "Redo", font, [this]() {
        if (!redo_stack.empty()) {
            // Save current state to undo stack (don't clear redo stack yet)
            try {
                const std::string path = "__undo_snapshot.json";
                ProjectSerializer::save(*circuit, path);
                std::ifstream ifs(path, std::ios::in | std::ios::binary);
                std::stringstream buffer; buffer << ifs.rdbuf();
                undo_stack.push_back(buffer.str());
            } catch (...) {
                std::cout << "Warning: Failed to save current state for undo" << std::endl;
            }

            try {
                // Apply the redo snapshot and remove it from redo stack
                auto snap = redo_stack.back();
                redo_stack.pop_back();
            applySnapshot(snap);
                std::cout << "Redo performed successfully" << std::endl;
            } catch (...) {
                std::cout << "Error: Failed to apply redo snapshot" << std::endl;
            }
        } else {
            std::cout << "Nothing to redo" << std::endl;
        }
    }));
    current_x += button_width + button_spacing;
    
    // Components menu - make buttons smaller to fit more
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 70, button_height, "Resistor", font, [this]() { this->selectComponentToPlace("Resistor"); }));
    current_x += 70 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 70, button_height, "Capacitor", font, [this]() { this->selectComponentToPlace("Capacitor"); }));
    current_x += 70 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 70, button_height, "Inductor", font, [this]() { this->selectComponentToPlace("Inductor"); }));
    current_x += 70 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 70, button_height, "Ground", font, [this]() { this->selectComponentToPlace("Ground"); }));
    current_x += 70 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 70, button_height, "Sources", font, [this]() { this->onToggleComponentSelector(); }));
    current_x += 70 + button_spacing;
    
    // Analysis menu
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "TRAN", font, [this]() { this->onShowTransientDialog(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "AC", font, [this]() { this->onRunACAnalysisClicked(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "DC", font, [this]() { this->onRunDCSweepClicked(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Phase", font, [this]() { this->onRunPhaseAnalysisClicked(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Probe", font, [this]() { this->onToggleProbePanel(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Math", font, [this]() { this->onShowSignalMath(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Settings", font, [this]() { this->onToggleSettingsPanel(); }));
    current_x += 60 + button_spacing;
    
    // Probe mode buttons - smaller to save space
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 70, button_height, "Probe Mode", font, [this]() { this->toggleProbeMode(); }));
    current_x += 70 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Voltage", font, [this]() { this->setProbeType(ProbeType::VOLTAGE); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Current", font, [this]() { this->setProbeType(ProbeType::CURRENT); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 70, button_height, "Clear Probe", font, [this]() { this->clearAllProbes(); }));
    current_x += 70 + button_spacing;
    
    // Tools menu
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Wire", font, [this]() { this->toggleWireMode(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Reset", font, [this]() {
        pushUndoSnapshot();
        circuit->clear();
        if (schematic_view) { schematic_view->rebuildWiresFromCircuit(); }
    }));
    current_x += 60 + button_spacing;
    
    // Exit button at top right
    int exit_button_x = screen_width - 60 - 5; // 60px width + 5px margin from right edge
    ui_elements.push_back(std::make_unique<ActionButton>(exit_button_x, button_y, 60, button_height, "✕", font, [this]() { this->onQuitClicked(); }));


}

// --- ComponentEditDialog Implementation ---
ComponentEditDialog::ComponentEditDialog(int x, int y, int w, int h, TTF_Font* font, std::function<void()> on_apply, std::function<void()> on_cancel)
    : font(font), on_apply_callback(std::move(on_apply)), on_cancel_callback(std::move(on_cancel)) {
    dialog_rect = {x, y, w, h};
    // Create input box for value editing
    value_input = new InputBox(x + 10, y + 60, w - 20, 30, font);
}

void ComponentEditDialog::setTargetElement(Element* element) {
    target_element = element;
    param_inputs.clear();
    param_labels.clear();

    if (element) {
        std::string type = element->getType();

        // Handle different element types
        if (type == "Resistor") {
            param_labels.push_back("Resistance (Ω):");
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 10, dialog_rect.y + 100, dialog_rect.w - 20, 30, font));
            param_inputs.back()->setText(std::to_string(element->getValue()));
        } else if (type == "Capacitor") {
            param_labels.push_back("Capacitance (F):");
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 10, dialog_rect.y + 100, dialog_rect.w - 20, 30, font));
            param_inputs.back()->setText(std::to_string(element->getValue()));
        } else if (type == "Inductor") {
            param_labels.push_back("Inductance (H):");
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 10, dialog_rect.y + 100, dialog_rect.w - 20, 30, font));
            param_inputs.back()->setText(std::to_string(element->getValue()));
        } else if (type == "IndependentVoltageSource") {
            param_labels.push_back("Voltage (V):");
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 10, dialog_rect.y + 100, dialog_rect.w - 20, 30, font));
            param_inputs.back()->setText(std::to_string(element->getValue()));
        } else if (type == "SinusoidalVoltageSource") {
            auto* sin_src = dynamic_cast<SinusoidalVoltageSource*>(element);
            if (sin_src) {
                // Increase dialog size for better visibility
                dialog_rect.w = 400;  // Make wider
                dialog_rect.h = 300;  // Make taller
                dialog_rect.x = dialog_rect.x - 50;  // Adjust position for wider dialog
                
                param_labels.push_back("Amplitude (V):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 150, dialog_rect.y + 100, 200, 30, font));
                param_inputs.back()->setText(std::to_string(sin_src->getAmplitude()));

                param_labels.push_back("Frequency (Hz):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 150, dialog_rect.y + 150, 200, 30, font));
                param_inputs.back()->setText(std::to_string(sin_src->getFrequency()));

                param_labels.push_back("DC Offset (V):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 150, dialog_rect.y + 200, 200, 30, font));
                param_inputs.back()->setText(std::to_string(sin_src->getDCOffset()));
            }
        } else if (type == "PulseVoltageSource") {
            auto* pulse_src = dynamic_cast<PulseVoltageSource*>(element);
            if (pulse_src) {
                // Increase dialog size for pulse parameters
                dialog_rect.w = 450;  // Make wider
                dialog_rect.h = 400;  // Make taller
                dialog_rect.x = dialog_rect.x - 75;  // Adjust position for wider dialog
                
                // V1 - Lower voltage
                param_labels.push_back("Lower Voltage V1 (V):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 100, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_src->getV1()));

                // V2 - Upper voltage
                param_labels.push_back("Upper Voltage V2 (V):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 140, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_src->getV2()));

                // TD - Delay time
                param_labels.push_back("Delay Time TD (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 180, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_src->getTd()));

                // TR - Rise time
                param_labels.push_back("Rise Time TR (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 220, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_src->getTr()));

                // TF - Fall time
                param_labels.push_back("Fall Time TF (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 260, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_src->getTf()));

                // PW - Pulse width
                param_labels.push_back("Pulse Width PW (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 300, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_src->getPw()));

                // PER - Period
                param_labels.push_back("Period PER (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 340, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_src->getPer()));
            }
        } else if (type == "PulseCurrentSource") {
            auto* pulse_curr = dynamic_cast<PulseCurrentSource*>(element);
            if (pulse_curr) {
                // Increase dialog size for pulse parameters
                dialog_rect.w = 450;  // Make wider
                dialog_rect.h = 400;  // Make taller
                dialog_rect.x = dialog_rect.x - 75;  // Adjust position for wider dialog
                
                // I1 - Lower current
                param_labels.push_back("Lower Current I1 (A):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 100, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_curr->getI1()));

                // I2 - Upper current
                param_labels.push_back("Upper Current I2 (A):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 140, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_curr->getI2()));

                // TD - Delay time
                param_labels.push_back("Delay Time TD (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 180, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_curr->getTd()));

                // TR - Rise time
                param_labels.push_back("Rise Time TR (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 220, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_curr->getTr()));

                // TF - Fall time
                param_labels.push_back("Fall Time TF (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 260, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_curr->getTf()));

                // PW - Pulse width
                param_labels.push_back("Pulse Width PW (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 300, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_curr->getPw()));

                // PER - Period
                param_labels.push_back("Period PER (s):");
                param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 200, dialog_rect.y + 340, 200, 25, font));
                param_inputs.back()->setText(std::to_string(pulse_curr->getPer()));
            }
        }
    }

    // Update input box values
    if (value_input && element) {
        value_input->setText(std::to_string(element->getValue()));
    }

    for (size_t i = 0; i < param_inputs.size() && i < param_labels.size(); ++i) {
        if (param_inputs[i]) {
            // Values are already set above
        }
    }
}

void ComponentEditDialog::show() {
    is_visible = true;
    // Focus functionality not available in InputBox
    // if (value_input) value_input->setFocus(true);
}

void ComponentEditDialog::hide() {
    is_visible = false;
}

void ComponentEditDialog::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;

    // Handle dialog events
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int mx = event.button.x, my = event.button.y;

        // Check apply button
        SDL_Rect apply_btn = {dialog_rect.x + 10, dialog_rect.y + dialog_rect.h - 40, 80, 30};
        if (mx >= apply_btn.x && mx <= apply_btn.x + apply_btn.w &&
            my >= apply_btn.y && my <= apply_btn.y + apply_btn.h) {
            // Apply changes
            if (target_element) {
                try {
                    std::string type = target_element->getType();
                    
                    if (type == "PulseVoltageSource") {
                        auto* pulse_src = dynamic_cast<PulseVoltageSource*>(target_element);
                        if (pulse_src && param_inputs.size() >= 7) {
                            // Update all pulse parameters
                            pulse_src->setV1(std::stod(param_inputs[0]->getText()));
                            pulse_src->setV2(std::stod(param_inputs[1]->getText()));
                            pulse_src->setTd(std::stod(param_inputs[2]->getText()));
                            pulse_src->setTr(std::stod(param_inputs[3]->getText()));
                            pulse_src->setTf(std::stod(param_inputs[4]->getText()));
                            pulse_src->setPw(std::stod(param_inputs[5]->getText()));
                            pulse_src->setPer(std::stod(param_inputs[6]->getText()));
                            ErrorManager::info("Pulse voltage source parameters updated");
                        }
                    } else if (type == "PulseCurrentSource") {
                        auto* pulse_curr = dynamic_cast<PulseCurrentSource*>(target_element);
                        if (pulse_curr && param_inputs.size() >= 7) {
                            // Update all pulse parameters
                            pulse_curr->setI1(std::stod(param_inputs[0]->getText()));
                            pulse_curr->setI2(std::stod(param_inputs[1]->getText()));
                            pulse_curr->setTd(std::stod(param_inputs[2]->getText()));
                            pulse_curr->setTr(std::stod(param_inputs[3]->getText()));
                            pulse_curr->setTf(std::stod(param_inputs[4]->getText()));
                            pulse_curr->setPw(std::stod(param_inputs[5]->getText()));
                            pulse_curr->setPer(std::stod(param_inputs[6]->getText()));
                            ErrorManager::info("Pulse current source parameters updated");
                        }
                    } else if (type == "SinusoidalVoltageSource") {
                        auto* sin_src = dynamic_cast<SinusoidalVoltageSource*>(target_element);
                        if (sin_src && param_inputs.size() >= 3) {
                            // Update sinusoidal parameters
                            sin_src->setAmplitude(std::stod(param_inputs[0]->getText()));
                            sin_src->setFrequency(std::stod(param_inputs[1]->getText()));
                            sin_src->setDCOffset(std::stod(param_inputs[2]->getText()));
                            ErrorManager::info("Sinusoidal source parameters updated");
                        }
                    } else if (value_input) {
                        // Handle simple components with single value
                    double new_value = std::stod(value_input->getText());
                    target_element->setValue(new_value);
                    ErrorManager::info("Component value updated");
                    }
                } catch (const std::exception& e) {
                    ErrorManager::displayError("Invalid value entered: " + std::string(e.what()));
                }
            }

            if (on_apply_callback) on_apply_callback();
            hide();
            return;
        }

        // Check cancel button
        SDL_Rect cancel_btn = {dialog_rect.x + 100, dialog_rect.y + dialog_rect.h - 40, 80, 30};
        if (mx >= cancel_btn.x && mx <= cancel_btn.x + cancel_btn.w &&
            my >= cancel_btn.y && my <= cancel_btn.y + cancel_btn.h) {
            if (on_cancel_callback) on_cancel_callback();
            hide();
            return;
        }

        // Check close button (X)
        SDL_Rect close_btn = {dialog_rect.x + dialog_rect.w - 30, dialog_rect.y + 10, 20, 20};
        if (mx >= close_btn.x && mx <= close_btn.x + close_btn.w &&
            my >= close_btn.y && my <= close_btn.y + close_btn.h) {
            hide();
            return;
        }
    }

    // Handle input box events
    if (value_input) value_input->handleEvent(event);
    for (auto& input : param_inputs) {
        if (input) input->handleEvent(event);
    }
}

void ComponentEditDialog::render(SDL_Renderer* renderer) {
    if (!is_visible) return;

    // Draw dialog background
    SDL_SetRenderDrawColor(renderer, 250, 250, 255, 240);
    SDL_RenderFillRect(renderer, &dialog_rect);

    // Draw border
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &dialog_rect);

    // Draw title
    if (font && target_element) {
        std::string title = "Edit " + target_element->getName() + " (" + target_element->getType() + ")";
        SDL_Surface* surface = TTF_RenderText_Blended(font, title.c_str(), {0, 0, 0, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = {dialog_rect.x + 10, dialog_rect.y + 10, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // Draw input boxes and labels
    if (value_input) {
        // Draw value label
        if (font) {
            SDL_Surface* surface = TTF_RenderText_Blended(font, "Value:", {20, 20, 20, 255});
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect dest = {dialog_rect.x + 10, dialog_rect.y + 40, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
            }
        }

        // Render input box
        value_input->render(renderer);
    }

    // Draw parameter inputs
    for (size_t i = 0; i < param_labels.size() && i < param_inputs.size(); ++i) {
        // Draw label
        if (font) {
            SDL_Surface* surface = TTF_RenderText_Blended(font, param_labels[i].c_str(), {20, 20, 20, 255});
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect dest = {dialog_rect.x + 10, dialog_rect.y + 100 + static_cast<int>(i) * 40, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
            }
        }

        // Render input box
        if (param_inputs[i]) {
            param_inputs[i]->render(renderer);
        }
    }

    // Draw Apply button
    SDL_Rect apply_btn = {dialog_rect.x + 10, dialog_rect.y + dialog_rect.h - 40, 80, 30};
    SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
    SDL_RenderFillRect(renderer, &apply_btn);
    SDL_SetRenderDrawColor(renderer, 80, 160, 80, 255);
    SDL_RenderDrawRect(renderer, &apply_btn);

    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Apply", {255, 255, 255, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = {apply_btn.x + 20, apply_btn.y + 6, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // Draw Cancel button
    SDL_Rect cancel_btn = {dialog_rect.x + 100, dialog_rect.y + dialog_rect.h - 40, 80, 30};
    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
    SDL_RenderFillRect(renderer, &cancel_btn);
    SDL_SetRenderDrawColor(renderer, 160, 80, 80, 255);
    SDL_RenderDrawRect(renderer, &cancel_btn);

    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Cancel", {255, 255, 255, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = {cancel_btn.x + 15, cancel_btn.y + 6, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // Draw close button (X)
    SDL_Rect close_btn = {dialog_rect.x + dialog_rect.w - 30, dialog_rect.y + 10, 20, 20};
    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
    SDL_RenderFillRect(renderer, &close_btn);

    if (font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, "X", {255, 255, 255, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = {close_btn.x + 6, close_btn.y + 2, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
}

void GuiApplication::run() {
    while (is_running) {
        handleEvents();
        render();
    }
}

void GuiApplication::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) is_running = false;
        if (event.type == SDL_MOUSEMOTION) {
            current_mouse_pos = { event.motion.x, event.motion.y };
            
            // Handle probe mode hover detection
            if (is_probe_mode) {
                handleProbeHover(event.motion.x, event.motion.y);
            }
            
            // Update pin hover states for wire mode
            if (schematic_view && (is_drawing_wire || is_creating_wire_from_pin)) {
                schematic_view->updatePinHoverStates(event.motion.x, event.motion.y, is_drawing_wire, is_creating_wire_from_pin);
            }
        }

        // LTSpice-style keyboard shortcuts
        if (event.type == SDL_KEYDOWN) {
            const Uint8* keystate = SDL_GetKeyboardState(NULL);
            bool ctrl_pressed = keystate[SDL_SCANCODE_LCTRL] || keystate[SDL_SCANCODE_RCTRL];
            
            switch (event.key.keysym.sym) {
                case SDLK_F2:
                    // Open component library (toggle component selector)
                    onToggleComponentSelector();
                    break;
                case SDLK_F3:
                    // Toggle settings panel
                    onToggleSettingsPanel();
                    break;
                case SDLK_w:
                    // Also support 'W' as a shortcut to toggle wire mode
                    is_drawing_wire = !is_drawing_wire;
                    if (is_drawing_wire) {
                        is_creating_wire_from_pin = false;
                        placing_component_type = "Wire";
                        resetPlacementState();
                        std::cout << "Wire mode ACTIVATED via W. Click on pins to connect them." << std::endl;
                    } else {
                        is_creating_wire_from_pin = false;
                        placing_component_type = "";
                        cancelWireCreation();
                        std::cout << "Wire mode DEACTIVATED via W." << std::endl;
                    }
                    break;
                case SDLK_F4:
                    // Label net
                    onAddNodeLabel();
                    break;
                case SDLK_r:
                    if (ctrl_pressed) {
                        // Rotate component (TODO: implement rotation)
                        std::cout << "Component rotation not yet implemented" << std::endl;
                    } else {
                        // Place resistor
                        std::cout << "R key pressed - selecting resistor for placement" << std::endl;
                        selectComponentToPlace("Resistor");
                    }
                    break;
                case SDLK_c:
                    if (!ctrl_pressed) {
                        // Place capacitor
                        selectComponentToPlace("Capacitor");
                    }
                    break;
                case SDLK_l:
                    if (!ctrl_pressed) {
                        // Place inductor
                        selectComponentToPlace("Inductor");
                    }
                    break;
                case SDLK_d:
                    if (!ctrl_pressed) {
                        // Place diode
                        selectComponentToPlace("Diode");
                    }
                    break;
                case SDLK_g:
                    if (!ctrl_pressed) {
                        // Place ground
                        selectComponentToPlace("Ground");
                    }
                    break;
                case SDLK_v:
                    if (!ctrl_pressed) {
                        // Place voltage source
                        selectComponentToPlace("IndependentVoltageSource");
                    }
                    break;
                case SDLK_i:
                    if (!ctrl_pressed) {
                        // Place current source
                        selectComponentToPlace("IndependentCurrentSource");
                    }
                    break;
                case SDLK_F5:
                    // Toggle voltage display on schematic
                    if (schematic_view) {
                        bool current_state = schematic_view->getShowVoltages();
                        schematic_view->setShowVoltages(!current_state);
                        std::cout << "Voltage display " << (!current_state ? "ENABLED" : "DISABLED") << " on schematic" << std::endl;

                        // Reset debug flag so we can see signal list again
                        static bool* debug_flag = nullptr;
                        if (debug_flag) *debug_flag = false;
                    }
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    // Enter key handling is now done by individual components (settings panel)
                    // Show hint if pressed outside any component
                    if (!settings_panel || !settings_panel->isVisible()) {
                        std::cout << "[ENTER] Press F3 to open settings panel, then ENTER to run analysis" << std::endl;
                        std::cout << "[ENTER] Or use Ctrl+A for direct AC analysis" << std::endl;
                    }
                    break;
                case SDLK_a:
                    if (ctrl_pressed) {
                        // Direct AC analysis shortcut
                        std::cout << "[Ctrl+A] Running direct AC analysis" << std::endl;
                        onRunACAnalysisClicked();
                    }
                    break;
                case SDLK_p:
                    if (ctrl_pressed) {
                        // Direct phase analysis shortcut
                        std::cout << "[Ctrl+P] Running direct phase analysis" << std::endl;
                        onRunPhaseAnalysisClicked();
                    }
                    break;
                case SDLK_e:
                    if (ctrl_pressed) {
                        // Mirror component (TODO: implement mirroring)
                        std::cout << "Component mirroring not yet implemented" << std::endl;
                    }
                    break;
                case SDLK_s:
                    if (ctrl_pressed) {
                        // Save project
                        std::cout << "[Ctrl+S] Saving project..." << std::endl;
                        onSaveProjectClicked();
                    }
                    break;
                case SDLK_o:
                    if (ctrl_pressed) {
                        // Open/Load project
                        std::cout << "[Ctrl+O] Loading project..." << std::endl;
                        onLoadProjectClicked();
                    }
                    break;
                case SDLK_ESCAPE:
                    // Cancel current operation
                    is_drawing_wire = false;
                    is_creating_wire_from_pin = false;
                    placing_component_type = "";
                    resetPlacementState();
                    cancelWireCreation();
                    std::cout << "Operation cancelled" << std::endl;
                    break;
            }
        }

        // LTSpice-style wire drawing: Process in handleSchematicClick to avoid conflicts
        // (Logic moved to handleSchematicClick for better event coordination)

        // Handle wire completion for grid-to-grid connections
        if (event.type == SDL_MOUSEBUTTONUP && is_wire_drag_active && is_drawing_wire) {
            // If we were drawing a pin-to-pin wire, skip grid completion to avoid duplicate wires
            if (is_creating_wire_from_pin) {
                is_wire_drag_active = false;
                wire_start_node = "";
                continue; // skip grid wiring path
            }
            if (schematic_view) {
                // Check if releasing near a pin first (prioritize pin connections)
                auto near_pin = schematic_view->getPinNear(event.button.x, event.button.y, 25);
                if (near_pin) {
                    // Create a virtual start pin at the grid location for the wire start
                    SDL_Point start_pos = wire_draw_start_pos;
                    auto start_pin = std::make_shared<Pin>(wire_start_node + ".virtual", "GRID", 1, start_pos);
                    start_pin->setNodeId(wire_start_node);
                    
                    // Connect grid point to pin (GUI-only to avoid back-end duplication)
                    pushUndoSnapshot();
                    schematic_view->createGuiWireOnly(start_pin, near_pin);
                    std::cout << "Connected GUI wire from grid " << wire_start_node << " to pin: " << near_pin->getFullId() << std::endl;
                    
                    // Add backend connectivity once
                    circuit->addElement(std::make_unique<CircuitWire>("W" + std::to_string(circuit->getElements().size() + 1), wire_start_node, near_pin->getNodeId()));
                } else {
                    // Connect to grid point
                    SDL_Point end_pos = schematic_view->snapToGrid(event.button.x, event.button.y);
                    std::string end_node = schematic_view->getNodeAt(end_pos.x, end_pos.y);
                    if (!end_node.empty() && end_node != wire_start_node) {
                        pushUndoSnapshot();
                        // Create GUI wire between start and end grid points
                        auto start_pin = std::make_shared<Pin>(wire_start_node + ".virtual", "GRID", 1, wire_draw_start_pos);
                        auto end_pin = std::make_shared<Pin>(end_node + ".virtual", "GRID", 1, end_pos);
                        start_pin->setNodeId(wire_start_node);
                        end_pin->setNodeId(end_node);
                        if (schematic_view) schematic_view->createGuiWireOnly(start_pin, end_pin);
                        
                        // Add backend connectivity once
                        circuit->addElement(std::make_unique<CircuitWire>("W" + std::to_string(circuit->getElements().size() + 1), wire_start_node, end_node));
                        std::cout << "Created GUI+backend wire from " << wire_start_node << " to " << end_node << std::endl;
                    }
                }
            }
            is_wire_drag_active = false;
            wire_start_node = "";
        }

        // Always process UI elements first
    for (auto& el : ui_elements) {
        el->handleEvent(event);
    }
        
        // Then handle schematic clicks (includes wire mode and component placement)
        if (!is_wire_drag_active) {
            handleSchematicClick(event);
        }
    }
}

void GuiApplication::render() {
    SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
    SDL_RenderClear(renderer);
    
    // Get screen dimensions for layout
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);
    int screen_width = display_mode.w;
    int screen_height = display_mode.h;
    int menu_height = 30;
    int schematic_height = (screen_height - menu_height) * 0.40;
    
    // Draw fine separator line between schematic and plot sections
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    int separator_y = menu_height + schematic_height;
    SDL_RenderDrawLine(renderer, 0, separator_y, screen_width, separator_y);
    
    // Draw fine grid pattern on separator area
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    for (int x = 0; x < screen_width; x += 20) {
        SDL_RenderDrawLine(renderer, x, separator_y - 1, x, separator_y + 1);
    }
    
    for (auto& el : ui_elements) el->render(renderer);

    // Render wire preview when creating wire from pin
    if (is_creating_wire_from_pin && wire_start_pin) {
        SDL_Point start_pos = wire_start_pin->getPosition();
        SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255); // Blue preview line
        
        // Show grid-snapped preview
        if (schematic_view) {
            SDL_Point snap_pos = schematic_view->snapToGrid(current_mouse_pos.x, current_mouse_pos.y);
            SDL_RenderDrawLine(renderer, start_pos.x, start_pos.y, snap_pos.x, snap_pos.y);
            
            // Draw grid snap indicator
            SDL_SetRenderDrawColor(renderer, 100, 200, 255, 150);
            const int SNAP_SIZE = 6;
            SDL_Rect snap_indicator = {snap_pos.x - SNAP_SIZE/2, snap_pos.y - SNAP_SIZE/2, SNAP_SIZE, SNAP_SIZE};
            SDL_RenderFillRect(renderer, &snap_indicator);
        } else {
            SDL_RenderDrawLine(renderer, start_pos.x, start_pos.y, current_mouse_pos.x, current_mouse_pos.y);
        }

        // Highlight potential connection targets
        if (schematic_view) {
            auto near_pin = schematic_view->getPinNear(current_mouse_pos.x, current_mouse_pos.y, 20);
            if (near_pin && near_pin != wire_start_pin) {
                SDL_Point target_pos = near_pin->getPosition();
                // Draw connection indicator
                SDL_SetRenderDrawColor(renderer, 255, 200, 0, 200); // Semi-transparent orange
                const int INDICATOR_SIZE = 10;
                SDL_Rect indicator = {target_pos.x - INDICATOR_SIZE/2, target_pos.y - INDICATOR_SIZE/2, INDICATOR_SIZE, INDICATOR_SIZE};
                SDL_RenderFillRect(renderer, &indicator);
            }
        }
    }

    // Wire drag preview (for grid-to-grid connections)
    if (is_wire_drag_active && is_drawing_wire) {
        SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255);
        
        if (schematic_view) {
            // Show grid-snapped preview
            SDL_Point snap_pos = schematic_view->snapToGrid(current_mouse_pos.x, current_mouse_pos.y);
            SDL_RenderDrawLine(renderer, wire_draw_start_pos.x, wire_draw_start_pos.y, snap_pos.x, snap_pos.y);
            
            // Draw grid snap indicator at end point
            SDL_SetRenderDrawColor(renderer, 100, 200, 255, 150);
            const int SNAP_SIZE = 6;
            SDL_Rect snap_indicator = {snap_pos.x - SNAP_SIZE/2, snap_pos.y - SNAP_SIZE/2, SNAP_SIZE, SNAP_SIZE};
            SDL_RenderFillRect(renderer, &snap_indicator);
            
            // Highlight if near a pin
            auto near_pin = schematic_view->getPinNear(current_mouse_pos.x, current_mouse_pos.y, 20);
            if (near_pin) {
                SDL_Point target_pos = near_pin->getPosition();
                SDL_SetRenderDrawColor(renderer, 255, 200, 0, 200);
                const int INDICATOR_SIZE = 10;
                SDL_Rect indicator = {target_pos.x - INDICATOR_SIZE/2, target_pos.y - INDICATOR_SIZE/2, INDICATOR_SIZE, INDICATOR_SIZE};
                SDL_RenderFillRect(renderer, &indicator);
            }
        } else {
            SDL_RenderDrawLine(renderer, wire_draw_start_pos.x, wire_draw_start_pos.y, current_mouse_pos.x, current_mouse_pos.y);
        }
    }

    renderComponentPreview(renderer); // Render component preview
    
    // Render status text for current mode
    if (font) {
        std::string status_text;
        if (is_drawing_wire) {
            if (is_creating_wire_from_pin) {
                status_text = "Wire Mode: Click on a pin or grid point to complete wire";
            } else {
                status_text = "Wire Mode: Click on pins or grid points to start wire (F3 to toggle, ESC to cancel)";
            }
        } else if (is_probe_mode) {
            status_text = "Probe Mode: " + std::string(current_probe_type == ProbeType::VOLTAGE ? "Voltage" : "Current") + 
                         " - Probes: " + std::to_string(selected_signals.size()) + 
                         " (Hover over " + std::string(current_probe_type == ProbeType::VOLTAGE ? "nodes" : "components") + " to place)";
        } else if (!placing_component_type.empty()) {
            status_text = "Placing: " + placing_component_type + " (ESC to cancel)";
        } else {
            if (schematic_view) {
                std::string components_info = std::to_string(circuit->getElements().size()) + " components";
                std::string probes_info = analysis_completed ? ", " + std::to_string(selected_signals.size()) + " probes" : "";
                status_text = "Ready (F2: Components, F3: Settings, F4: Label, F5: Toggle Voltages, Ctrl+A: AC Analysis, Ctrl+P: Phase Analysis, R: Resistor, C: Capacitor, L: Inductor, G: Ground, V: Voltage, I: Current) - " + components_info + probes_info;
            } else {
                status_text = "Ready (F2: Components, F3: Settings, F4: Label, F5: Toggle Voltages, Ctrl+A: AC Analysis, Ctrl+P: Phase Analysis, R: Resistor, C: Capacitor, L: Inductor, G: Ground, V: Voltage, I: Current)";
            }
        }
        
        SDL_Surface* surface = TTF_RenderText_Blended(font, status_text.c_str(), {50, 50, 50, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect dest_rect = {10, 10, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    SDL_RenderPresent(renderer);
}

void GuiApplication::cleanup() {
    for (auto const& [key, val] : component_textures) if (val) SDL_DestroyTexture(val);
    component_textures.clear();
    ui_elements.clear();
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

void GuiApplication::onRunSimulationClicked() {
    MNAMatrix mna;
    LUDecompositionSolver solver;
    TransientAnalysis tran(settings_panel->getTranTStep(), settings_panel->getTranTStop(), true); // Use UIC (Use Initial Conditions) = true
    try {
        if (!circuit->checkGroundNodeExists()) throw std::runtime_error("No ground node set.");
        if (!circuit->checkConnectivity()) throw std::runtime_error("Circuit not fully connected.");
        
        // Debug: Check if circuit has transient-sensitive elements
        bool has_reactive = false;
        bool has_pulse_sources = false;
        for (const auto& elem : circuit->getElements()) {
            if (elem->getType() == "Capacitor" || elem->getType() == "Inductor") {
                has_reactive = true;
                ErrorManager::info("[TRAN] Found reactive element: " + elem->getName() + " (" + elem->getType() + ")");
            } else if (elem->getType() == "PulseVoltageSource" || elem->getType() == "PulseCurrentSource" || 
                      elem->getType() == "SinusoidalVoltageSource" || elem->getType() == "ACVoltageSource") {
                has_pulse_sources = true;
                ErrorManager::info("[TRAN] Found time-dependent source: " + elem->getName() + " (" + elem->getType() + ")");
            }
        }
        if (has_reactive || has_pulse_sources) {
            ErrorManager::info("[TRAN] Circuit has time-dependent elements - dynamic response expected");
        } else {
            ErrorManager::info("[TRAN] No time-dependent elements found - response will be DC steady-state");
        }
        {
            std::stringstream ss; ss << "[TRAN] starting: dt=" << settings_panel->getTranTStep() << ", tstop=" << settings_panel->getTranTStop();
            std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());
        }
        try {
            tran.analyze(*circuit, mna, solver);
        } catch (const std::exception& ex) {
            std::stringstream ss; ss << "[TRAN] exception: " << ex.what();
            std::cout << ss.str() << std::endl; logLine(ss.str());
            throw; // propagate to existing error handler
        }
        {
            std::stringstream ss; ss << "[TRAN] finished: points=" << tran.getTimePoints().size() << ", vars=" << tran.getResults().size();
            std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());
        }
        // Store latest results for probes and schematic display
        latest_time_points = tran.getTimePoints();
        latest_tran_results = tran.getResults();
        if (latest_time_points.empty() || latest_tran_results.empty()) {
            logLine("[TRAN] WARNING: No data produced (points or results empty)");
        }

        // Update schematic view with latest results and enable voltage display
        if (schematic_view) {
            schematic_view->setLatestResults(latest_tran_results);
            schematic_view->setShowVoltages(true);
        }

        if (probe_panel) probe_panel->setSignalsFromResults(latest_tran_results);
        
        // Auto-select voltage signals for initial display, prioritize non-zero signals
        if (selected_signals.empty() && !latest_tran_results.empty()) {
            std::vector<std::string> non_zero_signals;
            std::vector<std::string> all_voltage_signals;
            
            for (const auto& pair : latest_tran_results) {
                if (pair.first.find("V(") == 0) { // Voltage signals start with "V("
                    all_voltage_signals.push_back(pair.first);
                    
                    // Check if signal has variation (not constant)
                    if (pair.second.size() > 1) {
                        double min_val = *std::min_element(pair.second.begin(), pair.second.end());
                        double max_val = *std::max_element(pair.second.begin(), pair.second.end());
                        if (std::abs(max_val - min_val) > 1e-6) {
                            non_zero_signals.push_back(pair.first);
                        }
                    }
                }
            }
            
            // Prefer signals with variation, fall back to any voltage signals
            auto& signals_to_use = non_zero_signals.empty() ? all_voltage_signals : non_zero_signals;
            for (const auto& signal : signals_to_use) {
                selected_signals.insert(signal);
                if (selected_signals.size() >= 3) break;
            }
            
            // Log what we selected and signal characteristics
            std::stringstream ss;
            ss << "[TRAN] Auto-selected signals: ";
            for (const auto& sig : selected_signals) ss << sig << " ";
            ss << "(from " << non_zero_signals.size() << " varying + " << all_voltage_signals.size() << " total)";
            ErrorManager::info(ss.str());
            
            // Debug: Show first few values of each selected signal
            for (const auto& sig_name : selected_signals) {
                if (latest_tran_results.count(sig_name) && !latest_tran_results.at(sig_name).empty()) {
                    const auto& values = latest_tran_results.at(sig_name);
                    std::stringstream debug_ss;
                    debug_ss << "[TRAN] " << sig_name << " values: ";
                    for (size_t i = 0; i < std::min(size_t(5), values.size()); ++i) {
                        debug_ss << values[i] << " ";
                    }
                    if (values.size() > 5) debug_ss << "... (" << values.size() << " total)";
                    ErrorManager::info(debug_ss.str());
                }
            }
        }
        
        bool plotted = false;
        for (auto& el : ui_elements) {
            if (auto* scope = dynamic_cast<Oscilloscope*>(el.get())) {
                // Update oscilloscope with selected signals
                scope->clearData();
                scope->setTimePoints(latest_time_points);

                for (const auto& signal_name : selected_signals) {
                    auto it = latest_tran_results.find(signal_name);
                    if (it != latest_tran_results.end()) {
                        scope->addSignal(it->first, it->second);
                    }
                }

                if (!selected_signals.empty()) {
                    scope->autoScale();
                }

                {
                    std::stringstream ss; ss << "[TRAN] plotted selected signals: " << selected_signals.size();
                    std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());
                }
                plotted = true;
                break;
            }
        }
        if (!plotted) logLine("[TRAN] WARNING: Oscilloscope not found");
        
        // Enable node name display after successful analysis
        analysis_completed = true;
        if (schematic_view) {
            schematic_view->setShowNodeNames(true);
        }
        
    } catch (const std::exception& e) {
        ErrorManager::displayError("Simulation failed: " + std::string(e.what()));
    }
}

void GuiApplication::onRunDCAnalysisClicked() {
    try {
        if (!circuit->checkGroundNodeExists()) throw std::runtime_error("No ground node set.");

        std::cout << "[DC] Starting DC analysis..." << std::endl;

        bool success = circuit->analyzeDC();

        if (success) {
            std::cout << "[DC] Analysis completed successfully" << std::endl;
            ErrorManager::info("[DC] DC analysis completed successfully");
        } else {
            std::cout << "[DC] Analysis failed" << std::endl;
            ErrorManager::displayError("[DC] DC analysis failed");
        }

    } catch (const std::exception& e) {
        ErrorManager::displayError("DC Analysis failed: " + std::string(e.what()));
    }
}

void GuiApplication::onRunACAnalysisClicked() {
    try {
        if (!circuit->checkGroundNodeExists()) throw std::runtime_error("No ground node set.");

        // Check for AC sources in the circuit
        bool has_ac_sources = false;
        std::string ac_source_name;
        std::cout << "[AC DEBUG] Checking for AC sources in circuit:" << std::endl;
        for (const auto& elem : circuit->getElements()) {
            std::cout << "[AC DEBUG] Element: " << elem->getName() << ", Type: " << elem->getType() << std::endl;
            if (elem->getType() == "ACVoltageSource" || 
                elem->getType() == "SinusoidalVoltageSource" ||
                elem->getType() == "IndependentVoltageSource") {
                has_ac_sources = true;
                ac_source_name = elem->getName();
                std::cout << "[AC DEBUG] Found AC source: " << elem->getName() << std::endl;
            }
        }
        
        if (!has_ac_sources) {
            ErrorManager::info("[AC] No AC sources found - add ACVoltageSource for frequency analysis");
            return;
        }
        
        std::cout << "[AC DEBUG] Using AC source: " << ac_source_name << " for analysis" << std::endl;

        // Use simplified AC analysis
        double start_freq = settings_panel->getACStartFreq();
        double stop_freq = settings_panel->getACStopFreq();
        int points = settings_panel->getACPoints();

        // Convert frequency to angular frequency
        double start_omega = 2 * M_PI * start_freq;
        double stop_omega = 2 * M_PI * stop_freq;

        // Find a suitable signal to analyze - use the first non-ground node
        std::string desired_signal = "";
        std::string ground_node = circuit->getGroundNodeId();
        std::cout << "[AC DEBUG] Ground node: " << ground_node << std::endl;
        
        // First try to find nodes from resistors/capacitors/inductors
        for (const auto& elem : circuit->getElements()) {
            if (elem->getType() == "Resistor" || elem->getType() == "Capacitor" || elem->getType() == "Inductor") {
                // Get the nodes connected to this element
                std::string node1 = elem->getNode1Id();
                std::string node2 = elem->getNode2Id();
                
                std::cout << "[AC DEBUG] Element " << elem->getName() << " connected to nodes: " << node1 << ", " << node2 << std::endl;
                
                if (node1 != "0" && node1 != ground_node && !node1.empty()) {
                    desired_signal = "V(" + node1 + ")";
                    std::cout << "[AC DEBUG] Using signal from element " << elem->getName() << ": " << desired_signal << std::endl;
                    break;
                } else if (node2 != "0" && node2 != ground_node && !node2.empty()) {
                    desired_signal = "V(" + node2 + ")";
                    std::cout << "[AC DEBUG] Using signal from element " << elem->getName() << ": " << desired_signal << std::endl;
                    break;
                }
            }
        }
        
        // If still empty, try to find any non-ground node from the circuit
        if (desired_signal.empty()) {
            std::cout << "[AC DEBUG] No signal found from elements, searching circuit nodes..." << std::endl;
            for (const auto& node_pair : circuit->getNodes()) {
                std::string node_id = node_pair.first;
                if (node_id != "0" && node_id != ground_node && !node_id.empty()) {
                    desired_signal = "V(" + node_id + ")";
                    std::cout << "[AC DEBUG] Using signal from circuit node: " << desired_signal << std::endl;
                    break;
                }
            }
        }
        
        if (desired_signal.empty()) {
            // Fallback to any available node
            desired_signal = "V(N1)";
            std::cout << "[AC DEBUG] Using fallback signal: " << desired_signal << std::endl;
        }

        std::cout << "[AC DEBUG] Starting AC sweep: " << start_freq << " Hz to " << stop_freq << " Hz, " << points << " points" << std::endl;
        std::cout << "[AC DEBUG] Angular frequency range: " << start_omega << " to " << stop_omega << " rad/s" << std::endl;
        std::cout << "[AC DEBUG] Analyzing signal: " << desired_signal << std::endl;

        ACSweepVals result = circuit->ACsweep(start_omega, stop_omega, points, desired_signal);

        // Debug: Check if we have data
        std::cout << "[AC DEBUG] Frequency points: " << result.freq.size() << std::endl;
        std::cout << "[AC DEBUG] AC results: " << result.ys.size() << " signals" << std::endl;
        
        if (!result.freq.empty()) {
            std::cout << "[AC DEBUG] Frequency range: " << result.freq[0] << " to " << result.freq.back() << " Hz" << std::endl;
        }

        if (result.freq.empty()) {
            ErrorManager::displayError("[AC] No frequency points generated");
            return;
        }

        // Convert to format expected by plotting system
        std::map<std::string, std::vector<Complex>> complex_results;
        for (size_t i = 0; i < result.names.size(); ++i) {
            std::vector<Complex> complex_data;
            for (double mag : result.ys[i]) {
                complex_data.push_back(Complex(mag, 0.0)); // Magnitude only for now
            }
            complex_results[result.names[i]] = complex_data;
        }

        // Enable AC analysis plotting
        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<Oscilloscope*>(el.get())) {
                plot->setDataAC(result.freq, complex_results);
                std::cout << "[AC DEBUG] Data sent to plot view" << std::endl;
                break;
            }
        }
        
        std::cout << "[AC] Analysis complete: " << result.freq.size() << " frequency points" << std::endl;

    } catch (const std::exception& e) {
        ErrorManager::displayError("AC Simulation failed: " + std::string(e.what()));
    }
}

void GuiApplication::onRunPhaseAnalysisClicked() {
    MNAMatrix mna;
    LUDecompositionSolver solver;

    // Use settings panel values for phase analysis
    std::string source_name = settings_panel->getACSource();
    double base_freq = 1000.0; // Default 1kHz base frequency
    int points = settings_panel->getACPoints();

    PhaseSweepAnalysis phase(source_name, 0, 360, base_freq, points);

    try {
        if (!circuit->checkGroundNodeExists()) throw std::runtime_error("No ground node set.");
        if (!circuit->checkConnectivity()) throw std::runtime_error("Circuit not fully connected.");

        // Check for AC sources in the circuit
        bool has_ac_sources = false;
        for (const auto& elem : circuit->getElements()) {
            if (elem->getType() == "ACVoltageSource" ||
                elem->getType() == "SinusoidalVoltageSource") {
                has_ac_sources = true;
                break;
            }
        }

        if (!has_ac_sources) {
            ErrorManager::info("[PHASE] No AC sources found - add ACVoltageSource for phase analysis");
        }

        std::cout << "[PHASE DEBUG] Starting phase analysis with source: " << source_name << std::endl;
        std::cout << "[PHASE DEBUG] Base frequency: " << base_freq << " Hz, Points: " << points << std::endl;

        phase.analyze(*circuit, mna, solver);

        // Debug: Check if we have data
        auto phase_points = phase.getPhasePoints();
        auto phase_results = phase.getComplexResults();

        std::cout << "[PHASE DEBUG] Phase points: " << phase_points.size() << std::endl;
        std::cout << "[PHASE DEBUG] Phase results: " << phase_results.size() << " signals" << std::endl;
        
        if (!phase_points.empty()) {
            std::cout << "[PHASE DEBUG] Phase range: " << phase_points[0] << " to " << phase_points.back() << " degrees" << std::endl;
        }

        for (const auto& pair : phase_results) {
            std::cout << "[PHASE DEBUG] Signal: " << pair.first << ", data points: " << pair.second.size() << std::endl;
        }

        if (phase_points.empty()) {
            ErrorManager::displayError("[PHASE] No phase points generated");
        } else if (phase_results.empty()) {
            ErrorManager::displayError("[PHASE] No phase results generated");
        }

        // Enable phase analysis plotting
        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<Oscilloscope*>(el.get())) {
                plot->setDataPhase(phase.getPhasePoints(), phase.getComplexResults());
                std::cout << "[PHASE DEBUG] Data sent to plot view" << std::endl;
                break;
            }
        }

        ErrorManager::info("[PHASE] Analysis complete: " + std::to_string(phase.getPhasePoints().size()) + " phase points");

    } catch (const std::exception& e) {
        ErrorManager::displayError("Phase Simulation failed: " + std::string(e.what()));
    }
}

void GuiApplication::onRunDCSweepClicked() {
    MNAMatrix mna;
    LUDecompositionSolver solver;
    DCSweepAnalysis dc(settings_panel->getACSource(), 0.0, 10.0, 0.1);
    try {
        if (!circuit->checkGroundNodeExists()) throw std::runtime_error("No ground node set.");
        if (!circuit->checkConnectivity()) throw std::runtime_error("Circuit not fully connected.");

        dc.analyze(*circuit, mna, solver);

        // DC sweep plotting disabled - Oscilloscope is for time-domain signals
        // TODO: Implement separate DC sweep plotting if needed
        /*
        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                // Convert DC sweep results to plot format
                std::map<std::string, std::vector<double>> plot_results;
                const auto& dc_results = dc.getResults();
                for (const auto& pair : dc_results) {
                    plot_results[pair.first] = pair.second;
                }
                plot->setData(dc.getSweepValues(), plot_results);
                break;
            }
        }
        */

    } catch (const std::exception& e) {
        ErrorManager::displayError("DC Sweep Simulation failed: " + std::string(e.what()));
    }
}

std::string GuiApplication::openFileDialog(const std::string& title, const std::string& defaultPath, const std::string& filter) {
#ifdef _WIN32
    OPENFILENAME ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Circuit Files\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = defaultPath.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileName(&ofn)) {
        return std::string(szFile);
    }
#endif
    return "";
}

std::string GuiApplication::openLoadFileDialog(const std::string& title, const std::string& defaultPath, const std::string& filter) {
#ifdef _WIN32
    OPENFILENAME ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Circuit Files\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = defaultPath.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileName(&ofn)) {
        return std::string(szFile);
    }
#endif
    return "";
}

void GuiApplication::onSaveProjectClicked() {
    try {
        // Initialize save directory if not already done
        if (save_directory.empty()) {
            char* pref_path = SDL_GetPrefPath("CircuitSim", "Projects");
            if (pref_path) {
                save_directory = std::string(pref_path);
                SDL_free(pref_path);
                std::filesystem::create_directories(save_directory + "circuits");
                std::filesystem::create_directories(save_directory + "exports");
                std::filesystem::create_directories(save_directory + "backups");
            } else {
                save_directory = "./saved_circuits/";
                std::filesystem::create_directories(save_directory);
            }
        }
        
        // Open file dialog to let user choose save location
        std::string default_path = save_directory + "circuits/";
        std::string selected_file = openFileDialog("Save Circuit Project", default_path, "*.json");
        
        if (selected_file.empty()) {
            // User cancelled, use default location with timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream default_name;
            default_name << save_directory << "circuits/circuit_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".json";
            selected_file = default_name.str();
        }
        
        // Ensure .json extension
        if (selected_file.find(".json") == std::string::npos) {
            selected_file += ".json";
        }
        
        // Save the project
        ProjectSerializer::save(*circuit, selected_file);
        
        // Also save as latest version in default location
        std::string latest_file = save_directory + "circuits/circuit_latest.json";
        ProjectSerializer::save(*circuit, latest_file);
        
        std::cout << "\n[SAVE SUCCESS] Project saved to:" << std::endl;
        std::cout << "  Selected file: " << selected_file << std::endl;
        std::cout << "  Latest backup: " << latest_file << std::endl;
        std::cout << "  Default directory: " << save_directory << std::endl;
        
        ErrorManager::info("[SAVE] Project saved to user-selected location");
        
    } catch (const std::exception& e) {
        std::string error_msg = "Failed to save project: " + std::string(e.what());
        std::cout << "[SAVE ERROR] " << error_msg << std::endl;
        ErrorManager::displayError(error_msg);
    }
}

void GuiApplication::onLoadProjectClicked() {
    try {
        // Initialize save directory if not already done
        if (save_directory.empty()) {
            char* pref_path = SDL_GetPrefPath("CircuitSim", "Projects");
            if (pref_path) {
                save_directory = std::string(pref_path);
                SDL_free(pref_path);
            } else {
                save_directory = "./";
            }
        }
        
        // Set default path for file dialog
        std::string default_path = save_directory + "circuits/";
        if (!std::filesystem::exists(default_path)) {
            default_path = save_directory;
        }
        
        // Open file dialog to let user choose file to load
        std::string selected_file = openLoadFileDialog("Load Circuit Project", default_path, "*.json");
        
        if (selected_file.empty()) {
            // User cancelled, try to load from default locations
            std::string load_file;
            if (!save_directory.empty()) {
                load_file = save_directory + "circuits/circuit_latest.json";
                if (!std::filesystem::exists(load_file)) {
                    load_file = "circuit.json"; // Fallback to old location
                }
            } else {
                load_file = "circuit.json";
            }
            
            if (std::filesystem::exists(load_file)) {
                selected_file = load_file;
                std::cout << "[LOAD] Loading from default location: " << load_file << std::endl;
            } else {
                ErrorManager::displayError("No circuit file found to load. Please select a file using the file dialog.");
                return;
            }
        }
        
        // Verify file exists before loading
        if (!std::filesystem::exists(selected_file)) {
            ErrorManager::displayError("Selected file does not exist: " + selected_file);
            return;
        }
        
        // Check file size to ensure it's not empty
        auto file_size = std::filesystem::file_size(selected_file);
        if (file_size == 0) {
            ErrorManager::displayError("Selected file is empty: " + selected_file);
            return;
        }
        
        // Load the project
        ProjectSerializer::load(*circuit, selected_file);
        std::cout << "[LOAD] Project loaded from " << selected_file << " (size: " << file_size << " bytes)" << std::endl;
        ErrorManager::info("[LOAD] Project successfully loaded from: " + std::filesystem::path(selected_file).filename().string());
        
        // Update schematic view if available
        if (schematic_view) {
            schematic_view->rebuildWiresFromCircuit();
        }
        
        // Clear any existing analysis results
        analysis_completed = false;
        
        // Log circuit statistics
        std::cout << "[LOAD] Circuit statistics:" << std::endl;
        std::cout << "  - Elements: " << circuit->getElements().size() << std::endl;
        std::cout << "  - Nodes: " << circuit->getNodes().size() << std::endl;
        std::cout << "  - Ground node: " << circuit->getGroundNodeId() << std::endl;
        
        // Count element types
        int resistors = 0, capacitors = 0, inductors = 0, voltage_sources = 0, current_sources = 0, grounds = 0, wires = 0;
        for (const auto& elem : circuit->getElements()) {
            std::string type = elem->getType();
            if (type == "Resistor") resistors++;
            else if (type == "Capacitor") capacitors++;
            else if (type == "Inductor") inductors++;
            else if (type == "IndependentVoltageSource" || type == "SinusoidalVoltageSource" || type == "PulseVoltageSource") voltage_sources++;
            else if (type == "IndependentCurrentSource") current_sources++;
            else if (type == "Ground") grounds++;
            else if (type == "Wire") wires++;
        }
        
        std::cout << "  - Resistors: " << resistors << std::endl;
        std::cout << "  - Capacitors: " << capacitors << std::endl;
        std::cout << "  - Inductors: " << inductors << std::endl;
        std::cout << "  - Voltage Sources: " << voltage_sources << std::endl;
        std::cout << "  - Current Sources: " << current_sources << std::endl;
        std::cout << "  - Grounds: " << grounds << std::endl;
        std::cout << "  - Wires: " << wires << std::endl;
        
    } catch (const std::exception& e) {
        std::string error_msg = "Failed to load project: " + std::string(e.what());
        std::cout << "[LOAD ERROR] " << error_msg << std::endl;
        ErrorManager::displayError(error_msg);
    }
}

void GuiApplication::onSaveSubcircuitClicked() {
    is_creating_subcircuit = true;
    resetPlacementState();
    std::cout << "Select the first port for the subcircuit." << std::endl;
}

void GuiApplication::onAddNodeLabel() {
    is_labeling_node = true;
    std::cout << "Click on a node to add a label." << std::endl;
}


void GuiApplication::onToggleComponentSelector() {
    if (component_selector) {
        component_selector->toggleVisibility();
        if (component_selector->isVisible()) {
            ErrorManager::info("[GUI] Component selector opened - Click 'Sources' for voltage/current sources");
        }
    }
}

void GuiApplication::onToggleSettingsPanel() {
    if (settings_panel) settings_panel->toggleVisibility();
}

void GuiApplication::onToggleProbePanel() {
    // First, run transient analysis if not already completed
    if (!analysis_completed || latest_tran_results.empty()) {
        ErrorManager::info("[Probe] Running transient analysis first...");
        std::cout << "Running transient analysis for probe mode..." << std::endl;
        
        // Run transient analysis with default parameters
        runTransientAnalysis(0.0, 1e-6, 1e-3); // 0 to 1ms with 1μs steps
    }
    
    // Then toggle the probe panel
    if (probe_panel) {
        // If opening the panel and we have results, auto-select voltage signals
        if (!probe_panel->isVisible() && !latest_tran_results.empty()) {
            probe_panel->setSignalsFromResults(latest_tran_results);
            probe_panel->autoSelectVoltageSignals();
        }
        
        probe_panel->toggleVisibility();
        
        if (probe_panel->isVisible()) {
            ErrorManager::info("[Probe] Probe panel opened - Voltage signals auto-selected, click on nodes to add more");
            std::cout << "Probe panel opened. Voltage signals auto-selected." << std::endl;
        }
    }
}

void GuiApplication::onShowTransientDialog() {
    if (tran_dialog) {
        tran_dialog->show();
        ErrorManager::info("[TRAN] Transient Analysis Dialog opened");
    }
}

void GuiApplication::runTransientAnalysis(double tstart, double tstep, double tstop) {
    try {
        if (!circuit->checkGroundNodeExists()) throw std::runtime_error("No ground node set.");
        if (!circuit->checkConnectivity()) throw std::runtime_error("Circuit not fully connected.");

        std::cout << "[TRAN CUSTOM] Starting transient analysis:" << std::endl;
        std::cout << "  Start Time: " << tstart << "s" << std::endl;
        std::cout << "  Time Step: " << tstep << "s" << std::endl;
        std::cout << "  Stop Time: " << tstop << "s" << std::endl;
        std::cout << "  Expected steps: " << static_cast<int>((tstop - tstart) / tstep) << std::endl;
        
        // Store parameters for the analysis
        latest_tstart = tstart;
        latest_tstep = tstep;
        latest_tstop = tstop;
        
        // Prepare signal list - for now use all nodes and key elements
        std::vector<std::string> desiredSignals;

        // Add all node voltages
        std::vector<Node*> non_ground_nodes;
        NodeIndexMap node_map;
        circuit->getNonGroundNodes(non_ground_nodes, node_map);
        for (const auto& pair : node_map) {
            desiredSignals.push_back("V(" + pair.first + ")");
        }

        // Add ground if it exists
        if (circuit->checkGroundNodeExists()) {
            desiredSignals.push_back("V(" + circuit->getGroundNodeId() + ")");
        }

        // Add currents for key elements
        for (const auto& elem_ptr : circuit->getElements()) {
            std::string type = elem_ptr->getType();
            if (type == "Resistor" || type == "Capacitor" || type == "Inductor" ||
                type == "IndependentVoltageSource" || type == "SinusoidalVoltageSource") {
                desiredSignals.push_back("I(" + elem_ptr->getName() + ")");
            }
        }

        // Use simplified transient analysis
        // Pass total duration (tstop - tstart) as expected by SimpleAnalysis
        TransientSeries result = circuit->analyzeTransientMulti(tstep, std::max(0.0, tstop - tstart), desiredSignals);

        // Convert to the format expected by the existing code
        latest_time_points = result.t;
        latest_tran_results.clear();

        std::cout << "[TRAN DEBUG] Result from analysis: t.size()=" << result.t.size()
                  << ", ys.size()=" << result.ys.size() << ", names.size()=" << result.names.size() << std::endl;

        for (size_t i = 0; i < result.names.size(); ++i) {
            latest_tran_results[result.names[i]] = result.ys[i];
            std::cout << "[TRAN DEBUG] Signal " << result.names[i] << ": " << result.ys[i].size() << " points" << std::endl;
        }

        std::cout << "[TRAN DEBUG] Stored in latest_*: time_points=" << latest_time_points.size()
                  << ", tran_results=" << latest_tran_results.size() << std::endl;

        // Debug: Verify data integrity
        if (!latest_time_points.empty()) {
            std::cout << "[TRAN DEBUG] Time range: " << latest_time_points.front() << " to " << latest_time_points.back() << std::endl;
        }
        if (!latest_tran_results.empty()) {
            std::cout << "[TRAN DEBUG] Signals stored:" << std::endl;
            for (const auto& pair : latest_tran_results) {
                std::cout << "[TRAN DEBUG]   '" << pair.first << "': " << pair.second.size() << " points" << std::endl;
                if (!pair.second.empty()) {
                    std::cout << "[TRAN DEBUG]     Sample values: " << pair.second.front() << " ... " << pair.second.back() << std::endl;
                }
            }
        } else {
            std::cout << "[TRAN DEBUG] ERROR: latest_tran_results is empty after analysis!" << std::endl;
        }
        
        // Update time points to start from tstart if needed
        if (tstart != 0.0) {
            for (auto& t : latest_time_points) {
                t += tstart;
            }
        }

        // Update schematic view with latest results and enable voltage display
        if (schematic_view) {
            schematic_view->setLatestResults(latest_tran_results);
            schematic_view->setShowVoltages(true);
        }
        
        if (probe_panel) {
            probe_panel->setSignalsFromResults(latest_tran_results);
            std::cout << "[TRAN] Updated probe panel with " << latest_tran_results.size() << " signals" << std::endl;
            
            // Debug: Show available signals
            if (!latest_tran_results.empty()) {
                std::cout << "[TRAN] Available signals: ";
                for (const auto& pair : latest_tran_results) {
                    std::cout << pair.first << " ";
                }
                std::cout << std::endl;
            } else {
                std::cout << "[TRAN] WARNING: No signals available in results!" << std::endl;
            }
        }
        
        // Auto-update oscilloscope
        for (auto& el : ui_elements) {
            if (auto* scope = dynamic_cast<Oscilloscope*>(el.get())) {
                if (!latest_time_points.empty() && !latest_tran_results.empty()) {
                    std::cout << "[TRAN DEBUG] Updating oscilloscope with " << latest_time_points.size() << " time points, " << latest_tran_results.size() << " signals" << std::endl;

                    // Clear existing data and set time points
                    scope->clearData();
                    scope->setTimePoints(latest_time_points);

                    // Add all signals
                    for (const auto& pair : latest_tran_results) {
                        scope->addSignal(pair.first, pair.second);
                    }

                    // Auto-scale to fit all data
                    scope->autoScale();
                } else {
                    std::cout << "[TRAN DEBUG] WARNING: Cannot update oscilloscope - no data available!" << std::endl;
                    std::cout << "[TRAN DEBUG]   latest_time_points.empty() = " << latest_time_points.empty() << std::endl;
                    std::cout << "[TRAN DEBUG]   latest_tran_results.empty() = " << latest_tran_results.empty() << std::endl;
                }
                break;
            }
        }
        
        // Auto-show probe panel if there are results available
        if (probe_panel && !latest_tran_results.empty() && !probe_panel->isVisible()) {
            probe_panel->toggleVisibility();
            std::cout << "[TRAN] Auto-showing probe panel with " << latest_tran_results.size() << " signals" << std::endl;
        }
        
        std::cout << "[TRAN CUSTOM] Analysis complete: " << latest_time_points.size() << " points" << std::endl;
        ErrorManager::info("[TRAN] Custom transient analysis completed successfully");
        
    } catch (const std::exception& e) {
        std::string error_msg = "Transient analysis failed: " + std::string(e.what());
        std::cout << "[TRAN ERROR] " << error_msg << std::endl;
        ErrorManager::displayError(error_msg);
    }
}

void GuiApplication::onShowSignalMath() {
    ErrorManager::info("[SignalMath] Signal Math operations activated");
    
    if (!latest_tran_results.empty() && !latest_time_points.empty()) {
        std::map<std::string, std::vector<double>> combined_results = latest_tran_results;
        bool created_signal = false;
        
        // Create multiple math operations
        for (const auto& signal : latest_tran_results) {
            if (signal.first.find("V(") != std::string::npos) {
                std::string base_name = signal.first.substr(2, signal.first.length() - 3); // Remove V( and )
                
                // 1. Power signal (V^2/1k)
                auto power_signal = SignalProcessor::power(signal.second, 2.0);
                power_signal = SignalProcessor::scale(power_signal, 1.0/1000.0); // Assume 1k resistor
                combined_results["P(" + base_name + ")"] = power_signal;
                
                // 2. Absolute value
                auto abs_signal = SignalProcessor::absolute(signal.second);
                combined_results["|V(" + base_name + ")|"] = abs_signal;
                
                // 3. Scaled version (10x)
                auto scaled_signal = SignalProcessor::scale(signal.second, 10.0);
                combined_results["10*V(" + base_name + ")"] = scaled_signal;
                
                created_signal = true;
                ErrorManager::info("[SignalMath] Created power, absolute, and scaled signals for " + signal.first);
                break; // Process only first voltage signal to avoid clutter
            }
        }
        
        // Create derivative of first signal if available
        if (!latest_tran_results.empty()) {
            auto first_signal = latest_tran_results.begin();
            if (first_signal->second.size() > 1) {
                auto derivative = SignalProcessor::derivative(first_signal->second, latest_time_points);
                combined_results["d/dt(" + first_signal->first + ")"] = derivative;
                created_signal = true;
                ErrorManager::info("[SignalMath] Created derivative signal");
            }
        }
        
        if (created_signal) {
            // Update oscilloscope with all derived signals
            for (auto& el : ui_elements) {
                if (auto* scope = dynamic_cast<Oscilloscope*>(el.get())) {
                    scope->clearData();
                    scope->setTimePoints(latest_time_points);

                    for (const auto& pair : combined_results) {
                        scope->addSignal(pair.first, pair.second);
                    }

                    scope->autoScale();
                    break;
                }
            }
            ErrorManager::info("[SignalMath] Updated plot with derived signals");
        } else {
            ErrorManager::info("[SignalMath] No suitable signals found for math operations");
        }
    } else {
        ErrorManager::info("[SignalMath] No analysis data available - run TRAN simulation first");
    }
}



void GuiApplication::onQuitClicked() {
    is_running = false;
}

void GuiApplication::toggleWireMode() {
    is_drawing_wire = !is_drawing_wire;
    if (is_drawing_wire) {
        is_creating_wire_from_pin = false;
        placing_component_type = "";
        is_probe_mode = false; // Disable probe mode when wire mode is enabled
        
        // Reset cursor to default when exiting probe mode
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
        
        std::cout << "Wire mode enabled. Click on pins or grid points to create wires (ESC to cancel)." << std::endl;
    } else {
        is_creating_wire_from_pin = false;
        wire_start_pin.reset();
        std::cout << "Wire mode disabled." << std::endl;
    }
}

void GuiApplication::toggleProbeMode() {
    is_probe_mode = !is_probe_mode;
    if (is_probe_mode) {
        is_drawing_wire = false;
        placing_component_type = "";
        
        // Reset to default cursor
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
        
        // Show probe panel if analysis has been completed
        if (probe_panel && analysis_completed && !latest_tran_results.empty()) {
            probe_panel->toggleVisibility();
            ErrorManager::info("[Probe] Probe mode enabled - Select signals from the probe panel");
            std::cout << "Probe mode enabled. Probe panel opened for signal selection." << std::endl;
    } else {
            ErrorManager::info("[Probe] Probe mode enabled - Run analysis first to see available signals");
            std::cout << "Probe mode enabled. Run transient analysis first to see available signals." << std::endl;
        }
    } else {
        // Reset to default cursor
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
        
        // Hide probe panel when exiting probe mode
        if (probe_panel && probe_panel->isVisible()) {
            probe_panel->toggleVisibility();
        }
        
        ErrorManager::info("[Probe] Probe mode disabled");
        std::cout << "Probe mode disabled." << std::endl;
    }
}

void GuiApplication::setProbeType(ProbeType type) {
    current_probe_type = type;
    std::cout << "Probe type set to: " << (type == ProbeType::VOLTAGE ? "Voltage" : "Current") << std::endl;
}

void GuiApplication::clearAllProbes() {
    selected_signals.clear();
    std::cout << "All probes cleared from plot" << std::endl;
    
    // Update oscilloscope to show empty selection
    for (auto& el : ui_elements) {
        if (auto* scope = dynamic_cast<Oscilloscope*>(el.get())) {
            scope->setSelectedSignals(selected_signals);
            break;
        }
    }
}

void GuiApplication::selectComponentToPlace(const std::string& type) {
    placing_component_type = type;
    resetPlacementState();
    is_drawing_wire = false;
    is_probe_mode = false; // Disable probe mode when placing components

    // Reset cursor to default when exiting probe mode
    SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));

    std::cout << "Selected component type: " << type << std::endl;
    if (component_selector) component_selector->toggleVisibility();
}

void GuiApplication::resetPlacementState() {
    placement_step = 0;
    node1 = node2 = ctrl_node1 = ctrl_node2 = "";
}

void GuiApplication::pushUndoSnapshot() {
    try {
        // Serialize circuit to JSON string snapshot
        const std::string path = "__undo_snapshot.json";
        ProjectSerializer::save(*circuit, path);
        // Read file back into string
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        std::stringstream buffer; buffer << ifs.rdbuf();
        undo_stack.push_back(buffer.str());
        redo_stack.clear(); // Clear redo stack when new action is performed
    } catch (...) {}
}

void GuiApplication::applySnapshot(const std::string& snapshot) {
    try {
        if (snapshot.empty()) {
            std::cout << "Error: Empty snapshot provided" << std::endl;
            return;
        }

        const std::string path = "__apply_snapshot.json";
        std::ofstream ofs(path, std::ios::out | std::ios::binary);
        if (!ofs.is_open()) {
            std::cout << "Error: Failed to open snapshot file for writing" << std::endl;
            return;
        }

        ofs << snapshot;
        ofs.close();

        // Clear the circuit before loading
        if (circuit) {
            circuit->clear();
        }

        ProjectSerializer::load(*circuit, path);

        if (schematic_view) {
            schematic_view->rebuildWiresFromCircuit();
        }

        std::cout << "Snapshot applied successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error applying snapshot: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unknown error applying snapshot" << std::endl;
    }
}

void GuiApplication::startWireFromPin(std::shared_ptr<Pin> pin) {
    if (pin) {
        wire_start_pin = pin;
        is_creating_wire_from_pin = true;
        wire_draw_start_pos = pin->getPosition();
        std::cout << "Started wire from pin " << pin->getFullId() << std::endl;
    }
}

void GuiApplication::finishWireToPin(std::shared_ptr<Pin> pin) {
    if (wire_start_pin && pin && wire_start_pin != pin) {
        // Create wire between the two pins
        if (schematic_view) {
            schematic_view->createWire(wire_start_pin, pin);
            std::cout << "Created wire from " << wire_start_pin->getFullId() 
                      << " to " << pin->getFullId() << std::endl;
        }
    }
    
    // Reset wire creation state
    wire_start_pin.reset();
    is_creating_wire_from_pin = false;
}

void GuiApplication::cancelWireCreation() {
    wire_start_pin.reset();
    is_creating_wire_from_pin = false;
}

void GuiApplication::handleProbeHover(int mouse_x, int mouse_y) {
    if (!schematic_view || !is_probe_mode) return;
    
    static std::string last_hover_msg = "";
    std::string hover_msg = "";
    
    // Check if hovering over a node name (for voltage probes)
    std::string node_name_at_cursor = schematic_view->getNodeNameAt(mouse_x, mouse_y);
    
    // Check if hovering over a node (for voltage probes)
    std::string node_at_cursor = schematic_view->getNodeAt(mouse_x, mouse_y);
    
    if (!node_name_at_cursor.empty()) {
        if (current_probe_type == ProbeType::VOLTAGE) {
            hover_msg = "[Probe] Click to measure voltage at node " + node_name_at_cursor;
        } else {
            hover_msg = "[Probe] Switch to voltage mode to probe this node";
        }
    } else if (!node_at_cursor.empty()) {
        if (current_probe_type == ProbeType::VOLTAGE) {
            hover_msg = "[Probe] Click to measure voltage at node " + node_at_cursor;
        } else {
            hover_msg = "[Probe] Switch to voltage mode to probe this node";
        }
    } else {
        if (current_probe_type == ProbeType::CURRENT) {
            hover_msg = "[Probe] Click on component to measure current";
        } else {
            hover_msg = "[Probe] Hover over nodes or wires for voltage probes";
        }
    }
    
    // Only show message if it changed to avoid spam
    if (hover_msg != last_hover_msg) {
        ErrorManager::info(hover_msg);
        last_hover_msg = hover_msg;
    }
    
    bool can_place_voltage_probe = !node_at_cursor.empty() || !node_name_at_cursor.empty();
    bool can_place_current_probe = false;
    
    // Check if hovering over a component (for current probes)
    for (const auto& element : circuit->getElements()) {
        if (!element) continue;
        
        // Get component position and check if mouse is within bounds
        SDL_Point elem_pos;
        if (element->getType() == "Ground") {
            elem_pos = schematic_view->getNodePosition(element->getNode1Id());
        } else {
            // For two-terminal components, check center position
            SDL_Point pos1 = schematic_view->getNodePosition(element->getNode1Id());
            SDL_Point pos2 = schematic_view->getNodePosition(element->getNode2Id());
            elem_pos.x = (pos1.x + pos2.x) / 2;
            elem_pos.y = (pos1.y + pos2.y) / 2;
        }
        
        // Check if mouse is within component bounds (50x50 pixel area)
        if (mouse_x >= elem_pos.x - 25 && mouse_x <= elem_pos.x + 25 && 
            mouse_y >= elem_pos.y - 25 && mouse_y <= elem_pos.y + 25) {
            can_place_current_probe = true;
            break;
        }
    }
    
    // Set appropriate cursor based on probe type and hover target
    if (current_probe_type == ProbeType::VOLTAGE && can_place_voltage_probe) {
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
    } else if (current_probe_type == ProbeType::CURRENT && can_place_current_probe) {
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
    } else {
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
    }
}

void GuiApplication::handleSchematicClick(const SDL_Event& event) {
    if (event.type != SDL_MOUSEBUTTONDOWN) return;
    if (!schematic_view) return;
    // If settings panel is visible and click is inside it, ignore schematic click
    if (settings_panel && settings_panel->isVisible()) {
        int mx = event.button.x, my = event.button.y;
        if (settings_panel->contains(mx, my)) return;
    }
    
    // Handle probe mode clicking
    if (is_probe_mode) {
        int mx = event.button.x, my = event.button.y;
        
        // Check if clicking on a node name (for voltage probes)
        std::string node_name_at_cursor = schematic_view->getNodeNameAt(mx, my);
        
        // Check if clicking on a node (for voltage probes)
        std::string node_at_cursor = schematic_view->getNodeAt(mx, my);
        
        if (current_probe_type == ProbeType::VOLTAGE) {
            std::string target_node = "";
            if (!node_name_at_cursor.empty()) {
                target_node = node_name_at_cursor;
            } else if (!node_at_cursor.empty()) {
                target_node = node_at_cursor;
            }
            
            if (!target_node.empty()) {
                // Add voltage probe for this node
                std::string signal_name = "V(" + target_node + ")";
                probe_panel->addSignal(signal_name);
                ErrorManager::info("Added voltage probe for " + signal_name);
                return;
            }
        } else if (current_probe_type == ProbeType::CURRENT) {
            // Check if clicking on a component (for current probes)
            for (const auto& element : circuit->getElements()) {
                if (!element) continue;
                
                // Get component position and check if mouse is within bounds
                SDL_Point elem_pos;
                if (element->getType() == "Ground") {
                    elem_pos = schematic_view->getNodePosition(element->getNode1Id());
                } else {
                    // For two-terminal components, check center position
                    SDL_Point pos1 = schematic_view->getNodePosition(element->getNode1Id());
                    SDL_Point pos2 = schematic_view->getNodePosition(element->getNode2Id());
                    elem_pos.x = (pos1.x + pos2.x) / 2;
                    elem_pos.y = (pos1.y + pos2.y) / 2;
                }
                
                // Check if mouse is within component bounds (50x50 pixel area)
                if (mx >= elem_pos.x - 25 && mx <= elem_pos.x + 25 && 
                    my >= elem_pos.y - 25 && my <= elem_pos.y + 25) {
                    
                    // Add current probe for this component
                    std::string signal_name = "I(" + element->getName() + ")";
                    probe_panel->addSignal(signal_name);
                    ErrorManager::info("Added current probe for " + signal_name);
                    return;
                }
            }
        }
        return; // Don't process other logic in probe mode
    }
    
    // Handle right-click for component editing
    if (event.button.button == SDL_BUTTON_RIGHT) {
        int mx = event.button.x, my = event.button.y;
        
        // Check if we clicked on a component
        for (const auto& element : circuit->getElements()) {
            if (!element) continue;
            
            // Get component position (simplified - you might need more precise hit detection)
            SDL_Point elem_pos;
            if (element->getType() == "Ground") {
                elem_pos = schematic_view->getNodePosition(element->getNode1Id());
            } else {
                // For two-terminal components, check center position
                SDL_Point pos1 = schematic_view->getNodePosition(element->getNode1Id());
                SDL_Point pos2 = schematic_view->getNodePosition(element->getNode2Id());
                elem_pos.x = (pos1.x + pos2.x) / 2;
                elem_pos.y = (pos1.y + pos2.y) / 2;
            }
            
            // Check if click is within component bounds (50x50 pixel area)
            if (mx >= elem_pos.x - 25 && mx <= elem_pos.x + 25 && 
                my >= elem_pos.y - 25 && my <= elem_pos.y + 25) {
                
                std::cout << "Right-clicked on component: " << element->getName() << " (" << element->getType() << ")" << std::endl;
                
                if (edit_dialog) {
                    edit_dialog->setTargetElement(element.get());
                    edit_dialog->show();
                }
                return;
            }
        }
        return; // Don't process other logic for right-clicks
    }

    // Handle wire mode clicks
    if (is_drawing_wire && event.button.button == SDL_BUTTON_LEFT) {
        std::cout << "Wire mode: Mouse clicked at (" << event.button.x << ", " << event.button.y << ")" << std::endl;
        
        // First check if clicking on a pin
        auto clicked_pin = schematic_view->getPinAt(event.button.x, event.button.y);
        std::cout << "Found pin at click: " << (clicked_pin ? clicked_pin->getFullId() : "none") << std::endl;
        if (clicked_pin) {
            if (is_creating_wire_from_pin) {
                // Finish wire to this pin
                finishWireToPin(clicked_pin);
                std::cout << "Circuit graph updated after wire creation" << std::endl;
            } else {
                // Start wire from this pin
                startWireFromPin(clicked_pin);
            }
            return;
        }

        // If not clicking on a pin, check if clicking near a pin (for easier connection)
        auto near_pin = schematic_view->getPinNear(event.button.x, event.button.y, 30); // Larger radius for easier clicking
        std::cout << "Found nearby pin: " << (near_pin ? near_pin->getFullId() : "none") << std::endl;
        if (near_pin) {
            if (is_creating_wire_from_pin) {
                finishWireToPin(near_pin);
            } else {
                startWireFromPin(near_pin);
            }
            return;
        }

        // If no pin nearby, create wire points on grid for multi-segment wires
        SDL_Point grid_pos = schematic_view->snapToGrid(event.button.x, event.button.y);
        std::string grid_node = schematic_view->getNodeAt(grid_pos.x, grid_pos.y);
        
        if (!grid_node.empty()) {
            if (is_creating_wire_from_pin && wire_start_pin) {
                // Create a wire segment from pin to grid point
                auto grid_pin = std::make_shared<Pin>(grid_node + ".virtual", "GRID", 1, grid_pos);
                grid_pin->setNodeId(grid_node);
                
                pushUndoSnapshot();
                schematic_view->createWire(wire_start_pin, grid_pin);
                std::cout << "Created wire from pin " << wire_start_pin->getFullId() << " to grid point: " << grid_node << std::endl;
                
                // Reset wire creation state
                wire_start_pin.reset();
                is_creating_wire_from_pin = false;
            } else if (!is_creating_wire_from_pin) {
                // Start wire from grid point
                wire_draw_start_pos = grid_pos;
                wire_start_node = grid_node;
                is_wire_drag_active = true;
                std::cout << "Started wire from grid point: " << grid_node << std::endl;
            }
        }
        return; // Don't process component placement when in wire mode
    }

    std::string clicked_node = schematic_view->getNodeAt(event.button.x, event.button.y);
    if (clicked_node.empty()) return;

    if (is_creating_subcircuit) {
        if (placement_step == 0) {
            node1 = clicked_node;
            placement_step = 1;
            std::cout << "First port selected: " << node1 << ". Select the second port." << std::endl;
        } else {
            node2 = clicked_node;
            std::cout << "Second port selected: " << node2 << ". Saving subcircuit." << std::endl;

            auto subcircuit_circuit = std::make_unique<Circuit>();
            // This is a simplified implementation. A full implementation would copy the relevant elements
            // and remap the nodes.
            ProjectSerializer::save(*circuit, "subcircuit.json");
            std::cout << "Subcircuit saved to subcircuit.json" << std::endl;

            is_creating_subcircuit = false;
            resetPlacementState();
        }
        return;
    }

    if(is_labeling_node) {
        // For simplicity, we'll use a hardcoded label for now.
        // A full implementation would prompt the user for the label text.
        circuit->addNodeLabel(clicked_node, "VCC");
        std::cout << "Labeled node " << clicked_node << " as VCC." << std::endl;
        is_labeling_node = false;
        return;
    }

    if (placing_component_type.empty() || placing_component_type == "Wire") return;

    // Snap to grid for component placement
    SDL_Point snap_pos = schematic_view->snapToGrid(event.button.x, event.button.y);
    std::string snapped_node = schematic_view->getNodeAt(snap_pos.x, snap_pos.y);

    bool is_dep_source = (placing_component_type == "VCVS" || placing_component_type == "VCCS" || placing_component_type == "CCVS" || placing_component_type == "CCCS");

    if (!is_dep_source) {
        // Single-click placement: automatically determine pin positions
        try {
            std::string name = placing_component_type.substr(0, 1) + std::to_string(circuit->getElements().size() + 1);
            
            // Calculate pin positions based on component orientation
            // For horizontal components: pins at left and right
            // Get grid coordinates for clicked position  
            const int GRID_SIZE = 15; // Use the same grid size as defined in GUI.h
            int grid_x = (snap_pos.x - schematic_view->getViewArea().x) / GRID_SIZE;
            int grid_y = (snap_pos.y - schematic_view->getViewArea().y) / GRID_SIZE;
            
            if (placing_component_type == "Ground") {
                // Ground only needs one node
                node1 = snapped_node;
                node2 = "";
            } else {
                // Two-terminal components: place pins at both ends
                std::string left_node_id = "N" + std::to_string(grid_y * 100 + grid_x - 1);     // Left pin
                std::string right_node_id = "N" + std::to_string(grid_y * 100 + grid_x + 1);   // Right pin
                
                // Use existing nodes if they exist, otherwise use the calculated IDs
                node1 = circuit->hasNode(left_node_id) ? left_node_id : left_node_id;
                node2 = circuit->hasNode(right_node_id) ? right_node_id : right_node_id;
            }
            
            std::cout << "Single-click placing: " << name << " of type " << placing_component_type << std::endl;
            std::cout << "Pin positions: node1='" << node1 << "', node2='" << node2 << "'" << std::endl;
            
            pushUndoSnapshot();
            if (placing_component_type == "Resistor") circuit->addElement(std::make_unique<Resistor>(name, node1, node2, 1000.0));
            else if (placing_component_type == "Capacitor") circuit->addElement(std::make_unique<Capacitor>(name, node1, node2, 1e-6));
            else if (placing_component_type == "Inductor") circuit->addElement(std::make_unique<Inductor>(name, node1, node2, 1e-3));
            else if (placing_component_type == "IndependentVoltageSource") circuit->addElement(std::make_unique<IndependentVoltageSource>(name, node1, node2, 5.0));
            else if (placing_component_type == "PulseVoltageSource") circuit->addElement(std::make_unique<PulseVoltageSource>(name, node1, node2, 0.0, 5.0, 1e-3, 1e-4, 1e-4, 3e-3, 8e-3));
            else if (placing_component_type == "SinusoidalVoltageSource") circuit->addElement(std::make_unique<SinusoidalVoltageSource>(name, node1, node2, 0, 5, 1000));
            else if (placing_component_type == "WaveformVoltageSource") {
                // Create default waveform (triangle wave)
                std::vector<double> waveform = {0.0, 2.5, 5.0, 2.5, 0.0, -2.5, -5.0, -2.5};
                circuit->addElement(std::make_unique<WaveformVoltageSource>(name, node1, node2, waveform, 8000.0, 1e-3));
            }
            else if (placing_component_type == "PhaseVoltageSource") circuit->addElement(std::make_unique<PhaseVoltageSource>(name, node1, node2, 5.0, 2*3.14159*1000, 0.0));
            else if (placing_component_type == "IndependentCurrentSource") circuit->addElement(std::make_unique<IndependentCurrentSource>(name, node1, node2, 1.0));
            else if (placing_component_type == "PulseCurrentSource") circuit->addElement(std::make_unique<PulseCurrentSource>(name, node1, node2, 0, 1e-3, 1e-3, 1e-4, 1e-4, 2e-3, 5e-3));
            else if (placing_component_type == "Diode") circuit->addElement(std::make_unique<Diode>(name, node1, node2, "D"));
            else if (placing_component_type == "Ground") {
                circuit->addElement(std::make_unique<Ground>("GND", node1));
                circuit->setGroundNode(node1);  // Set this node as the ground node
                std::cout << "Ground node set to: " << node1 << std::endl;
            }
            else {
                std::cout << "WARNING: Unknown component type: " << placing_component_type << std::endl;
            }
            
            std::cout << "Component " << name << " placed successfully with single click!" << std::endl;
            ErrorManager::info("Component " + name + " placed successfully");
            
            // Update pins after adding component
            if (schematic_view) {
                schematic_view->updatePinPositions();
            }
            
            // Open configuration dialog for configurable components
            if (placing_component_type == "PulseVoltageSource" || 
                placing_component_type == "PulseCurrentSource" ||
                placing_component_type == "SinusoidalVoltageSource") {
                
                if (edit_dialog) {
                    edit_dialog->setTargetElement(circuit->getElements().back().get());
                    edit_dialog->show();
                    ErrorManager::info("[Component] Configuration dialog opened for " + placing_component_type);
                }
            }
            
            placing_component_type = "";
            resetPlacementState();
            
        } catch (const std::exception& e) { 
            std::cout << "ERROR creating component: " << e.what() << std::endl;
            ErrorManager::displayError(e.what()); 
        }
    } else { // Dependent source placement
        if (placement_step == 0) { node1 = clicked_node; placement_step = 1; }
        else if (placement_step == 1) { node2 = clicked_node; placement_step = 2; }
        else if (placement_step == 2) { ctrl_node1 = clicked_node; placement_step = 3; }
        else {
            ctrl_node2 = clicked_node;
            try {
                pushUndoSnapshot();
                std::string name = placing_component_type.substr(0, 1) + std::to_string(circuit->getElements().size() + 1);
                if (placing_component_type == "VCVS") circuit->addElement(std::make_unique<VoltageControlledVoltageSource>(name, node1, node2, ctrl_node1, ctrl_node2, 2.0));
                else if (placing_component_type == "VCCS") circuit->addElement(std::make_unique<VoltageControlledCurrentSource>(name, node1, node2, ctrl_node1, ctrl_node2, 0.01));
                else if (placing_component_type == "CCVS") circuit->addElement(std::make_unique<CurrentControlledVoltageSource>(name, node1, node2, "Vcontrol", 10.0));
                else if (placing_component_type == "CCCS") circuit->addElement(std::make_unique<CurrentControlledCurrentSource>(name, node1, node2, "Vcontrol", 2.0));
                // Update pins after adding dependent source
                if (schematic_view) {
                    schematic_view->updatePinPositions();
                }
            } catch (const std::exception& e) { ErrorManager::displayError(e.what()); }
            placing_component_type = "";
            resetPlacementState();
        }
    }
}

void GuiApplication::renderComponentPreview(SDL_Renderer* renderer) {
    if (placing_component_type.empty() || placing_component_type == "Wire") return;
    
    // Get the texture for the component being placed
    SDL_Texture* texture = nullptr;
    if (placing_component_type == "Resistor") texture = component_textures["resistor.png"];
    else if (placing_component_type == "Capacitor") texture = component_textures["capacitor.png"];
    else if (placing_component_type == "Inductor") texture = component_textures["inductor.png"];
    else if (placing_component_type == "Diode") texture = component_textures["diode.png"];
    else if (placing_component_type == "Ground") texture = component_textures["gnd.png"];
    else if (placing_component_type == "IndependentVoltageSource") texture = component_textures["dc_v_source.png"];
    else if (placing_component_type == "PulseVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (placing_component_type == "SinusoidalVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (placing_component_type == "ACVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (placing_component_type == "WaveformVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (placing_component_type == "PhaseVoltageSource") texture = component_textures["ac_v_source.png"];
    else if (placing_component_type == "IndependentCurrentSource") texture = component_textures["dc_c_source.png"];
    else if (placing_component_type == "PulseCurrentSource") texture = component_textures["ac_c_source.png"];

    else if (placing_component_type == "VoltageControlledVoltageSource") texture = component_textures["dep_v_source.png"];
    else if (placing_component_type == "VoltageControlledCurrentSource") texture = component_textures["dep_c_source.png"];
    else if (placing_component_type == "CurrentControlledVoltageSource") texture = component_textures["dep_v_source.png"];
    else if (placing_component_type == "CurrentControlledCurrentSource") texture = component_textures["dep_c_source.png"];
    
    if (!texture) return;
    
    // Get original dimensions
    int original_w, original_h;
    SDL_QueryTexture(texture, NULL, NULL, &original_w, &original_h);
    
    // Calculate preview size (smaller than actual component)
    const int PREVIEW_SIZE = 60;
    const int MIN_PREVIEW_SIZE = 30;
    
    double aspect_ratio = static_cast<double>(original_w) / original_h;
    int preview_w, preview_h;
    
    if (aspect_ratio > 1.0) {
        preview_w = PREVIEW_SIZE;
        preview_h = static_cast<int>(PREVIEW_SIZE / aspect_ratio);
    } else {
        preview_h = PREVIEW_SIZE;
        preview_w = static_cast<int>(PREVIEW_SIZE * aspect_ratio);
    }
    
    preview_w = std::max(preview_w, MIN_PREVIEW_SIZE);
    preview_h = std::max(preview_h, MIN_PREVIEW_SIZE);
    
    // Position preview at mouse cursor
    int preview_x = current_mouse_pos.x - preview_w / 2;
    int preview_y = current_mouse_pos.y - preview_h / 2;
    
    SDL_Rect preview_rect = { preview_x, preview_y, preview_w, preview_h };
    
    // Draw preview with transparency effect
    SDL_SetTextureAlphaMod(texture, 180); // Semi-transparent
    SDL_RenderCopy(renderer, texture, nullptr, &preview_rect);
    SDL_SetTextureAlphaMod(texture, 255); // Reset alpha
    
    // Draw preview border
    SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255); // Blue border
    SDL_RenderDrawRect(renderer, &preview_rect);
    
    // Draw placement instructions
    if (font) {
        std::string instruction;
        if (placement_step == 0) {
            instruction = "Click to place first node";
        } else if (placement_step == 1) {
            instruction = "Click to place second node";
        } else if (placement_step == 2) {
            instruction = "Click to place control node 1";
        } else if (placement_step == 3) {
            instruction = "Click to place control node 2";
        }
        
        SDL_Surface* surface = TTF_RenderText_Blended(font, instruction.c_str(), {0, 0, 0, 255});
        if (surface) {
            SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect text_rect = { current_mouse_pos.x + 20, current_mouse_pos.y - 10, surface->w, surface->h };
            SDL_RenderCopy(renderer, text_texture, nullptr, &text_rect);
            SDL_DestroyTexture(text_texture);
            SDL_FreeSurface(surface);
        }
    }
}


