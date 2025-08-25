#include "Plotter.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <iomanip>

// --- CircuitPlotter Implementation ---
CircuitPlotter::CircuitPlotter(const PlotConfig& config) 
    : config(config),
      window(nullptr, SDL_DestroyWindow),
      renderer(nullptr, SDL_DestroyRenderer) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        // Handle initialization error
        return;
    }

    if (TTF_Init() < 0) {
        // Handle TTF initialization error
        return;
    }

    // Create window
    window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>(
        SDL_CreateWindow(config.title.c_str(),
                         SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         config.width, config.height, SDL_WINDOW_SHOWN),
        SDL_DestroyWindow
    );

    if (!window) {
        // Handle window creation error
        return;
    }

    // Create renderer
    renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED),
        SDL_DestroyRenderer
    );

    if (!renderer) {
        // Handle renderer creation error
        return;
    }
}

CircuitPlotter::~CircuitPlotter() {
    // SDL cleanup is handled by unique_ptr with custom deleters
    TTF_Quit();
    SDL_Quit();
}

void CircuitPlotter::plotTransientResults(const AnalysisResult& result) {
    if (!result.success || result.time_series.empty()) {
        return;
    }
    
    // Convert to plot data
    auto plot_data = PlotUtils::transientResultToPlotData(result);
    plotTimeSeries(plot_data, "Transient Analysis Results");
}

void CircuitPlotter::plotACResults(const AnalysisResult& result) {
    if (!result.success || result.frequency_series.empty()) {
        return;
    }
    
    // Convert to plot data
    auto plot_data = PlotUtils::acResultToPlotData(result);
    plotFrequencyResponse(plot_data, "AC Analysis Results");
}

void CircuitPlotter::plotDCSweepResults(const AnalysisResult& result) {
    if (!result.success || result.time_series.empty()) {
        return;
    }
    
    // Convert to plot data
    auto plot_data = PlotUtils::dcSweepResultToPlotData(result);
    plotDCSweep(plot_data, "DC Sweep Results");
}

void CircuitPlotter::plotDCResults(const AnalysisResult& result) {
    if (!result.success) {
        return;
    }
    
    // Create bar chart of node voltages
    plotBarChart(result.node_voltages, "DC Analysis - Node Voltages");
    
    // Create bar chart of branch currents
    plotBarChart(result.branch_currents, "DC Analysis - Branch Currents");
}

void CircuitPlotter::plotTimeSeries(const std::vector<PlotData>& data, const std::string& title) {
    if (data.empty()) return;
    
    clear();
    
    // Find data ranges
    double x_min = std::numeric_limits<double>::max();
    double x_max = std::numeric_limits<double>::lowest();
    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::lowest();
    
    for (const auto& series : data) {
        for (const auto& point : series.points) {
            x_min = std::min(x_min, point.x);
            x_max = std::max(x_max, point.x);
            y_min = std::min(y_min, point.y);
            y_max = std::max(y_max, point.y);
        }
    }
    
    // Add some padding
    double x_range = x_max - x_min;
    double y_range = y_max - y_min;
    x_min -= x_range * 0.05;
    x_max += x_range * 0.05;
    y_min -= y_range * 0.05;
    y_max += y_range * 0.05;
    
    // Draw grid and axis
    if (config.show_grid) drawGrid();
    drawAxis();
    
    // Draw each series
    for (const auto& series : data) {
        if (series.points.size() < 2) continue;
        
        setColor(series.color_r, series.color_g, series.color_b);
        
        // Draw lines connecting points
        for (size_t i = 1; i < series.points.size(); ++i) {
            auto [x1, y1] = worldToScreen(series.points[i-1].x, series.points[i-1].y, 
                                         x_min, x_max, y_min, y_max);
            auto [x2, y2] = worldToScreen(series.points[i].x, series.points[i].y, 
                                         x_min, x_max, y_min, y_max);
            drawLine(x1, y1, x2, y2);
        }
        
        // Draw points
        for (const auto& point : series.points) {
            auto [x, y] = worldToScreen(point.x, point.y, x_min, x_max, y_min, y_max);
            drawCircle(x, y, 3);
        }
    }
    
    // Draw legend
    if (config.show_legend) {
        std::vector<std::string> series_names;
        for (const auto& series : data) {
            series_names.push_back(series.series_name);
        }
        drawLegend(series_names);
    }
    
    update();
}

