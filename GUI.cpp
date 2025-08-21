#include "Gui.h"
#include "ErrorManager.h"
#include "Analyzers.h"
#include "Solvers.h"
#include "Element.h"
#include "ProjectSerializer.h"
#include <SDL_image.h>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <utility>
#include <algorithm>
#include <cmath>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Button Implementation ---
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

    if (is_clicked) SDL_SetRenderDrawColor(renderer, 80, 80, 180, 255);
    else if (is_hovered) SDL_SetRenderDrawColor(renderer, 130, 130, 230, 255);
    else SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);

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
    SDL_SetRenderDrawColor(renderer, is_active ? 255 : 150, is_active ? 255 : 150, is_active ? 255 : 150, 255);
    SDL_RenderDrawRect(renderer, &rect);

    if (!text.empty() && font) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), {255, 255, 255, 255});
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

// --- SimulationSettingsPanel Implementation ---
SimulationSettingsPanel::SimulationSettingsPanel(int x, int y, int w, int h, TTF_Font* font) {
    panel_rect = { x, y, w, h };
    tran_inputs.push_back(std::make_unique<InputBox>(x + 100, y + 30, 100, 30, font, "1e-5"));
    tran_inputs.push_back(std::make_unique<InputBox>(x + 100, y + 70, 100, 30, font, "5e-3"));
    ac_inputs.push_back(std::make_unique<InputBox>(x + 100, y + 130, 100, 30, font, "V1"));
    ac_inputs.push_back(std::make_unique<InputBox>(x + 100, y + 170, 100, 30, font, "1"));
    ac_inputs.push_back(std::make_unique<InputBox>(x + 100, y + 210, 100, 30, font, "100k"));
    ac_inputs.push_back(std::make_unique<InputBox>(x + 100, y + 250, 100, 30, font, "100"));
}

void SimulationSettingsPanel::handleEvent(const SDL_Event& event) {
    if (!is_visible) return;
    for (auto& box : tran_inputs) box->handleEvent(event);
    for (auto& box : ac_inputs) box->handleEvent(event);
}

void SimulationSettingsPanel::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
    SDL_RenderFillRect(renderer, &panel_rect);
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
    source_buttons.push_back(std::make_unique<ActionButton>(x + w + 5, source_y, source_width, button_height, "Current Src", font, [on_select](){ on_select("IndependentCurrentSource"); }));

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
    }
}

void ComponentSelector::render(SDL_Renderer* renderer) {
    if (!is_visible) return;
    
    // Draw main panel background
    SDL_SetRenderDrawColor(renderer, 55, 55, 60, 255);
    SDL_RenderFillRect(renderer, &panel_rect);
    
    // Draw panel border
    SDL_SetRenderDrawColor(renderer, 100, 100, 110, 255);
    SDL_RenderDrawRect(renderer, &panel_rect);

    if (show_sources) {
        // Draw source submenu background
        SDL_Rect source_rect = {panel_rect.x + panel_rect.w + 5, panel_rect.y, panel_rect.w - 10, panel_rect.h};
        SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255);
        SDL_RenderFillRect(renderer, &source_rect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 110, 255);
        SDL_RenderDrawRect(renderer, &source_rect);
        
        for (auto& btn : source_buttons) btn->render(renderer);
    } else if (show_dependent_sources) {
        // Draw dependent source submenu background
        SDL_Rect dep_rect = {panel_rect.x + panel_rect.w + 5, panel_rect.y, panel_rect.w - 10, panel_rect.h};
        SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255);
        SDL_RenderFillRect(renderer, &dep_rect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 110, 255);
        SDL_RenderDrawRect(renderer, &dep_rect);
        
        for (auto& btn : dependent_source_buttons) btn->render(renderer);
    } else {
        for (auto& btn : component_buttons) btn->render(renderer);
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
}

void SchematicView::handleEvent(const SDL_Event& event) {}

// Helper method to calculate optimal scaling for a component (DEPRECATED - now using fixed 100px size)
double SchematicView::calculateOptimalScale(int original_w, int original_h) {
    // This method is no longer used - all components are now fixed at 100x100 pixels
    return 1.0;
}

std::string SchematicView::getNodeAt(int mouse_x, int mouse_y) {
    SDL_Point mousePoint = {mouse_x, mouse_y};
    if (SDL_PointInRect(&mousePoint, &view_area)) {
        int grid_x = (mouse_x - view_area.x + GRID_SIZE / 2) / GRID_SIZE;
        int grid_y = (mouse_y - view_area.y + GRID_SIZE / 2) / GRID_SIZE;
        return "N" + std::to_string(grid_y * 100 + grid_x);
    }
    return "";
}

