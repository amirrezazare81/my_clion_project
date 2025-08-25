#pragma once

#include <SDL.h>
#include <vector>
#include <string>
#include <map>

/**
 * @brief Represents a cursor for interactive plot analysis
 */
class PlotCursor {
private:
    double x_position;          // X coordinate in data units
    double y_position;          // Y coordinate in data units (calculated from nearest point)
    bool enabled;               // Whether cursor is active
    bool dragging;              // Whether cursor is being dragged
    SDL_Color color;            // Cursor line color
    std::string label;          // Cursor identifier
    int screen_x;               // Screen X coordinate for rendering
    int screen_y;               // Screen Y coordinate for rendering
    
public:
    PlotCursor(const std::string& cursor_label = "C1", SDL_Color cursor_color = {255, 255, 0, 255});
    
    // Position management
    void setPosition(double x, double y);
    void setXPosition(double x);
    double getXPosition() const { return x_position; }
    double getYPosition() const { return y_position; }
    
    // Screen coordinates (for rendering)
    void setScreenPosition(int x, int y);
    int getScreenX() const { return screen_x; }
    int getScreenY() const { return screen_y; }
    
    // State management
    void setEnabled(bool state) { enabled = state; }
    bool isEnabled() const { return enabled; }
    void setDragging(bool state) { dragging = state; }
    bool isDragging() const { return dragging; }
    
    // Appearance
    void setColor(SDL_Color cursor_color) { color = cursor_color; }
    SDL_Color getColor() const { return color; }
    void setLabel(const std::string& cursor_label) { label = cursor_label; }
    std::string getLabel() const { return label; }
    
    // Interaction
    bool isNear(int mouse_x, int mouse_y, int tolerance = 5) const;
    void updateFromMouse(int mouse_x, int mouse_y, const SDL_Rect& plot_area, 
                        double x_min, double x_max, double y_min, double y_max);
    
    // Value extraction from signals
    double interpolateValue(const std::vector<double>& x_data, 
                           const std::vector<double>& y_data) const;
    std::map<std::string, double> getSignalValues(
        const std::vector<double>& x_data,
        const std::map<std::string, std::vector<double>>& signal_data) const;
};

/**
 * @brief Manages multiple cursors for advanced plot analysis
 */
class CursorManager {
private:
    std::vector<PlotCursor> cursors;
    int active_cursor_index;    // Currently selected cursor (-1 if none)
    bool double_cursor_mode;    // Whether dual cursor mode is enabled
    
    // Plot area information
    SDL_Rect plot_area;
    double x_min, x_max, y_min, y_max;
    
    // Data references
    std::vector<double> x_data;
    std::map<std::string, std::vector<double>> signal_data;
    
public:
    CursorManager();
    
    // Cursor management
    void addCursor(const std::string& label = "", SDL_Color color = {255, 255, 0, 255});
    void removeCursor(size_t index);
    void clearCursors();
    size_t getCursorCount() const { return cursors.size(); }
    
    // Mode management
    void setDoubleCursorMode(bool enabled);
    bool isDoubleCursorMode() const { return double_cursor_mode; }
    void toggleDoubleCursorMode();
    
    // Data binding
    void setPlotArea(const SDL_Rect& area) { plot_area = area; }
    void setAxisRanges(double xmin, double xmax, double ymin, double ymax);
    void setData(const std::vector<double>& x_values, 
                const std::map<std::string, std::vector<double>>& signals);
    
    // Interaction handling
    bool handleMouseDown(int mouse_x, int mouse_y);
    bool handleMouseMove(int mouse_x, int mouse_y);
    bool handleMouseUp(int mouse_x, int mouse_y);
    bool handleKeyPress(SDL_Scancode key);
    
    // Cursor access
    PlotCursor* getCursor(size_t index);
    const PlotCursor* getCursor(size_t index) const;
    PlotCursor* getActiveCursor();
    int getActiveCursorIndex() const { return active_cursor_index; }
    
    // Measurements (for double cursor mode)
    struct CursorMeasurement {
        double delta_x;
        double delta_y;
        double frequency;      // 1/delta_x (if applicable)
        double slope;          // delta_y/delta_x
        std::map<std::string, double> signal_differences;
    };
    
    CursorMeasurement getMeasurement() const;
    
    // Value readouts
    std::map<std::string, std::map<std::string, double>> getAllCursorValues() const;
    std::vector<std::string> getCursorInfoStrings() const;
    
    // Rendering support
    void updateScreenPositions();
    std::vector<PlotCursor*> getEnabledCursors();
    
    // Automatic cursor placement
    void placeCursorAtPeak(size_t cursor_index, const std::string& signal_name, bool find_maximum = true);
    void placeCursorAtValue(size_t cursor_index, double target_value, const std::string& signal_name);
    void placeCursorAtTime(size_t cursor_index, double time);
    
private:
    void updateActiveCursor(int mouse_x, int mouse_y);
    size_t findNearestDataPoint(double x_position) const;
    void ensureMinimumCursors();
    void moveCursorByDataPoint(int direction);
};

/**
 * @brief Utility class for cursor-based measurements and analysis
 */
class CursorAnalyzer {
public:
    // Measurement calculations
    static double calculateFrequency(double time_period);
    static double calculatePhaseDifference(const PlotCursor& cursor1, const PlotCursor& cursor2,
                                         const std::vector<double>& time_data,
                                         const std::vector<double>& signal_data);
    
    // Statistical analysis between cursors
    static double calculateRMS(const std::vector<double>& signal_data,
                              const std::vector<double>& time_data,
                              double start_time, double end_time);
    
    static double calculateAverage(const std::vector<double>& signal_data,
                                  const std::vector<double>& time_data,
                                  double start_time, double end_time);
    
    static double calculatePeakToPeak(const std::vector<double>& signal_data,
                                     const std::vector<double>& time_data,
                                     double start_time, double end_time);
    
    // Rise/fall time analysis
    static double calculateRiseTime(const std::vector<double>& signal_data,
                                   const std::vector<double>& time_data,
                                   double start_level = 0.1, double end_level = 0.9);
    
    static double calculateFallTime(const std::vector<double>& signal_data,
                                   const std::vector<double>& time_data,
                                   double start_level = 0.9, double end_level = 0.1);
    
    // Delay measurements
    static double calculatePropagationDelay(const std::vector<double>& input_signal,
                                           const std::vector<double>& output_signal,
                                           const std::vector<double>& time_data,
                                           double threshold = 0.5);
    
private:
    static size_t findTimeIndex(const std::vector<double>& time_data, double target_time);
    static size_t findLevelCrossing(const std::vector<double>& signal_data, 
                                   double level, size_t start_index = 0, bool rising = true);
};
