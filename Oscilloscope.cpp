#include "Oscilloscope.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <complex>

Oscilloscope::Oscilloscope(int x, int y, int w, int h, TTF_Font* f)
    : font(f)
{
    view_area = {x, y, w, h};
    std::cout << "[OSCILLOSCOPE] Created at (" << x << "," << y << ") size (" << w << "," << h << ")" << std::endl;
}

void Oscilloscope::clearData() {
    time_points.clear();
    signals.clear();
    needs_redraw = true;
    std::cout << "[OSCILLOSCOPE] Data cleared" << std::endl;
}

void Oscilloscope::setTimePoints(const std::vector<double>& times) {
    time_points = times;
    if (!time_points.empty()) {
        x_min = time_points.front();
        x_max = time_points.back();
    }
    needs_redraw = true;
    std::cout << "[OSCILLOSCOPE] Time points set: " << time_points.size() << " points" << std::endl;
}

void Oscilloscope::addSignal(const std::string& name, const std::vector<double>& values) {
    if (values.size() != time_points.size()) {
        std::cerr << "[OSCILLOSCOPE] ERROR: Signal '" << name << "' has " << values.size()
                  << " values but expected " << time_points.size() << std::endl;
        return;
    }

    // Find available color
    SDL_Color color = signal_colors[signals.size() % signal_colors.size()];

    signals.emplace_back(name, values, color);
    needs_redraw = true;
    std::cout << "[OSCILLOSCOPE] Added signal '" << name << "' with " << values.size() << " points" << std::endl;
}

void Oscilloscope::removeSignal(const std::string& name) {
    auto it = std::find_if(signals.begin(), signals.end(),
                          [&name](const Signal& s) { return s.name == name; });
    if (it != signals.end()) {
        signals.erase(it);
        needs_redraw = true;
        std::cout << "[OSCILLOSCOPE] Removed signal '" << name << "'" << std::endl;
    }
}

void Oscilloscope::setSignalVisibility(const std::string& name, bool visible) {
    auto it = std::find_if(signals.begin(), signals.end(),
                          [&name](const Signal& s) { return s.name == name; });
    if (it != signals.end()) {
        it->visible = visible;
        needs_redraw = true;
    }
}

void Oscilloscope::setSelectedSignals(const std::set<std::string>& selected_names) {
    // First, hide all signals
    for (auto& signal : signals) {
        signal.visible = false;
    }

    // Then show only selected signals
    for (const auto& name : selected_names) {
        auto it = std::find_if(signals.begin(), signals.end(),
                              [&name](const Signal& s) { return s.name == name; });
        if (it != signals.end()) {
            it->visible = true;
            std::cout << "[OSCILLOSCOPE] Showing selected signal '" << name << "'" << std::endl;
        } else {
            std::cout << "[OSCILLOSCOPE] Selected signal '" << name << "' not found" << std::endl;
        }
    }

    // If no signals were selected, show all signals as fallback
    bool any_visible = false;
    for (const auto& signal : signals) {
        if (signal.visible) {
            any_visible = true;
            break;
        }
    }

    if (!any_visible && !signals.empty()) {
        std::cout << "[OSCILLOSCOPE] No signals selected, showing all signals as fallback" << std::endl;
        for (auto& signal : signals) {
            signal.visible = true;
        }
    }

    needs_redraw = true;
}

void Oscilloscope::setConfig(const Config& new_config) {
    config = new_config;
    needs_redraw = true;
    std::cout << "[OSCILLOSCOPE] Configuration updated" << std::endl;
}