SDL_Point SchematicView::getNodePosition(const std::string& node_id) {
    if (node_id.empty()) return { view_area.x, view_area.y };
    if (node_id == "0" || node_id == circuit_backend.getGroundNodeId()) {
         return { view_area.x + view_area.w / 2, view_area.y + view_area.h - GRID_SIZE };
    }
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

    if (type == "Wire") {
        SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
        return;
    }

    // Map element types to texture files
    if (type == "Resistor") texture = component_textures["R.png"];
    else if (type == "Capacitor") texture = component_textures["C.png"];
    else if (type == "Inductor") texture = component_textures["L.png"];
    else if (type == "Diode") texture = component_textures["Diode.jpg"];
    else if (type == "Ground") texture = component_textures["Ground.png"];
    else if (type == "IndependentVoltageSource") texture = component_textures["VDC.png"];
    else if (type == "PulseVoltageSource") texture = component_textures["Vpulse.png"];
    else if (type == "SinusoidalVoltageSource") texture = component_textures["Vsin.png"];
    else if (type == "IndependentCurrentSource") texture = component_textures["Current.png"];
    else if (type == "VoltageControlledVoltageSource") texture = component_textures["VCVS.png"];
    else if (type == "VoltageControlledCurrentSource") texture = component_textures["VCCS.png"];
    else if (type == "CurrentControlledVoltageSource") texture = component_textures["CCVS.png"];
    else if (type == "CurrentControlledCurrentSource") texture = component_textures["CCCS.png"];

    if (texture) {
        // Read actual image dimensions dynamically
        int original_w, original_h;
        SDL_QueryTexture(texture, NULL, NULL, &original_w, &original_h);
        
        // Fixed size: all components are exactly 100 pixels
        const int FIXED_SIZE = 100;
        int scaled_w = FIXED_SIZE;
        int scaled_h = FIXED_SIZE;
        
        // Calculate center position between the two nodes
        int center_x = (p1.x + p2.x) / 2;
        int center_y = (p1.y + p2.y) / 2;
        
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
        
        // Draw connection lines from nodes to component center
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, dest.x + scaled_w/2, dest.y + scaled_h/2);
        SDL_RenderDrawLine(renderer, dest.x + scaled_w/2, dest.y + scaled_h/2, p2.x, p2.y);
        
        // Render the fixed-size component
        SDL_RenderCopy(renderer, texture, nullptr, &dest);
        
        // Draw a small border around the component to make it more visible
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow border
        SDL_RenderDrawRect(renderer, &dest);
        
        // Debug: Print component info
        std::cout << "Rendering " << type << " at (" << dest_x << "," << dest_y << ") size " << scaled_w << "x" << scaled_h 
                  << " (original: " << original_w << "x" << original_h << ")" << std::endl;
    }
}