void CircuitPlotter::plotFrequencyResponse(const std::vector<ComplexPlotData>& data, const std::string& title) {
    if (data.empty()) return;
    
    clear();
    
    // Find data ranges
    double x_min = std::numeric_limits<double>::max();
    double x_max = std::numeric_limits<double>::lowest();
    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::lowest();
    
    for (const auto& series : data) {
        for (const auto& point : series.points) {
            x_min = std::min(x_min, point.x);
            x_max = std::max(x_max, point.x);
            
            double magnitude = std::abs(point.y);
            y_min = std::min(y_min, magnitude);
            y_max = std::max(y_max, magnitude);
        }
    }
    
    // Add some padding
    double x_range = x_max - x_min;
    double y_range = y_max - y_min;
    x_min -= x_range * 0.05;
    x_max += x_range * 0.05;
    y_min -= y_range * 0.05;
    y_max += y_range * 0.05;
    
    // Draw grid and axis
    if (config.show_grid) drawGrid();
    drawAxis();
    
    // Draw each series
    for (const auto& series : data) {
        if (series.points.size() < 2) continue;
        
        setColor(series.color_r, series.color_g, series.color_b);
        
        // Draw magnitude response
        for (size_t i = 1; i < series.points.size(); ++i) {
            double mag1 = std::abs(series.points[i-1].y);
            double mag2 = std::abs(series.points[i].y);
            
            auto [x1, y1] = worldToScreen(series.points[i-1].x, mag1, x_min, x_max, y_min, y_max);
            auto [x2, y2] = worldToScreen(series.points[i].x, mag2, x_min, x_max, y_min, y_max);
            drawLine(x1, y1, x2, y2);
        }
    }
    
    // Draw legend
    if (config.show_legend) {
        std::vector<std::string> series_names;
        for (const auto& series : data) {
            series_names.push_back(series.series_name);
        }
        drawLegend(series_names);
    }
    
    update();
}

void CircuitPlotter::plotDCSweep(const std::vector<PlotData>& data, const std::string& title) {
    // Similar to time series but with different axis labels
    plotTimeSeries(data, title);
}

void CircuitPlotter::plotBarChart(const std::map<std::string, double>& data, const std::string& title) {
    if (data.empty()) return;
    
    clear();
    
    // Find data range
    double y_min = 0.0;
    double y_max = std::numeric_limits<double>::lowest();
    
    for (const auto& [key, value] : data) {
        y_max = std::max(y_max, value);
    }
    
    // Add some padding
    y_max += y_max * 0.1;
    
    // Calculate bar dimensions
    int num_bars = data.size();
    int bar_width = (config.width - 2 * config.margin) / num_bars;
    
    // Draw bars
    int bar_index = 0;
    for (const auto& [key, value] : data) {
        int x = config.margin + bar_index * bar_width + bar_width / 2;
        int height = static_cast<int>((value / y_max) * (config.height - 2 * config.margin));
        int y = config.height - config.margin - height;
        
        setColor(0, 0, 255);
        SDL_Rect bar_rect = {x - bar_width/4, y, bar_width/2, height};
        SDL_RenderFillRect(renderer.get(), &bar_rect);
        
        // Draw label
        setColor(0, 0, 0);
        drawText(x, config.height - config.margin + 10, key);
        
        bar_index++;
    }
    
    update();
}

void CircuitPlotter::show() {
    if (window) {
        SDL_ShowWindow(window.get());
    }
}

void CircuitPlotter::hide() {
    if (window) {
        SDL_HideWindow(window.get());
    }
}

void CircuitPlotter::clear() {
    if (renderer) {
        setColor(config.background.r, config.background.g, config.background.b);
        SDL_RenderClear(renderer.get());
    }
}

void CircuitPlotter::update() {
    if (renderer) {
        SDL_RenderPresent(renderer.get());
    }
}

bool CircuitPlotter::isRunning() const {
    return window != nullptr && renderer != nullptr;
}

void CircuitPlotter::setConfig(const PlotConfig& new_config) {
    config = new_config;
    // Note: Window resize would require additional SDL calls
}

const PlotConfig& CircuitPlotter::getConfig() const {
    return config;
}