void Oscilloscope::autoScale() {
    if (time_points.empty() || signals.empty()) {
        return;
    }

    // Find visible signals
    std::vector<Signal*> visible_signals;
    for (auto& signal : signals) {
        if (signal.visible && !signal.values.empty()) {
            visible_signals.push_back(&signal);
        }
    }

    if (visible_signals.empty()) {
        return;
    }

    // Calculate time range
    x_min = time_points.front();
    x_max = time_points.back();

    // Calculate voltage range across all visible signals
    y_min = std::numeric_limits<double>::max();
    y_max = std::numeric_limits<double>::lowest();

    for (auto* signal : visible_signals) {
        double sig_min = findMinValue(signal->values);
        double sig_max = findMaxValue(signal->values);
        y_min = std::min(y_min, sig_min);
        y_max = std::max(y_max, sig_max);
    }

    // Add some margin
    double time_range = x_max - x_min;
    double voltage_range = y_max - y_min;

    if (time_range > 0) {
        double time_margin = time_range * 0.05;
        x_min -= time_margin;
        x_max += time_margin;
    }

    if (voltage_range > 0) {
        double voltage_margin = voltage_range * 0.1;
        y_min -= voltage_margin;
        y_max += voltage_margin;
    } else if (voltage_range == 0) {
        // Handle constant signals
        double center = (y_min + y_max) / 2.0;
        double range = std::max(std::abs(center) * 0.1, 1.0);
        y_min = center - range;
        y_max = center + range;
    }

    needs_redraw = true;
    std::cout << "[OSCILLOSCOPE] Auto-scaled: time [" << x_min << "," << x_max
              << "] voltage [" << y_min << "," << y_max << "]" << std::endl;
}

void Oscilloscope::setTimeRange(double min_time, double max_time) {
    x_min = min_time;
    x_max = max_time;
    needs_redraw = true;
}

void Oscilloscope::setVoltageRange(double min_voltage, double max_voltage) {
    y_min = min_voltage;
    y_max = max_voltage;
    needs_redraw = true;
}

SDL_Point Oscilloscope::worldToScreen(double time, double voltage) const {
    // Account for margins
    int plot_x = view_area.x + 80;  // Left margin for Y-axis labels
    int plot_y = view_area.y + 40;  // Top margin
    int plot_width = view_area.w - 120;  // Right margin for legend
    int plot_height = view_area.h - 80;  // Bottom margin for X-axis labels

    // Transform to screen coordinates
    double time_ratio = (time - x_min) / (x_max - x_min);
    double voltage_ratio = (voltage - y_min) / (y_max - y_min);

    int screen_x = plot_x + static_cast<int>(time_ratio * plot_width);
    int screen_y = plot_y + plot_height - static_cast<int>(voltage_ratio * plot_height);

    return {screen_x, screen_y};
}

std::pair<double, double> Oscilloscope::screenToWorld(int screen_x, int screen_y) const {
    // Account for margins
    int plot_x = view_area.x + 80;
    int plot_y = view_area.y + 40;
    int plot_width = view_area.w - 120;
    int plot_height = view_area.h - 80;

    // Transform from screen coordinates
    double time_ratio = static_cast<double>(screen_x - plot_x) / plot_width;
    double voltage_ratio = 1.0 - static_cast<double>(screen_y - plot_y) / plot_height;

    double time = x_min + time_ratio * (x_max - x_min);
    double voltage = y_min + voltage_ratio * (y_max - y_min);

    return {time, voltage};
}

void Oscilloscope::render(SDL_Renderer* renderer) {
    if (!isDataValid()) {
        // Draw empty state
        SDL_SetRenderDrawColor(renderer, background_color.r, background_color.g, background_color.b, background_color.a);
        SDL_RenderFillRect(renderer, &view_area);
        drawText(renderer, "No Data", view_area.x + view_area.w/2 - 40, view_area.y + view_area.h/2, text_color);
        return;
    }

    if (needs_redraw) {
        autoScale();
        needs_redraw = false;
    }

    drawBackground(renderer);
    drawGrid(renderer);
    drawAxes(renderer);
    drawSignals(renderer);
    drawLegend(renderer);
    drawTitle(renderer);

    // Draw border
    SDL_SetRenderDrawColor(renderer, axis_color.r, axis_color.g, axis_color.b, axis_color.a);
    SDL_RenderDrawRect(renderer, &view_area);
}

void Oscilloscope::handleEvent(const SDL_Event& event) {
    // Basic event handling - can be extended for zooming, panning, etc.
    if (event.type == SDL_MOUSEBUTTONDOWN && contains(event.button.x, event.button.y)) {
        std::cout << "[OSCILLOSCOPE] Clicked at (" << event.button.x << "," << event.button.y << ")" << std::endl;
    }
}

