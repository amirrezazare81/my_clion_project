#include "Gui.h"
#include "ErrorManager.h"
#include "Analyzers.h"
#include "Solvers.h"
#include "Element.h"
#include "ProjectSerializer.h"
#include "SignalProcessor.h"
#include <SDL_image.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <utility>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <fstream>

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
    
    // Transient Analysis inputs
    tran_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 55, 100, 25, font, "1e-5"));    // Time Step  
    tran_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 95, 100, 25, font, "10e-3"));   // Stop Time - increased for pulse visibility
    
    // AC Analysis inputs  
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 155, 100, 25, font, "AC1"));      // Source
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 195, 100, 25, font, "1"));        // Start Freq
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 235, 100, 25, font, "100k"));     // Stop Freq
    ac_inputs.push_back(std::make_unique<InputBox>(x + 120, y + 275, 100, 25, font, "100"));      // Points
}

void SimulationSettingsPanel::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;
    for (auto& box : tran_inputs) box->handleEvent(event);
    for (auto& box : ac_inputs) box->handleEvent(event);
}

void SimulationSettingsPanel::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    
    // Draw panel background
    SDL_SetRenderDrawColor(renderer, 240, 240, 245, 240);
    SDL_RenderFillRect(renderer, &panel_rect);
    SDL_SetRenderDrawColor(renderer, 150, 150, 160, 255);
    SDL_RenderDrawRect(renderer, &panel_rect);
    
    // Title
    if (tran_inputs[0] && tran_inputs[0]->getFont()) {
        TTF_Font* font = tran_inputs[0]->getFont();
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Simulation Settings", {20,20,20,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 5, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        // Transient Analysis Section
        surface = TTF_RenderText_Blended(font, "Transient Analysis:", {40,40,40,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 30, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        surface = TTF_RenderText_Blended(font, "Time Step (s):", {60,60,60,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 55, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        surface = TTF_RenderText_Blended(font, "Stop Time (s):", {60,60,60,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 95, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        // AC Analysis Section
        surface = TTF_RenderText_Blended(font, "AC Analysis:", {40,40,40,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 130, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        surface = TTF_RenderText_Blended(font, "Source:", {60,60,60,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 155, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        surface = TTF_RenderText_Blended(font, "Start Freq (Hz):", {60,60,60,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 195, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        surface = TTF_RenderText_Blended(font, "Stop Freq (Hz):", {60,60,60,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 235, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        
        surface = TTF_RenderText_Blended(font, "Points:", {60,60,60,255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { panel_rect.x + 10, panel_rect.y + 275, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
    
    // Render input boxes
    for (auto& box : tran_inputs) box->render(renderer);
    for (auto& box : ac_inputs) box->render(renderer);
}

void SimulationSettingsPanel::toggleVisibility() { is_visible = !is_visible; }
double SimulationSettingsPanel::getTranTStep() const { try { return std::stod(tran_inputs[0]->getText()); } catch(...) { return 1e-5; } }
double SimulationSettingsPanel::getTranTStop() const { try { return std::stod(tran_inputs[1]->getText()); } catch(...) { return 5e-3; } }
std::string SimulationSettingsPanel::getACSource() const { return ac_inputs[0]->getText(); }
double SimulationSettingsPanel::getACStartFreq() const { try { return std::stod(ac_inputs[1]->getText()); } catch(...) { return 1.0; } }
double SimulationSettingsPanel::getACStopFreq() const { try { return std::stod(ac_inputs[2]->getText()); } catch(...) { return 100e3; } }
int SimulationSettingsPanel::getACPoints() const { try { return std::stoi(ac_inputs[3]->getText()); } catch(...) { return 100; } }

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
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "AC Source", font, [on_select](){ on_select("ACVoltageSource"); }));
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
    
    // Draw all node names when show_node_names is enabled (after analysis)
    if (show_node_names) {
        std::set<std::string> drawn_nodes; // Avoid drawing duplicate nodes
        std::map<std::pair<int, int>, int> grid_usage; // Track grid position usage for smart spacing
        
        for (const auto& element : circuit_backend.getElements()) {
            if (!element) continue;
            
            // Draw node names for all nodes used by elements
            std::string node1 = element->getNode1Id();
            std::string node2 = element->getNode2Id();
            
            auto drawNodeWithSmartPlacement = [&](const std::string& node_id) {
                if (node_id.empty() || drawn_nodes.find(node_id) != drawn_nodes.end()) return;
                
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
    pins.clear();
    
    // Rebuild pins but reuse existing instances to preserve connection state
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
        std::shared_ptr<Pin> pin1, pin2;
        
        auto it1 = pin_index.find(key1);
        if (it1 != pin_index.end() && it1->second) {
            pin1 = it1->second;
            pin1->setPosition(pin1_pos);
        } else {
            pin1 = std::make_shared<Pin>(key1, elem_name, 1, pin1_pos);
            pin_index[key1] = pin1;
        }
        pin1->setNodeId(node1_id);
        pins.push_back(pin1);
        
        if (elem_type != "Ground" && !node2_id.empty()) {
            auto it2 = pin_index.find(key2);
            if (it2 != pin_index.end() && it2->second) {
                pin2 = it2->second;
                pin2->setPosition(pin2_pos);
            } else {
                pin2 = std::make_shared<Pin>(key2, elem_name, 2, pin2_pos);
                pin_index[key2] = pin2;
            }
            pin2->setNodeId(node2_id);
            pins.push_back(pin2);
        }
    }
    
    std::cout << "Updated pin positions. Total pins: " << pins.size() << std::endl;
    
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
    for (const auto& pin : pins) {
        if (pin && pin->isAtPosition(x, y)) {
            return pin;
        }
    }
    return nullptr;
}

std::shared_ptr<Pin> SchematicView::getPinNear(int x, int y, int hover_radius) const {
    for (const auto& pin : pins) {
        if (pin && pin->isNearPosition(x, y, hover_radius)) {
            return pin;
        }
    }
    return nullptr;
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

// --- PlotView Implementation ---
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
    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};
    int i = 0;
    for (const auto& pair : analysis_results) {
        if (pair.first.find("V(") != std::string::npos) {
            signals.push_back({pair.first, pair.second, colors[i++ % colors.size()]});
        }
    }
    autoZoom();
    updateCursorManager();
}

void PlotView::setDataFiltered(const std::vector<double>& time_points, const std::map<std::string, std::vector<double>>& analysis_results, const std::set<std::string>& selected_names) {
    current_mode = PlotMode::TRANSIENT;
    signals.clear();
    x_values = time_points;
    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};
    int i = 0;
    for (const auto& pair : analysis_results) {
        if (!selected_names.empty() && selected_names.find(pair.first) == selected_names.end()) continue;
        signals.push_back({pair.first, pair.second, colors[i++ % colors.size()]});
    }
    {
        std::stringstream ss; ss << "PlotView::setDataFiltered -> x:" << x_values.size() << ", traces:" << signals.size();
        std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());
    }
    autoZoom();
    updateCursorManager();
}

void PlotView::setDataAC(const std::vector<double>& freq_points, const std::map<std::string, std::vector<Complex>>& ac_results) {
    current_mode = PlotMode::AC_MAGNITUDE;
    signals.clear();
    x_values = freq_points;
    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};
    int i = 0;
    for (const auto& pair : ac_results) {
        if (pair.first.find("V(") != std::string::npos) {
            std::vector<double> magnitudes;
            for(const auto& val : pair.second) {
                magnitudes.push_back(std::abs(val));
            }
            signals.push_back({pair.first, magnitudes, colors[i++ % colors.size()]});
        }
    }
    autoZoom();
}

void PlotView::setDataPhase(const std::vector<double>& phase_points, const std::map<std::string, std::vector<Complex>>& phase_results) {
    current_mode = PlotMode::PHASE_MAGNITUDE;
    signals.clear();
    x_values = phase_points;
    std::vector<SDL_Color> colors = {{100, 255, 100, 255}, {255, 100, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255}, {100, 255, 255, 255}};
    int i = 0;
    for (const auto& pair : phase_results) {
        if (pair.first.find("V(") != std::string::npos) {
            std::vector<double> magnitudes;
            for(const auto& val : pair.second) {
                magnitudes.push_back(std::abs(val));
            }
            signals.push_back({pair.first, magnitudes, colors[i++ % colors.size()]});
        }
    }
    autoZoom();
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
    
    return { view_area.x + label_margin_left + static_cast<int>((world_x + offset_x) * scale_x),
             view_area.y + view_area.h - label_margin_bottom - static_cast<int>((world_y + offset_y) * scale_y) };
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

    if (!x_values.empty() && !signals.empty()) {
        for (const auto& sig : signals) {
            if (sig.y_values.size() < 2) continue;
            if (sig.y_values.size() != x_values.size()) {
                // Guard against mismatched lengths
                size_t n = std::min(x_values.size(), sig.y_values.size());
                SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, sig.color.a);
                for (size_t i = 0; i + 1 < n; ++i) {
                    SDL_Point p1 = toScreenCoords(x_values[i], sig.y_values[i]);
                    SDL_Point p2 = toScreenCoords(x_values[i+1], sig.y_values[i+1]);
                    SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
                }
                continue;
            }
            SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, sig.color.a);
            for (size_t i = 0; i < x_values.size() - 1; ++i) {
                SDL_Point p1 = toScreenCoords(x_values[i], sig.y_values[i]);
                SDL_Point p2 = toScreenCoords(x_values[i+1], sig.y_values[i+1]);
                SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
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
            
        } else if (type == "ACVoltageSource") {
            auto* ac = static_cast<ACVoltageSource*>(element);
            
            dialog_rect.h = 250;
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 50, 150, 25, font, std::to_string(ac->getMagnitude())));
            param_labels.push_back("Magnitude (V):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 85, 150, 25, font, std::to_string(ac->getFrequency())));
            param_labels.push_back("Frequency (Hz):");
            
            param_inputs.push_back(std::make_unique<InputBox>(dialog_rect.x + 120, dialog_rect.y + 120, 150, 25, font, std::to_string(ac->getPhase())));
            param_labels.push_back("Phase (degrees):");
            

            
        } else {
            // Generic single-value input for other components
            dialog_rect.h = 150;
            current_value = std::to_string(element->getValue());
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
                            pulse->setV1(std::stod(param_inputs[0]->getText()));
                            pulse->setV2(std::stod(param_inputs[1]->getText()));
                            pulse->setTd(std::stod(param_inputs[2]->getText()));
                            pulse->setTr(std::stod(param_inputs[3]->getText()));
                            pulse->setTf(std::stod(param_inputs[4]->getText()));
                            pulse->setPw(std::stod(param_inputs[5]->getText()));
                            pulse->setPer(std::stod(param_inputs[6]->getText()));
                            std::cout << "Updated PulseVoltageSource parameters" << std::endl;
                            
                        } else if (type == "PulseCurrentSource") {
                            auto* pulse = static_cast<PulseCurrentSource*>(target_element);
                            pulse->setI1(std::stod(param_inputs[0]->getText()));
                            pulse->setI2(std::stod(param_inputs[1]->getText()));
                            pulse->setTd(std::stod(param_inputs[2]->getText()));
                            pulse->setTr(std::stod(param_inputs[3]->getText()));
                            pulse->setTf(std::stod(param_inputs[4]->getText()));
                            pulse->setPw(std::stod(param_inputs[5]->getText()));
                            pulse->setPer(std::stod(param_inputs[6]->getText()));
                            std::cout << "Updated PulseCurrentSource parameters" << std::endl;
                            
                        } else if (type == "ACVoltageSource") {
                            auto* ac = static_cast<ACVoltageSource*>(target_element);
                            ac->setMagnitude(std::stod(param_inputs[0]->getText()));
                            ac->setFrequency(std::stod(param_inputs[1]->getText()));
                            ac->setPhase(std::stod(param_inputs[2]->getText()));
                            std::cout << "Updated ACVoltageSource parameters" << std::endl;
                            

                        }
                    } else if (value_input) {
                        // Handle simple components with single value
                        double new_value = std::stod(value_input->getText());
                        target_element->setValue(new_value);
                        std::cout << "Updated " << target_element->getName() << " value to " << new_value << std::endl;
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

        // Toggle selection by clicking list items
        int list_top = panel_rect.y + 40;
        if (my >= list_top && my <= apply_btn.y - 10) {
            int line_h = 22;
            int idx = (my - list_top) / line_h;
            if (idx >= 0 && idx < (int)available_signals.size()) {
                const std::string& name = available_signals[idx];
                if (selected.count(name)) selected.erase(name);
                else selected.insert(name);
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

    // List signals
    int y = panel_rect.y + 40;
    for (const auto& name : available_signals) {
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
        y += 22;
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

    ui_elements.push_back(std::make_unique<PlotView>(sidebar_width, menu_height + schematic_height, screen_width - sidebar_width, plot_height, font));

    // Component selector for sources and components
    auto selector = std::make_unique<ComponentSelector>(screen_width/6, screen_height/4, 200, 400, font, [this](const std::string& type){ this->selectComponentToPlace(type); });
    component_selector = selector.get();
    ui_elements.push_back(std::move(selector));

    auto settings = std::make_unique<SimulationSettingsPanel>(screen_width/4, screen_height/6, 400, 320, font);
    settings_panel = settings.get();
    ui_elements.push_back(std::move(settings));

    // Probe panel
    auto probe = std::make_unique<ProbePanel>(screen_width/3, screen_height/6, 400, 320, font, [this](const std::set<std::string>& selected){
        selected_signals = selected;
        {
            std::stringstream ss; ss << "[Probe] Apply clicked. Selected signals:";
            for (const auto& s : selected_signals) ss << " " << s;
            std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());
        }
        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                if (!latest_time_points.empty() && !latest_tran_results.empty()) {
                    plot->setDataFiltered(latest_time_points, latest_tran_results, selected_signals);
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
            } catch (...) {}
            auto snap = undo_stack.back(); undo_stack.pop_back();
            applySnapshot(snap);
        }
    }));
    current_x += button_width + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, button_width, button_height, "Redo", font, [this]() {
        if (!redo_stack.empty()) {
            pushUndoSnapshot();
            auto snap = redo_stack.back(); redo_stack.pop_back();
            applySnapshot(snap);
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
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "TRAN", font, [this]() { this->onRunSimulationClicked(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "AC", font, [this]() { this->onRunACAnalysisClicked(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "DC", font, [this]() { this->onRunDCSweepClicked(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Probe", font, [this]() { this->onToggleProbePanel(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Math", font, [this]() { this->onShowSignalMath(); }));
    current_x += 60 + button_spacing;
    ui_elements.push_back(std::make_unique<ActionButton>(current_x, button_y, 60, button_height, "Cursor", font, [this]() { this->onToggleCursors(); }));
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
        if (schematic_view) { schematic_view->clearWires(); schematic_view->updatePinPositions(); }
    }));
    current_x += 60 + button_spacing;


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
            
            // Handle cursor changes and hover detection in probe mode
            if (is_probe_mode && analysis_completed && schematic_view) {
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
                    // Place wire mode
                    is_drawing_wire = !is_drawing_wire; // Toggle wire mode
                    if (is_drawing_wire) {
                        is_creating_wire_from_pin = false;
                        placing_component_type = "Wire";
                        resetPlacementState();
                        std::cout << "Wire mode ACTIVATED. Click on pins to connect them." << std::endl;
                    } else {
                        is_creating_wire_from_pin = false;
                        placing_component_type = "";
                        cancelWireCreation();
                        std::cout << "Wire mode DEACTIVATED." << std::endl;
                    }
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
                case SDLK_e:
                    if (ctrl_pressed) {
                        // Mirror component (TODO: implement mirroring)
                        std::cout << "Component mirroring not yet implemented" << std::endl;
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
        for (auto& el : ui_elements) el->handleEvent(event);
        
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
                status_text = "Ready (F2: Components, F3: Wire, F4: Label, R: Resistor, C: Capacitor, L: Inductor, G: Ground, V: Voltage, I: Current) - " + components_info + probes_info;
            } else {
                status_text = "Ready (F2: Components, F3: Wire, F4: Label, R: Resistor, C: Capacitor, L: Inductor, G: Ground, V: Voltage, I: Current)";
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
    TransientAnalysis tran(settings_panel->getTranTStep(), settings_panel->getTranTStop());
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
        // Store latest results for probes
        latest_time_points = tran.getTimePoints();
        latest_tran_results = tran.getResults();
        if (latest_time_points.empty() || latest_tran_results.empty()) {
            logLine("[TRAN] WARNING: No data produced (points or results empty)");
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
            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                plot->setDataFiltered(latest_time_points, latest_tran_results, selected_signals);
                {
                    std::stringstream ss; ss << "[TRAN] plotted selected signals: " << selected_signals.size();
                    std::cout << ss.str() << std::endl; logLine(ss.str()); ErrorManager::info(ss.str());
                }
                plotted = true;
                break;
            }
        }
        if (!plotted) logLine("[TRAN] WARNING: PlotView not found");
        
        // Enable node name display after successful analysis
        analysis_completed = true;
        if (schematic_view) {
            schematic_view->setShowNodeNames(true);
        }
        
    } catch (const std::exception& e) {
        ErrorManager::displayError("Simulation failed: " + std::string(e.what()));
    }
}

void GuiApplication::onRunACAnalysisClicked() {
    MNAMatrix mna;
    LUDecompositionSolver solver;
    ACSweepAnalysis ac(settings_panel->getACSource(), settings_panel->getACStartFreq(), settings_panel->getACStopFreq(), settings_panel->getACPoints(), "DEC");
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
            ErrorManager::info("[AC] No AC sources found - add ACVoltageSource for frequency analysis");
        }

        ac.analyze(*circuit, mna, solver);

        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                plot->setDataAC(ac.getFrequencyPoints(), ac.getComplexResults());
                break;
            }
        }
        
        ErrorManager::info("[AC] Analysis complete: " + std::to_string(ac.getFrequencyPoints().size()) + " frequency points");

    } catch (const std::exception& e) {
        ErrorManager::displayError("AC Simulation failed: " + std::string(e.what()));
    }
}

void GuiApplication::onRunPhaseAnalysisClicked() {
    MNAMatrix mna;
    LUDecompositionSolver solver;
    PhaseSweepAnalysis phase("V1", 0, 360, 1e3, 100);
    try {
        if (!circuit->checkGroundNodeExists()) throw std::runtime_error("No ground node set.");
        if (!circuit->checkConnectivity()) throw std::runtime_error("Circuit not fully connected.");

        phase.analyze(*circuit, mna, solver);

        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                plot->setDataPhase(phase.getPhasePoints(), phase.getComplexResults());
                break;
            }
        }

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

    } catch (const std::exception& e) {
        ErrorManager::displayError("DC Sweep Simulation failed: " + std::string(e.what()));
    }
}

void GuiApplication::onSaveProjectClicked() {
    try {
        ProjectSerializer::save(*circuit, "circuit.json");
        std::cout << "Project saved to circuit.json" << std::endl;
    } catch (const std::exception& e) {
        ErrorManager::displayError("Failed to save project: " + std::string(e.what()));
    }
}

void GuiApplication::onLoadProjectClicked() {
    try {
        ProjectSerializer::load(*circuit, "circuit.json");
        std::cout << "Project loaded from circuit.json" << std::endl;
    } catch (const std::exception& e) {
        ErrorManager::displayError("Failed to load project: " + std::string(e.what()));
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
    if (probe_panel) probe_panel->toggleVisibility();
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
            // Update plot with all derived signals
            for (auto& el : ui_elements) {
                if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                    plot->setData(latest_time_points, combined_results);
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

void GuiApplication::onToggleCursors() {
    // Find the plot view and toggle cursor functionality
    for (auto& el : ui_elements) {
        if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
            // Access the cursor manager and toggle double cursor mode
            ErrorManager::info("[Cursor] Toggling cursor mode - use Left/Right click to place cursors");
            break;
        }
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
        ErrorManager::info("[Probe] Probe mode enabled - Click nodes for voltage probes, components for current probes");
        std::cout << "Probe mode enabled. Click on nodes (voltage) or components (current) to place probes." << std::endl;
    } else {
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
    
    // Update plot to show empty selection
    for (auto& el : ui_elements) {
        if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
            plot->setDataFiltered(latest_time_points, latest_tran_results, selected_signals);
            break;
        }
    }
}

void GuiApplication::selectComponentToPlace(const std::string& type) {
    placing_component_type = type;
    resetPlacementState();
    is_drawing_wire = false;
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
        redo_stack.clear();
    } catch (...) {}
}

void GuiApplication::applySnapshot(const std::string& snapshot) {
    try {
        const std::string path = "__apply_snapshot.json";
        std::ofstream ofs(path, std::ios::out | std::ios::binary);
        ofs << snapshot;
        ofs.close();
        ProjectSerializer::load(*circuit, path);
        if (schematic_view) {
            schematic_view->clearWires();
            schematic_view->updatePinPositions();
        }
    } catch (...) {}
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
    
    // Check if hovering over a node (for voltage probes)
    std::string node_at_cursor = schematic_view->getNodeAt(mouse_x, mouse_y);
    if (!node_at_cursor.empty()) {
        if (current_probe_type == ProbeType::VOLTAGE) {
            hover_msg = "[Probe] Click to measure voltage at node " + node_at_cursor;
        } else {
            hover_msg = "[Probe] Switch to voltage mode to probe this node";
        }
    } else {
        if (current_probe_type == ProbeType::CURRENT) {
            hover_msg = "[Probe] Click on component to measure current";
        } else {
            hover_msg = "[Probe] Hover over nodes for voltage probes";
        }
    }
    
    // Only show message if it changed to avoid spam
    if (hover_msg != last_hover_msg) {
        ErrorManager::info(hover_msg);
        last_hover_msg = hover_msg;
    }
    
    bool can_place_voltage_probe = !node_at_cursor.empty();
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
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));
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
    
    // Handle probe placement clicks
    if (is_probe_mode && analysis_completed && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        
        if (current_probe_type == ProbeType::VOLTAGE) {
            // Place voltage probe on node
            std::string node_at_click = schematic_view->getNodeAt(mx, my);
            if (!node_at_click.empty()) {
                std::string voltage_signal = "V(" + node_at_click + ")";
                if (latest_tran_results.find(voltage_signal) != latest_tran_results.end()) {
                    if (selected_signals.find(voltage_signal) == selected_signals.end()) {
                        selected_signals.insert(voltage_signal);
                        std::cout << "Added voltage probe: " << voltage_signal << " [Total probes: " << selected_signals.size() << "]" << std::endl;
                    } else {
                        std::cout << "Voltage probe " << voltage_signal << " already exists" << std::endl;
                    }
                    
                    // Update plot with new selection
                    for (auto& el : ui_elements) {
                        if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                            plot->setDataFiltered(latest_time_points, latest_tran_results, selected_signals);
                            break;
                        }
                    }
                } else {
                    std::cout << "Voltage signal " << voltage_signal << " not found in analysis results" << std::endl;
                }
            }
        } else if (current_probe_type == ProbeType::CURRENT) {
            // Place current probe on component
            for (const auto& element : circuit->getElements()) {
                if (!element) continue;
                
                // Get component position and check if click is within bounds
                SDL_Point elem_pos;
                if (element->getType() == "Ground") {
                    elem_pos = schematic_view->getNodePosition(element->getNode1Id());
                } else {
                    SDL_Point pos1 = schematic_view->getNodePosition(element->getNode1Id());
                    SDL_Point pos2 = schematic_view->getNodePosition(element->getNode2Id());
                    elem_pos.x = (pos1.x + pos2.x) / 2;
                    elem_pos.y = (pos1.y + pos2.y) / 2;
                }
                
                if (mx >= elem_pos.x - 25 && mx <= elem_pos.x + 25 && 
                    my >= elem_pos.y - 25 && my <= elem_pos.y + 25) {
                    
                    std::string current_signal = "I(" + element->getName() + ")";
                    if (latest_tran_results.find(current_signal) != latest_tran_results.end()) {
                        if (selected_signals.find(current_signal) == selected_signals.end()) {
                            selected_signals.insert(current_signal);
                            std::cout << "Added current probe: " << current_signal << " [Total probes: " << selected_signals.size() << "]" << std::endl;
                        } else {
                            std::cout << "Current probe " << current_signal << " already exists" << std::endl;
                        }
                        
                        // Update plot with new selection
                        for (auto& el : ui_elements) {
                            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                                plot->setDataFiltered(latest_time_points, latest_tran_results, selected_signals);
                                break;
                            }
                        }
                    } else {
                        std::cout << "Current signal " << current_signal << " not found in analysis results" << std::endl;
                    }
                    break;
                }
            }
        }
        return; // Don't process other click logic when in probe mode
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
    if (snapped_node.empty()) return;

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
                std::string left_node = "N" + std::to_string(grid_y * 100 + grid_x - 1);     // Left pin
                std::string right_node = "N" + std::to_string(grid_y * 100 + grid_x + 1);   // Right pin
                
                node1 = left_node;
                node2 = right_node;
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
            else if (placing_component_type == "ACVoltageSource") circuit->addElement(std::make_unique<ACVoltageSource>(name, node1, node2, 5.0, 0.0, 1000.0));
            else if (placing_component_type == "WaveformVoltageSource") {
                // Create default waveform (triangle wave)
                std::vector<double> waveform = {0.0, 2.5, 5.0, 2.5, 0.0, -2.5, -5.0, -2.5};
                circuit->addElement(std::make_unique<WaveformVoltageSource>(name, node1, node2, waveform, 8000.0, 1e-3));
            }
            else if (placing_component_type == "PhaseVoltageSource") circuit->addElement(std::make_unique<PhaseVoltageSource>(name, node1, node2, 5.0, 2*3.14159*1000, 0.0));
            else if (placing_component_type == "IndependentCurrentSource") circuit->addElement(std::make_unique<IndependentCurrentSource>(name, node1, node2, 1.0));
            else if (placing_component_type == "PulseCurrentSource") circuit->addElement(std::make_unique<PulseCurrentSource>(name, node1, node2, 0, 1e-3, 1e-3, 1e-4, 1e-4, 2e-3, 5e-3));

            else if (placing_component_type == "Diode") circuit->addElement(std::make_unique<Diode>(name, node1, node2, "D"));
            else if (placing_component_type == "Ground") circuit->addElement(std::make_unique<Ground>("GND", node1));
            else {
                std::cout << "WARNING: Unknown component type: " << placing_component_type << std::endl;
            }
            
            std::cout << "Component " << name << " placed successfully with single click!" << std::endl;
            
            // Update pins after adding component
            if (schematic_view) {
                schematic_view->updatePinPositions();
            }
            
            // Open configuration dialog for configurable components
            if (placing_component_type == "PulseVoltageSource" || 
                placing_component_type == "PulseCurrentSource" ||
                placing_component_type == "ACVoltageSource" ||

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
