#pragma once
#include <vector>
#include <complex>
#include <string>
#include <map>
#include <memory>
#include "Solvers.h"
#include <SDL.h>

// Forward declarations
struct SDL_Renderer;
struct SDL_Window;

// --- Plot Types ---
enum class PlotType {
    TIME_SERIES,      // For transient analysis
    FREQUENCY_RESPONSE, // For AC analysis
    DC_SWEEP,         // For DC sweep analysis
    NODE_VOLTAGES,    // Bar chart of node voltages
    BRANCH_CURRENTS   // Bar chart of branch currents
};

// --- Plot Configuration ---
struct PlotConfig {
    int width = 800;
    int height = 600;
    std::string title = "Circuit Analysis Results";
    bool show_grid = true;
    bool show_legend = true;
    std::string x_label = "X Axis";
    std::string y_label = "Y Axis";
    double margin = 50.0;
    int grid_lines = 10;
    
    // Colors (RGB format)
    struct {
        int r = 255, g = 255, b = 255;  // Background
    } background;
    
    struct {
        int r = 0, g = 0, b = 0;        // Grid
    } grid_color;
    
    struct {
        int r = 0, g = 0, b = 255;      // Plot lines
    } plot_color;
    
    PlotConfig() = default;
};

// --- Data Point ---
struct DataPoint {
    double x;
    double y;
    std::string label;
    
    DataPoint(double x_val, double y_val, const std::string& lbl = "") 
        : x(x_val), y(y_val), label(lbl) {}
};

// --- Complex Data Point ---
struct ComplexDataPoint {
    double x;
    std::complex<double> y;
    std::string label;
    
    ComplexDataPoint(double x_val, const std::complex<double>& y_val, const std::string& lbl = "") 
        : x(x_val), y(y_val), label(lbl) {}
};

// --- Plot Data ---
struct PlotData {
    std::vector<DataPoint> points;
    std::string series_name;
    int color_r, color_g, color_b;
    
    PlotData(const std::string& name, int r = 0, int g = 0, int b = 255) 
        : series_name(name), color_r(r), color_g(g), color_b(b) {}
    
    void addPoint(double x, double y, const std::string& label = "") {
        points.emplace_back(x, y, label);
    }
    
    void clear() { points.clear(); }
};

// --- Complex Plot Data ---
struct ComplexPlotData {
    std::vector<ComplexDataPoint> points;
    std::string series_name;
    int color_r, color_g, color_b;
    
    ComplexPlotData(const std::string& name, int r = 0, int g = 0, int b = 255) 
        : series_name(name), color_r(r), color_g(g), color_b(b) {}
    
    void addPoint(double x, const std::complex<double>& y, const std::string& label = "") {
        points.emplace_back(x, y, label);
    }
    
    void clear() { points.clear(); }
};

// --- Main Plotter Class ---
class CircuitPlotter {
private:
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window;
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer;
    PlotConfig config;
    
    // Helper methods
    void drawGrid();
    void drawAxis();
    void drawLegend(const std::vector<std::string>& series_names);
    void drawTimeSeries(const std::vector<PlotData>& data);
    void drawFrequencyResponse(const std::vector<ComplexPlotData>& data);
    void drawDCSweep(const std::vector<PlotData>& data);
    void drawBarChart(const std::map<std::string, double>& data, const std::string& title);
    
    // Utility methods
    void setColor(int r, int g, int b, int a = 255);
    void drawLine(int x1, int y1, int x2, int y2);
    void drawText(int x, int y, const std::string& text);
    void drawCircle(int x, int y, int radius);
    
    // Coordinate transformations
    std::pair<int, int> worldToScreen(double x, double y, double x_min, double x_max, 
                                     double y_min, double y_max);
    std::pair<double, double> screenToWorld(int x, int y, double x_min, double x_max, 
                                           double y_min, double y_max);

public:
    explicit CircuitPlotter(const PlotConfig& config = PlotConfig());
    ~CircuitPlotter();
    
    // Plot different types of analysis results
    void plotTransientResults(const AnalysisResult& result);
    void plotACResults(const AnalysisResult& result);
    void plotDCSweepResults(const AnalysisResult& result);
    void plotDCResults(const AnalysisResult& result);
    
    // Generic plotting methods
    void plotTimeSeries(const std::vector<PlotData>& data, const std::string& title = "");
    void plotFrequencyResponse(const std::vector<ComplexPlotData>& data, const std::string& title = "");
    void plotDCSweep(const std::vector<PlotData>& data, const std::string& title = "");
    void plotBarChart(const std::map<std::string, double>& data, const std::string& title = "");
    
    // Display control
    void show();
    void hide();
    void clear();
    void update();
    bool isRunning() const;
    
    // Configuration
    void setConfig(const PlotConfig& new_config);
    const PlotConfig& getConfig() const;
    
    // Export functionality
    bool saveToFile(const std::string& filename);
    bool exportToCSV(const std::string& filename, const std::vector<PlotData>& data);
    bool exportToCSV(const std::string& filename, const std::vector<ComplexPlotData>& data);
};

// --- Utility Functions ---
namespace PlotUtils {
    // Convert analysis results to plot data
    std::vector<PlotData> transientResultToPlotData(const AnalysisResult& result);
    std::vector<ComplexPlotData> acResultToPlotData(const AnalysisResult& result);
    std::vector<PlotData> dcSweepResultToPlotData(const AnalysisResult& result);
    
    // Generate colors for multiple series
    std::vector<std::tuple<int, int, int>> generateColors(int num_series);
    
    // Format numbers for display
    std::string formatNumber(double value, int precision = 3);
    std::string formatComplex(const std::complex<double>& value, int precision = 3);
    
    // Statistical functions
    double findMin(const std::vector<double>& values);
    double findMax(const std::vector<double>& values);
    double findMin(const std::vector<std::complex<double>>& values);
    double findMax(const std::vector<std::complex<double>>& values);
}