void Oscilloscope::setPosition(int x, int y) {
    view_area.x = x;
    view_area.y = y;
    needs_redraw = true;
}

void Oscilloscope::setSize(int w, int h) {
    view_area.w = w;
    view_area.h = h;
    needs_redraw = true;
}

// Private helper methods

void Oscilloscope::drawBackground(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, background_color.r, background_color.g, background_color.b, background_color.a);
    SDL_RenderFillRect(renderer, &view_area);
}

void Oscilloscope::drawGrid(SDL_Renderer* renderer) {
    if (!config.show_grid) return;

    SDL_SetRenderDrawColor(renderer, grid_color.r, grid_color.g, grid_color.b, grid_color.a);

    // Account for margins
    int plot_x = view_area.x + 80;
    int plot_y = view_area.y + 40;
    int plot_width = view_area.w - 120;
    int plot_height = view_area.h - 80;

    // Vertical grid lines (time)
    int num_time_lines = 10;
    for (int i = 0; i <= num_time_lines; ++i) {
        int x = plot_x + (i * plot_width) / num_time_lines;
        SDL_RenderDrawLine(renderer, x, plot_y, x, plot_y + plot_height);
    }

    // Horizontal grid lines (voltage)
    int num_voltage_lines = 8;
    for (int i = 0; i <= num_voltage_lines; ++i) {
        int y = plot_y + (i * plot_height) / num_voltage_lines;
        SDL_RenderDrawLine(renderer, plot_x, y, plot_x + plot_width, y);
    }
}

void Oscilloscope::drawAxes(SDL_Renderer* renderer) {
    // Account for margins
    int plot_x = view_area.x + 80;
    int plot_y = view_area.y + 40;
    int plot_width = view_area.w - 120;
    int plot_height = view_area.h - 80;

    SDL_SetRenderDrawColor(renderer, axis_color.r, axis_color.g, axis_color.b, axis_color.a);

    // Draw axes
    SDL_RenderDrawLine(renderer, plot_x, plot_y + plot_height, plot_x + plot_width, plot_y + plot_height); // X-axis
    SDL_RenderDrawLine(renderer, plot_x, plot_y, plot_x, plot_y + plot_height); // Y-axis

    // Draw axis labels
    if (font) {
        // Time labels
        for (int i = 0; i <= 4; ++i) {
            double time = x_min + (x_max - x_min) * i / 4.0;
            int x = plot_x + (i * plot_width) / 4;
            int y = plot_y + plot_height + 5;

            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << time;
            drawText(renderer, ss.str(), x - 20, y, text_color);
        }

        // Voltage labels
        for (int i = 0; i <= 4; ++i) {
            double voltage = y_max - (y_max - y_min) * i / 4.0;
            int x = plot_x - 50;
            int y = plot_y + (i * plot_height) / 4 - 8;

            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << voltage;
            drawText(renderer, ss.str(), x, y, text_color);
        }
    }
}

void Oscilloscope::drawSignals(SDL_Renderer* renderer) {
    if (time_points.empty()) return;

    // Account for margins
    int plot_x = view_area.x + 80;
    int plot_y = view_area.y + 40;
    int plot_width = view_area.w - 120;
    int plot_height = view_area.h - 80;

    for (const auto& signal : signals) {
        if (!signal.visible || signal.values.empty()) continue;

        SDL_SetRenderDrawColor(renderer, signal.color.r, signal.color.g, signal.color.b, signal.color.a);

        // Draw signal as connected line segments
        for (size_t i = 0; i + 1 < time_points.size() && i + 1 < signal.values.size(); ++i) {
            SDL_Point p1 = worldToScreen(time_points[i], signal.values[i]);
            SDL_Point p2 = worldToScreen(time_points[i + 1], signal.values[i + 1]);

            // Clip to plot area
            if (p1.x >= plot_x && p1.x <= plot_x + plot_width &&
                p2.x >= plot_x && p2.x <= plot_x + plot_width &&
                p1.y >= plot_y && p1.y <= plot_y + plot_height &&
                p2.y >= plot_y && p2.y <= plot_y + plot_height) {
                SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
            }
        }
    }
}