void SchematicView::drawNodeLabels(SDL_Renderer* renderer) {
    if (!font) return;
    for (const auto& pair : circuit_backend.getNodeLabels()) {
        SDL_Point pos = getNodePosition(pair.first);
        SDL_Surface* surface = TTF_RenderText_Blended(font, pair.second.c_str(), {255, 255, 0, 255});
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { pos.x + 5, pos.y - 20, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
}

void SchematicView::render(SDL_Renderer* renderer) {
    // Draw background
    SDL_SetRenderDrawColor(renderer, 20, 20, 22, 255);
    SDL_RenderFillRect(renderer, &view_area);
    
    // Draw grid with different intensities for major and minor lines
    SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
    for (int x = view_area.x; x < view_area.x + view_area.w; x += GRID_SIZE) {
        SDL_RenderDrawLine(renderer, x, view_area.y, x, view_area.y + view_area.h);
    }
    for (int y = view_area.y; y < view_area.y + view_area.h; y += GRID_SIZE) {
        SDL_RenderDrawLine(renderer, view_area.x, y, view_area.x + view_area.w, y);
    }
    
    // Draw major grid lines (every 5th line) with higher intensity
    SDL_SetRenderDrawColor(renderer, 45, 45, 49, 255);
    for (int x = view_area.x; x < view_area.x + view_area.w; x += GRID_SIZE * 5) {
        SDL_RenderDrawLine(renderer, x, view_area.y, x, view_area.y + view_area.h);
    }
    for (int y = view_area.y; y < view_area.y + view_area.h; y += GRID_SIZE * 5) {
        SDL_RenderDrawLine(renderer, view_area.x, y, view_area.x + view_area.w, y);
    }
    
    // Draw border
    SDL_SetRenderDrawColor(renderer, 60, 60, 64, 255);
    SDL_RenderDrawRect(renderer, &view_area);
    
    // Draw all circuit elements
    for (const auto& el : circuit_backend.getElements()) {
        if (el) drawElementSymbol(renderer, *el);
    }
    
    // Draw node labels
    drawNodeLabels(renderer);
}

// --- PlotView Implementation ---
PlotView::PlotView(int x, int y, int w, int h, TTF_Font* font) : font(font) {
    view_area = { x, y, w, h };
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
    for (const auto& sig : signals) {
        for (double val : sig.y_values) {
            min_y = std::min(min_y, val);
            max_y = std::max(max_y, val);
        }
    }
    if (min_y == max_y) { min_y -= 1.0; max_y += 1.0; }
    double margin_y = (max_y - min_y) * 0.1;
    min_y -= margin_y; max_y += margin_y;
    if (max_x - min_x == 0 || max_y - min_y == 0) return;
    scale_x = view_area.w / (max_x - min_x);
    scale_y = view_area.h / (max_y - min_y);
    offset_x = -min_x;
    offset_y = -min_y;
}

void PlotView::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEWHEEL) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        if (mouseX >= view_area.x && mouseX <= view_area.x + view_area.w &&
            mouseY >= view_area.y && mouseY <= view_area.y + view_area.h) {
            double old_scale_x = scale_x;
            double old_scale_y = scale_y;
            if (event.wheel.y > 0) { // Zoom in
                scale_x *= 1.1;
                scale_y *= 1.1;
            } else { // Zoom out
                scale_x /= 1.1;
                scale_y /= 1.1;
            }
            offset_x = (offset_x * old_scale_x + (mouseX - view_area.x) * (old_scale_x - scale_x)) / scale_x;
            offset_y = (offset_y * old_scale_y + (view_area.h - (mouseY - view_area.y)) * (old_scale_y - scale_y)) / scale_y;
        }
    } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            cursor1.visible = true;
            cursor1.screen_x = event.button.x;
            updateCursorValue(cursor1);
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            cursor2.visible = true;
            cursor2.screen_x = event.button.x;
            updateCursorValue(cursor2);
        }
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
    return { static_cast<int>((screen_x - view_area.x) / scale_x - offset_x),
             static_cast<int>((view_area.h - (screen_y - view_area.y)) / scale_y - offset_y) };
}

