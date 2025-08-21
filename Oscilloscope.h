#pragma once

#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <SDL.h>
#include <SDL_ttf.h>
#include "GUI.h"  // Include GUI.h for GuiComponent

struct SDL_Renderer;
struct SDL_Window;

/**
 * @brief Robust Oscilloscope widget for displaying circuit signals
 * Replaces the complex PlotView system with a cleaner, more reliable implementation
 */
class Oscilloscope : public GuiComponent {
public:
    // --- Signal Data Structure ---
    struct Signal {
        std::string name;
        std::vector<double> values;
        SDL_Color color;
        bool visible = true;

        Signal(const std::string& n, const std::vector<double>& v, SDL_Color c)
            : name(n), values(v), color(c) {}
    };

    // --- Plot Configuration ---
    struct Config {
        int width = 800;
        int height = 400;
        std::string title = "Oscilloscope";
        bool show_grid = true;
        bool show_legend = true;
        bool auto_scale = true;
        double time_scale = 1.0;  // Time units per pixel
        double voltage_scale = 1.0; // Voltage units per pixel
        double time_offset = 0.0;
        double voltage_offset = 0.0;
    };

private:
    SDL_Rect view_area;
    TTF_Font* font = nullptr;

    // Data
    std::vector<double> time_points;
    std::vector<Signal> signals;
    Config config;

    // Display state
    double x_min = 0, x_max = 1;
    double y_min = -1, y_max = 1;
    bool needs_redraw = true;

    // Colors
    SDL_Color background_color = {20, 20, 22, 255};
    SDL_Color grid_color = {60, 60, 70, 255};
    SDL_Color axis_color = {150, 150, 160, 255};
    SDL_Color text_color = {200, 200, 210, 255};

    // Signal colors (palette)
    std::vector<SDL_Color> signal_colors = {
        {100, 255, 100, 255},  // Green
        {255, 100, 100, 255},  // Red
        {100, 100, 255, 255},  // Blue
        {255, 255, 100, 255},  // Yellow
        {100, 255, 255, 255},  // Cyan
        {255, 100, 255, 255},  // Magenta
        {255, 150, 100, 255},  // Orange
        {150, 100, 255, 255}   // Purple
    };

public:
    Oscilloscope(int x, int y, int w, int h, TTF_Font* f);
    virtual ~Oscilloscope() = default;

    // Data management
    void clearData();
    void setTimePoints(const std::vector<double>& times);
    void addSignal(const std::string& name, const std::vector<double>& values);
    void removeSignal(const std::string& name);
    void setSignalVisibility(const std::string& name, bool visible);
    void setSelectedSignals(const std::set<std::string>& selected_names);
    
    // AC and Phase analysis plotting
    void setDataAC(const std::vector<double>& freq_points, const std::map<std::string, std::vector<std::complex<double>>>& ac_results);
    void setDataPhase(const std::vector<double>& phase_points, const std::map<std::string, std::vector<std::complex<double>>>& phase_results);

    // Configuration
    void setConfig(const Config& new_config);
    const Config& getConfig() const { return config; }

    // Auto-scaling
    void autoScale();
    void setTimeRange(double min_time, double max_time);
    void setVoltageRange(double min_voltage, double max_voltage);

    // Coordinate transformations
    SDL_Point worldToScreen(double time, double voltage) const;
    std::pair<double, double> screenToWorld(int screen_x, int screen_y) const;

    // GuiComponent interface
    void render(SDL_Renderer* renderer) override;
    void handleEvent(const SDL_Event& event) override;
    bool contains(int x, int y) const override {
        SDL_Point point = {x, y};
        return SDL_PointInRect(&point, &view_area);
    }
    void setPosition(int x, int y) override;
    void setSize(int w, int h) override;
    SDL_Rect getBounds() const override { return view_area; }

private:
    // Rendering helpers
    void drawBackground(SDL_Renderer* renderer);
    void drawGrid(SDL_Renderer* renderer);
    void drawAxes(SDL_Renderer* renderer);
    void drawSignals(SDL_Renderer* renderer);
    void drawLegend(SDL_Renderer* renderer);
    void drawTitle(SDL_Renderer* renderer);

    // Text rendering
    void drawText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);

    // Utility functions
    double findMinValue(const std::vector<double>& values) const;
    double findMaxValue(const std::vector<double>& values) const;
    bool isDataValid() const;
};