// --- Helper Methods ---
void CircuitPlotter::drawGrid() {
    if (!renderer) return;
    
    setColor(config.grid_color.r, config.grid_color.g, config.grid_color.b);
    
    // Vertical grid lines
    for (int i = 0; i <= config.grid_lines; ++i) {
        int x = config.margin + (i * (config.width - 2 * config.margin)) / config.grid_lines;
        drawLine(x, config.margin, x, config.height - config.margin);
    }
    
    // Horizontal grid lines
    for (int i = 0; i <= config.grid_lines; ++i) {
        int y = config.margin + (i * (config.height - 2 * config.margin)) / config.grid_lines;
        drawLine(config.margin, y, config.width - config.margin, y);
    }
}

void CircuitPlotter::drawAxis() {
    if (!renderer) return;
    
    setColor(0, 0, 0);
    
    // X-axis
    drawLine(config.margin, config.height - config.margin, 
             config.width - config.margin, config.height - config.margin);
    
    // Y-axis
    drawLine(config.margin, config.margin, config.margin, config.height - config.margin);
}

void CircuitPlotter::drawLegend(const std::vector<std::string>& series_names) {
    if (!renderer) return;
    
    int legend_x = config.width - 150;
    int legend_y = config.margin;
    
    for (size_t i = 0; i < series_names.size(); ++i) {
        // Draw color indicator
        setColor(0, 0, 255);
        drawCircle(legend_x, legend_y + i * 20, 5);
        
        // Draw text
        setColor(0, 0, 0);
        drawText(legend_x + 15, legend_y + i * 20, series_names[i]);
    }
}

void CircuitPlotter::setColor(int r, int g, int b, int a) {
    if (renderer) {
        SDL_SetRenderDrawColor(renderer.get(), r, g, b, a);
    }
}

void CircuitPlotter::drawLine(int x1, int y1, int x2, int y2) {
    if (renderer) {
        SDL_RenderDrawLine(renderer.get(), x1, y1, x2, y2);
    }
}

void CircuitPlotter::drawText(int x, int y, const std::string& text) {
    // Simple text rendering - in a full implementation, you'd use SDL_ttf
    // For now, we'll just draw a small rectangle as a placeholder
    if (renderer) {
        SDL_Rect text_rect = {x, y - 5, static_cast<int>(text.length() * 8), 10};
        SDL_RenderFillRect(renderer.get(), &text_rect);
    }
}

void CircuitPlotter::drawCircle(int x, int y, int radius) {
    if (renderer) {
        // Simple circle approximation using multiple lines
        for (int i = 0; i < 360; i += 10) {
            double angle1 = i * M_PI / 180.0;
            double angle2 = (i + 10) * M_PI / 180.0;
            
            int x1 = x + radius * cos(angle1);
            int y1 = y + radius * sin(angle1);
            int x2 = x + radius * cos(angle2);
            int y2 = y + radius * sin(angle2);
            
            drawLine(x1, y1, x2, y2);
        }
    }
}

std::pair<int, int> CircuitPlotter::worldToScreen(double x, double y, double x_min, double x_max, 
                                                 double y_min, double y_max) {
    int screen_x = config.margin + (x - x_min) * (config.width - 2 * config.margin) / (x_max - x_min);
    int screen_y = config.height - config.margin - (y - y_min) * (config.height - 2 * config.margin) / (y_max - y_min);
    return {screen_x, screen_y};
}

std::pair<double, double> CircuitPlotter::screenToWorld(int x, int y, double x_min, double x_max, 
                                                       double y_min, double y_max) {
    double world_x = x_min + (x - config.margin) * (x_max - x_min) / (config.width - 2 * config.margin);
    double world_y = y_max - (y - config.margin) * (y_max - y_min) / (config.height - 2 * config.margin);
    return {world_x, world_y};
}

// --- PlotUtils Implementation ---
namespace PlotUtils {
    std::vector<PlotData> transientResultToPlotData(const AnalysisResult& result) {
        std::vector<PlotData> plot_data;
        
        if (result.time_series.empty()) return plot_data;
        
        // Get time points from the first series
        std::vector<double> time_points;
        auto first_series = result.time_series.begin();
        if (first_series != result.time_series.end()) {
            time_points.resize(first_series->second.size());
            // Use proper time scaling based on number of points
            // If we have more than 1000 points, assume smaller timestep
            double estimated_timestep = 1e-5; // Default 10 microseconds
            if (first_series->second.size() > 1000) {
                estimated_timestep = 1e-6; // 1 microsecond for dense sampling
            } else if (first_series->second.size() < 100) {
                estimated_timestep = 1e-4; // 100 microseconds for sparse sampling
            }
            
            for (size_t i = 0; i < first_series->second.size(); ++i) {
                time_points[i] = i * estimated_timestep;
            }
        }
        
        // Create plot data for each series
        auto colors = generateColors(result.time_series.size());
        int color_index = 0;
        
        for (const auto& [series_name, values] : result.time_series) {
            auto [r, g, b] = colors[color_index % colors.size()];
            PlotData series(series_name, r, g, b);
            
            for (size_t i = 0; i < values.size() && i < time_points.size(); ++i) {
                series.addPoint(time_points[i], values[i]);
            }
            
            plot_data.push_back(std::move(series));
            color_index++;
        }
        
        return plot_data;
    }
    