SDL_Point PlotView::toScreenCoords(double world_x, double world_y) {
    return { view_area.x + static_cast<int>((world_x + offset_x) * scale_x),
             view_area.y + view_area.h - static_cast<int>((world_y + offset_y) * scale_y) };
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

    if (!x_values.empty()) {
        for (const auto& sig : signals) {
            if (sig.y_values.size() < 2) continue;
            SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, sig.color.a);
            for (size_t i = 0; i < x_values.size() - 1; ++i) {
                SDL_Point p1 = toScreenCoords(x_values[i], sig.y_values[i]);
                SDL_Point p2 = toScreenCoords(x_values[i+1], sig.y_values[i+1]);
                SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
            }
        }
    }

    if (cursor1.visible) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_RenderDrawLine(renderer, cursor1.screen_x, view_area.y, cursor1.screen_x, view_area.y + view_area.h);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << "C1: (" << cursor1.world_x << ", " << cursor1.world_y << ")";
        renderText(renderer, ss.str(), view_area.x + 5, view_area.y + 5, {255, 255, 0, 255});
    }
    if (cursor2.visible) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
        SDL_RenderDrawLine(renderer, cursor2.screen_x, view_area.y, cursor2.screen_x, view_area.y + view_area.h);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << "C2: (" << cursor2.world_x << ", " << cursor2.world_y << ")";
        renderText(renderer, ss.str(), view_area.x + 5, view_area.y + 25, {0, 255, 255, 255});
    }

    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderDrawRect(renderer, &view_area);
    SDL_RenderSetClipRect(renderer, nullptr);
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
    if (TTF_Init() == -1) throw std::runtime_error("TTF init failed");
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) throw std::runtime_error("IMG init failed");

    window = SDL_CreateWindow("Circuit Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    font = TTF_OpenFont("C:/Windows/Fonts/Arial.ttf", 16);

    if (!window || !renderer || !font) throw std::runtime_error("GUI creation failed");

    component_textures["R.png"] = loadTexture("assets/R.png");
    component_textures["C.png"] = loadTexture("assets/C.png");
    component_textures["L.png"] = loadTexture("assets/L.png");
    component_textures["Diode.jpg"] = loadTexture("assets/Diode.jpg");
    component_textures["Ground.png"] = loadTexture("assets/Ground.png");
    component_textures["VDC.png"] = loadTexture("assets/VDC.png");
    component_textures["Vpulse.png"] = loadTexture("assets/Vpulse.png");
    component_textures["Vsin.png"] = loadTexture("assets/Vsin.png");
    component_textures["Current.png"] = loadTexture("assets/Current.png");
    component_textures["VCVS.png"] = loadTexture("assets/VCVS.png");
    component_textures["VCCS.png"] = loadTexture("assets/VCCS.png");
    component_textures["CCVS.png"] = loadTexture("assets/CCVS.png");
    component_textures["CCCS.png"] = loadTexture("assets/CCCS.png");

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

    auto schematic = std::make_unique<SchematicView>(200, 20, 1060, 480, *circuit, component_textures, font);
    schematic_view = schematic.get();
    ui_elements.push_back(std::move(schematic));

    ui_elements.push_back(std::make_unique<PlotView>(200, 520, 1060, 180, font));

    auto selector = std::make_unique<ComponentSelector>(20, 420, 150, 300, font, [this](const std::string& type){ this->selectComponentToPlace(type); });
    component_selector = selector.get();
    ui_elements.push_back(std::move(selector));

    auto settings = std::make_unique<SimulationSettingsPanel>(200, 200, 350, 250, font);
    settings_panel = settings.get();
    ui_elements.push_back(std::move(settings));

    // Analysis buttons - smaller and better organized
    ui_elements.push_back(std::make_unique<ActionButton>(20, 20, 150, 30, "Run TRAN", font, [this]() { this->onRunSimulationClicked(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 55, 150, 30, "Run AC", font, [this]() { this->onRunACAnalysisClicked(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 90, 150, 30, "Run DC Sweep", font, [this]() { this->onRunDCSweepClicked(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 125, 150, 30, "Run Phase", font, [this]() { this->onRunPhaseAnalysisClicked(); }));
    
    // Project management buttons
    ui_elements.push_back(std::make_unique<ActionButton>(20, 170, 150, 30, "Settings", font, [this]() { this->onToggleSettingsPanel(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 205, 150, 30, "Add Component", font, [this]() { this->onToggleComponentSelector(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 240, 150, 30, "Add Label", font, [this]() { this->onAddNodeLabel(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 275, 150, 30, "Save Project", font, [this]() { this->onSaveProjectClicked(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 310, 150, 30, "Load Project", font, [this]() { this->onLoadProjectClicked(); }));
    ui_elements.push_back(std::make_unique<ActionButton>(20, 345, 150, 30, "Save Subcircuit", font, [this]() { this->onSaveSubcircuitClicked(); }));
    
    // Quit button at the bottom
    ui_elements.push_back(std::make_unique<ActionButton>(20, 380, 150, 30, "Quit", font, [this]() { onQuitClicked(); }));
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
        if (event.type == SDL_MOUSEMOTION) current_mouse_pos = { event.motion.x, event.motion.y };

        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_w) {
            is_drawing_wire = true;
            placing_component_type = "Wire";
            resetPlacementState();
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && is_drawing_wire && !is_wire_drag_active) {
            if (schematic_view) {
                std::string clicked_node = schematic_view->getNodeAt(event.button.x, event.button.y);
                if (!clicked_node.empty()) {
                    wire_draw_start_pos = { event.button.x, event.button.y };
                    wire_start_node = clicked_node;
                    is_wire_drag_active = true;
                }
            }
        }

        if (event.type == SDL_MOUSEBUTTONUP && is_wire_drag_active) {
            if (schematic_view) {
                std::string end_node = schematic_view->getNodeAt(event.button.x, event.button.y);
                if (!end_node.empty()) {
                    circuit->addElement(std::make_unique<Wire>("W" + std::to_string(circuit->getElements().size() + 1), wire_start_node, end_node));
                }
            }
            is_wire_drag_active = false;
            wire_start_node = "";
        }

        if (!is_wire_drag_active) {
             for (auto& el : ui_elements) el->handleEvent(event);
             handleSchematicClick(event);
        }
    }
}

void GuiApplication::render() {
    SDL_SetRenderDrawColor(renderer, 45, 45, 48, 255);
    SDL_RenderClear(renderer);
    for (auto& el : ui_elements) el->render(renderer);

    if (is_wire_drag_active) {
        SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);
        SDL_RenderDrawLine(renderer, wire_draw_start_pos.x, wire_draw_start_pos.y, current_mouse_pos.x, current_mouse_pos.y);
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
        tran.analyze(*circuit, mna, solver);
        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                plot->setData(tran.getTimePoints(), tran.getResults());
                break;
            }
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

        ac.analyze(*circuit, mna, solver);

        for (auto& el : ui_elements) {
            if (auto* plot = dynamic_cast<PlotView*>(el.get())) {
                plot->setDataAC(ac.getFrequencyPoints(), ac.getComplexResults());
                break;
            }
        }

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
    if (component_selector) component_selector->toggleVisibility();
}

void GuiApplication::onToggleSettingsPanel() {
    if (settings_panel) settings_panel->toggleVisibility();
}

void GuiApplication::onQuitClicked() {
    is_running = false;
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

void GuiApplication::handleSchematicClick(const SDL_Event& event) {
    if (event.type != SDL_MOUSEBUTTONDOWN) return;
    if (!schematic_view) return;

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

    bool is_dep_source = (placing_component_type == "VCVS" || placing_component_type == "VCCS" || placing_component_type == "CCVS" || placing_component_type == "CCCS");

    if (!is_dep_source) {
        if (placement_step == 0) {
            node1 = clicked_node;
            placement_step = 1;
            std::cout << "First node selected: " << node1 << " for " << placing_component_type << std::endl;
        } else {
            node2 = clicked_node;
            std::cout << "Second node selected: " << node2 << " for " << placing_component_type << std::endl;
            try {
                std::string name = placing_component_type.substr(0, 1) + std::to_string(circuit->getElements().size() + 1);
                std::cout << "Creating component: " << name << " of type " << placing_component_type << std::endl;
                
                if (placing_component_type == "Resistor") circuit->addElement(std::make_unique<Resistor>(name, node1, node2, 1000.0));
                else if (placing_component_type == "Capacitor") circuit->addElement(std::make_unique<Capacitor>(name, node1, node2, 1e-6));
                else if (placing_component_type == "Inductor") circuit->addElement(std::make_unique<Inductor>(name, node1, node2, 1e-3));
                else if (placing_component_type == "IndependentVoltageSource") circuit->addElement(std::make_unique<IndependentVoltageSource>(name, node1, node2, 5.0));
                else if (placing_component_type == "PulseVoltageSource") circuit->addElement(std::make_unique<PulseVoltageSource>(name, node1, node2, 0, 5, 0, 1e-6, 1e-6, 1e-3, 2e-3));
                else if (placing_component_type == "SinusoidalVoltageSource") circuit->addElement(std::make_unique<SinusoidalVoltageSource>(name, node1, node2, 0, 5, 1000));
                else if (placing_component_type == "IndependentCurrentSource") circuit->addElement(std::make_unique<IndependentCurrentSource>(name, node1, node2, 1.0));
                else if (placing_component_type == "Diode") circuit->addElement(std::make_unique<Diode>(name, node1, node2, "D"));
                else if (placing_component_type == "Ground") circuit->addElement(std::make_unique<Ground>("GND", node1));
                else {
                    std::cout << "WARNING: Unknown component type: " << placing_component_type << std::endl;
                }
                
                std::cout << "Component " << name << " added successfully!" << std::endl;
            } catch (const std::exception& e) { 
                std::cout << "ERROR creating component: " << e.what() << std::endl;
                ErrorManager::displayError(e.what()); 
            }
            placing_component_type = "";
            resetPlacementState();
        }
    } else { // Dependent source placement
        if (placement_step == 0) { node1 = clicked_node; placement_step = 1; }
        else if (placement_step == 1) { node2 = clicked_node; placement_step = 2; }
        else if (placement_step == 2) { ctrl_node1 = clicked_node; placement_step = 3; }
        else {
            ctrl_node2 = clicked_node;
            try {
                std::string name = placing_component_type.substr(0, 1) + std::to_string(circuit->getElements().size() + 1);
                if (placing_component_type == "VCVS") circuit->addElement(std::make_unique<VoltageControlledVoltageSource>(name, node1, node2, ctrl_node1, ctrl_node2, 2.0));
                else if (placing_component_type == "VCCS") circuit->addElement(std::make_unique<VoltageControlledCurrentSource>(name, node1, node2, ctrl_node1, ctrl_node2, 0.01));
                else if (placing_component_type == "CCVS") circuit->addElement(std::make_unique<CurrentControlledVoltageSource>(name, node1, node2, "Vcontrol", 10.0));
                else if (placing_component_type == "CCCS") circuit->addElement(std::make_unique<CurrentControlledCurrentSource>(name, node1, node2, "Vcontrol", 2.0));
            } catch (const std::exception& e) { ErrorManager::displayError(e.what()); }
            placing_component_type = "";
            resetPlacementState();
        }
    }
}