void Oscilloscope::drawLegend(SDL_Renderer* renderer) {
    if (!config.show_legend || signals.empty()) return;

    int legend_x = view_area.x + view_area.w - 100;
    int legend_y = view_area.y + 50;
    int line_height = 20;

    for (size_t i = 0; i < signals.size(); ++i) {
        const auto& signal = signals[i];
        if (!signal.visible) continue;

        // Draw color box
        SDL_Rect color_box = {legend_x, legend_y + static_cast<int>(i) * line_height, 15, 15};
        SDL_SetRenderDrawColor(renderer, signal.color.r, signal.color.g, signal.color.b, signal.color.a);
        SDL_RenderFillRect(renderer, &color_box);

        // Draw signal name
        SDL_SetRenderDrawColor(renderer, text_color.r, text_color.g, text_color.b, text_color.a);
        SDL_RenderDrawRect(renderer, &color_box);
        drawText(renderer, signal.name, legend_x + 20, legend_y + static_cast<int>(i) * line_height + 2, text_color);
    }
}

void Oscilloscope::drawTitle(SDL_Renderer* renderer) {
    if (config.title.empty() || !font) return;

    drawText(renderer, config.title, view_area.x + 10, view_area.y + 10, text_color);
}

void Oscilloscope::drawText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    if (!font) return;

    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect dest = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

double Oscilloscope::findMinValue(const std::vector<double>& values) const {
    if (values.empty()) return 0.0;
    return *std::min_element(values.begin(), values.end());
}

double Oscilloscope::findMaxValue(const std::vector<double>& values) const {
    if (values.empty()) return 0.0;
    return *std::max_element(values.begin(), values.end());
}

bool Oscilloscope::isDataValid() const {
    if (time_points.empty()) return false;

    // Check if any signals are visible
    for (const auto& signal : signals) {
        if (signal.visible && !signal.values.empty()) {
            return true;
        }
    }

    return false;
}

void Oscilloscope::setDataAC(const std::vector<double>& freq_points, const std::map<std::string, std::vector<std::complex<double>>>& ac_results) {
    std::cout << "[OSCILLOSCOPE] setDataAC called with " << freq_points.size() << " frequency points" << std::endl;
    std::cout << "[OSCILLOSCOPE] AC results has " << ac_results.size() << " signals" << std::endl;
    
    // Clear existing data
    clearData();
    
    // Set frequency points as x-axis
    setTimePoints(freq_points);
    
    // Add magnitude data for each signal
    for (const auto& pair : ac_results) {
        const std::string& signal_name = pair.first;
        const std::vector<std::complex<double>>& complex_data = pair.second;
        
        std::vector<double> magnitude_data;
        magnitude_data.reserve(complex_data.size());
        
        for (const auto& complex_val : complex_data) {
            magnitude_data.push_back(std::abs(complex_val));
        }
        
        addSignal(signal_name, magnitude_data);
    }
    
    // Update configuration for frequency domain
    config.title = "AC Analysis - Magnitude";
    config.time_scale = 1.0; // Will be auto-scaled
    needs_redraw = true;
}

void Oscilloscope::setDataPhase(const std::vector<double>& phase_points, const std::map<std::string, std::vector<std::complex<double>>>& phase_results) {
    std::cout << "[OSCILLOSCOPE] setDataPhase called with " << phase_points.size() << " phase points" << std::endl;
    std::cout << "[OSCILLOSCOPE] Phase results has " << phase_results.size() << " signals" << std::endl;
    
    // Clear existing data
    clearData();
    
    // Set phase points as x-axis
    setTimePoints(phase_points);
    
    // Add phase data for each signal
    for (const auto& pair : phase_results) {
        const std::string& signal_name = pair.first;
        const std::vector<std::complex<double>>& complex_data = pair.second;
        
        std::vector<double> phase_data;
        phase_data.reserve(complex_data.size());
        
        for (const auto& complex_val : complex_data) {
            phase_data.push_back(std::arg(complex_val) * 180.0 / M_PI); // Convert to degrees
        }
        
        addSignal(signal_name, phase_data);
    }
    
    // Update configuration for phase domain
    config.title = "Phase Analysis";
    config.time_scale = 1.0; // Will be auto-scaled
    needs_redraw = true;
}