    std::vector<ComplexPlotData> acResultToPlotData(const AnalysisResult& result) {
        std::vector<ComplexPlotData> plot_data;
        
        if (result.frequency_series.empty()) return plot_data;
        
        // Get frequency points from the first series
        std::vector<double> frequencies;
        auto first_series = result.frequency_series.begin();
        if (first_series != result.frequency_series.end()) {
            frequencies.resize(first_series->second.size());
            for (size_t i = 0; i < first_series->second.size(); ++i) {
                frequencies[i] = 1.0 * pow(10, i * 0.1); // Logarithmic frequency spacing
            }
        }
        
        // Create plot data for each series
        auto colors = generateColors(result.frequency_series.size());
        int color_index = 0;
        
        for (const auto& [series_name, values] : result.frequency_series) {
            auto [r, g, b] = colors[color_index % colors.size()];
            ComplexPlotData series(series_name, r, g, b);
            
            for (size_t i = 0; i < values.size() && i < frequencies.size(); ++i) {
                series.addPoint(frequencies[i], values[i]);
            }
            
            plot_data.push_back(std::move(series));
            color_index++;
        }
        
        return plot_data;
    }
    
    std::vector<PlotData> dcSweepResultToPlotData(const AnalysisResult& result) {
        std::vector<PlotData> plot_data;
        
        if (result.time_series.empty()) return plot_data;
        
        // For DC sweep, time_series contains the sweep parameter values
        auto colors = generateColors(result.time_series.size());
        int color_index = 0;
        
        for (const auto& [series_name, values] : result.time_series) {
            auto [r, g, b] = colors[color_index % colors.size()];
            PlotData series(series_name, r, g, b);
            
            for (size_t i = 0; i < values.size(); ++i) {
                // Assume sweep parameter starts at 0 and increments by 1
                series.addPoint(static_cast<double>(i), values[i]);
            }
            
            plot_data.push_back(std::move(series));
            color_index++;
        }
        
        return plot_data;
    }
    
    std::vector<std::tuple<int, int, int>> generateColors(int num_series) {
        std::vector<std::tuple<int, int, int>> colors;
        
        // Predefined colors for good visibility
        std::vector<std::tuple<int, int, int>> predefined = {
            {0, 0, 255},    // Blue
            {255, 0, 0},    // Red
            {0, 255, 0},    // Green
            {255, 0, 255},  // Magenta
            {0, 255, 255},  // Cyan
            {255, 165, 0},  // Orange
            {128, 0, 128},  // Purple
            {165, 42, 42}   // Brown
        };
        
        for (int i = 0; i < num_series; ++i) {
            colors.push_back(predefined[i % predefined.size()]);
        }
        
        return colors;
    }
    
    std::string formatNumber(double value, int precision) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        return oss.str();
    }
    
    std::string formatComplex(const std::complex<double>& value, int precision) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) 
            << value.real() << " + " << value.imag() << "j";
        return oss.str();
    }
    
    double findMin(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::min_element(values.begin(), values.end());
    }
    
    double findMax(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::max_element(values.begin(), values.end());
    }
    
    double findMin(const std::vector<std::complex<double>>& values) {
        if (values.empty()) return 0.0;
        double min_mag = std::abs(values[0]);
        for (const auto& val : values) {
            min_mag = std::min(min_mag, std::abs(val));
        }
        return min_mag;
    }
    
    double findMax(const std::vector<std::complex<double>>& values) {
        if (values.empty()) return 0.0;
        double max_mag = std::abs(values[0]);
        for (const auto& val : values) {
            max_mag = std::max(max_mag, std::abs(val));
        }
        return max_mag;
    }
}
