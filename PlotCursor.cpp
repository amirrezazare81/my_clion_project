#include "PlotCursor.h"
#include "ErrorManager.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

// --- PlotCursor Implementation ---

PlotCursor::PlotCursor(const std::string& cursor_label, SDL_Color cursor_color)
    : x_position(0.0), y_position(0.0), enabled(true), dragging(false), 
      color(cursor_color), label(cursor_label), screen_x(0), screen_y(0) {}

void PlotCursor::setPosition(double x, double y) {
    x_position = x;
    y_position = y;
}

void PlotCursor::setXPosition(double x) {
    x_position = x;
}

void PlotCursor::setScreenPosition(int x, int y) {
    screen_x = x;
    screen_y = y;
}

bool PlotCursor::isNear(int mouse_x, int mouse_y, int tolerance) const {
    int dx = mouse_x - screen_x;
    int dy = mouse_y - screen_y;
    return (dx * dx + dy * dy) <= (tolerance * tolerance);
}

void PlotCursor::updateFromMouse(int mouse_x, int mouse_y, const SDL_Rect& plot_area, 
                                double x_min, double x_max, double y_min, double y_max) {
    // Convert mouse position to data coordinates
    if (plot_area.w > 0 && plot_area.h > 0) {
        double x_ratio = static_cast<double>(mouse_x - plot_area.x) / plot_area.w;
        double y_ratio = static_cast<double>(plot_area.y + plot_area.h - mouse_y) / plot_area.h;
        
        x_position = x_min + x_ratio * (x_max - x_min);
        y_position = y_min + y_ratio * (y_max - y_min);
        
        // Update screen position
        screen_x = mouse_x;
        screen_y = mouse_y;
    }
}

double PlotCursor::interpolateValue(const std::vector<double>& x_data, 
                                   const std::vector<double>& y_data) const {
    if (x_data.size() != y_data.size() || x_data.empty()) {
        return 0.0;
    }
    
    // Find surrounding points
    if (x_position <= x_data.front()) {
        return y_data.front();
    }
    if (x_position >= x_data.back()) {
        return y_data.back();
    }
    
    // Binary search for efficiency
    size_t left = 0, right = x_data.size() - 1;
    while (right - left > 1) {
        size_t mid = (left + right) / 2;
        if (x_data[mid] <= x_position) {
            left = mid;
        } else {
            right = mid;
        }
    }
    
    // Linear interpolation
    double x1 = x_data[left], x2 = x_data[right];
    double y1 = y_data[left], y2 = y_data[right];
    
    if (std::abs(x2 - x1) < 1e-12) {
        return y1;
    }
    
    return y1 + (y2 - y1) * (x_position - x1) / (x2 - x1);
}

std::map<std::string, double> PlotCursor::getSignalValues(
    const std::vector<double>& x_data,
    const std::map<std::string, std::vector<double>>& signal_data) const {
    
    std::map<std::string, double> values;
    
    for (const auto& signal : signal_data) {
        if (signal.second.size() == x_data.size()) {
            values[signal.first] = interpolateValue(x_data, signal.second);
        }
    }
    
    return values;
}

// --- CursorManager Implementation ---

CursorManager::CursorManager() 
    : active_cursor_index(-1), double_cursor_mode(false),
      x_min(0.0), x_max(1.0), y_min(0.0), y_max(1.0) {
    plot_area = {0, 0, 100, 100};
}

void CursorManager::addCursor(const std::string& label, SDL_Color color) {
    std::string cursor_label = label;
    if (cursor_label.empty()) {
        cursor_label = "C" + std::to_string(cursors.size() + 1);
    }
    
    cursors.emplace_back(cursor_label, color);
    
    // Position new cursor at center of plot
    double center_x = (x_min + x_max) * 0.5;
    double center_y = (y_min + y_max) * 0.5;
    cursors.back().setPosition(center_x, center_y);
    
    ErrorManager::info("[Cursor] Added cursor: " + cursor_label);
}

void CursorManager::removeCursor(size_t index) {
    if (index < cursors.size()) {
        std::string label = cursors[index].getLabel();
        cursors.erase(cursors.begin() + index);
        
        // Adjust active cursor index
        if (active_cursor_index == static_cast<int>(index)) {
            active_cursor_index = -1;
        } else if (active_cursor_index > static_cast<int>(index)) {
            active_cursor_index--;
        }
        
        ErrorManager::info("[Cursor] Removed cursor: " + label);
    }
}

void CursorManager::clearCursors() {
    cursors.clear();
    active_cursor_index = -1;
    ErrorManager::info("[Cursor] Cleared all cursors");
}

void CursorManager::setDoubleCursorMode(bool enabled) {
    double_cursor_mode = enabled;
    
    if (enabled) {
        // Ensure we have at least 2 cursors
        while (cursors.size() < 2) {
            SDL_Color colors[] = {{255, 255, 0, 255}, {0, 255, 255, 255}};
            addCursor("", colors[cursors.size() % 2]);
        }
        
        // Enable first two cursors
        for (size_t i = 0; i < std::min(cursors.size(), size_t(2)); ++i) {
            cursors[i].setEnabled(true);
        }
    }
    
    ErrorManager::info("[Cursor] Double cursor mode: " + std::string(enabled ? "ON" : "OFF"));
}

void CursorManager::toggleDoubleCursorMode() {
    setDoubleCursorMode(!double_cursor_mode);
}

void CursorManager::setAxisRanges(double xmin, double xmax, double ymin, double ymax) {
    x_min = xmin;
    x_max = xmax;
    y_min = ymin;
    y_max = ymax;
}

void CursorManager::setData(const std::vector<double>& x_values, 
                           const std::map<std::string, std::vector<double>>& signals) {
    x_data = x_values;
    signal_data = signals;
}

bool CursorManager::handleMouseDown(int mouse_x, int mouse_y) {
    // Check if clicking near any existing cursor
    for (size_t i = 0; i < cursors.size(); ++i) {
        if (cursors[i].isEnabled() && cursors[i].isNear(mouse_x, mouse_y, 8)) {
            active_cursor_index = static_cast<int>(i);
            cursors[i].setDragging(true);
            return true;
        }
    }
    
    // If in double cursor mode and we have less than 2 cursors, add one
    if (double_cursor_mode && cursors.size() < 2) {
        SDL_Color colors[] = {{255, 255, 0, 255}, {0, 255, 255, 255}};
        addCursor("", colors[cursors.size()]);
        active_cursor_index = static_cast<int>(cursors.size() - 1);
        cursors.back().setDragging(true);
        updateActiveCursor(mouse_x, mouse_y);
        return true;
    }
    
    // Otherwise, move active cursor to mouse position
    if (!cursors.empty()) {
        if (active_cursor_index < 0 || active_cursor_index >= static_cast<int>(cursors.size())) {
            active_cursor_index = 0;
        }
        
        cursors[active_cursor_index].setDragging(true);
        updateActiveCursor(mouse_x, mouse_y);
        return true;
    }
    
    return false;
}

bool CursorManager::handleMouseMove(int mouse_x, int mouse_y) {
    if (active_cursor_index >= 0 && active_cursor_index < static_cast<int>(cursors.size())) {
        PlotCursor& cursor = cursors[active_cursor_index];
        if (cursor.isDragging()) {
            updateActiveCursor(mouse_x, mouse_y);
            return true;
        }
    }
    return false;
}

bool CursorManager::handleMouseUp(int mouse_x, int mouse_y) {
    bool was_dragging = false;
    
    for (auto& cursor : cursors) {
        if (cursor.isDragging()) {
            cursor.setDragging(false);
            was_dragging = true;
        }
    }
    
    return was_dragging;
}

bool CursorManager::handleKeyPress(SDL_Scancode key) {
    switch (key) {
        case SDL_SCANCODE_C:
            if (cursors.empty()) {
                addCursor();
            } else {
                toggleDoubleCursorMode();
            }
            return true;
            
        case SDL_SCANCODE_DELETE:
        case SDL_SCANCODE_BACKSPACE:
            if (active_cursor_index >= 0 && active_cursor_index < static_cast<int>(cursors.size())) {
                removeCursor(active_cursor_index);
            }
            return true;
            
        case SDL_SCANCODE_TAB:
            if (!cursors.empty()) {
                active_cursor_index = (active_cursor_index + 1) % static_cast<int>(cursors.size());
            }
            return true;
            
        case SDL_SCANCODE_LEFT:
            if (active_cursor_index >= 0 && active_cursor_index < static_cast<int>(cursors.size())) {
                moveCursorByDataPoint(-1);
            }
            return true;
            
        case SDL_SCANCODE_RIGHT:
            if (active_cursor_index >= 0 && active_cursor_index < static_cast<int>(cursors.size())) {
                moveCursorByDataPoint(1);
            }
            return true;
            
        default:
            return false;
    }
}

PlotCursor* CursorManager::getCursor(size_t index) {
    return (index < cursors.size()) ? &cursors[index] : nullptr;
}

const PlotCursor* CursorManager::getCursor(size_t index) const {
    return (index < cursors.size()) ? &cursors[index] : nullptr;
}

PlotCursor* CursorManager::getActiveCursor() {
    return (active_cursor_index >= 0 && active_cursor_index < static_cast<int>(cursors.size())) 
           ? &cursors[active_cursor_index] : nullptr;
}

CursorManager::CursorMeasurement CursorManager::getMeasurement() const {
    CursorMeasurement measurement;
    measurement.delta_x = 0.0;
    measurement.delta_y = 0.0;
    measurement.frequency = 0.0;
    measurement.slope = 0.0;
    
    if (cursors.size() >= 2) {
        const PlotCursor& cursor1 = cursors[0];
        const PlotCursor& cursor2 = cursors[1];
        
        measurement.delta_x = cursor2.getXPosition() - cursor1.getXPosition();
        measurement.delta_y = cursor2.getYPosition() - cursor1.getYPosition();
        
        if (std::abs(measurement.delta_x) > 1e-12) {
            measurement.frequency = 1.0 / std::abs(measurement.delta_x);
            measurement.slope = measurement.delta_y / measurement.delta_x;
        }
        
        // Calculate signal differences
        auto values1 = cursor1.getSignalValues(x_data, signal_data);
        auto values2 = cursor2.getSignalValues(x_data, signal_data);
        
        for (const auto& signal : values1) {
            if (values2.count(signal.first)) {
                measurement.signal_differences[signal.first] = values2[signal.first] - signal.second;
            }
        }
    }
    
    return measurement;
}

std::map<std::string, std::map<std::string, double>> CursorManager::getAllCursorValues() const {
    std::map<std::string, std::map<std::string, double>> all_values;
    
    for (const auto& cursor : cursors) {
        if (cursor.isEnabled()) {
            all_values[cursor.getLabel()] = cursor.getSignalValues(x_data, signal_data);
        }
    }
    
    return all_values;
}

std::vector<std::string> CursorManager::getCursorInfoStrings() const {
    std::vector<std::string> info_strings;
    
    for (const auto& cursor : cursors) {
        if (cursor.isEnabled()) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(3);
            ss << cursor.getLabel() << ": X=" << cursor.getXPosition() 
               << ", Y=" << cursor.getYPosition();
            info_strings.push_back(ss.str());
        }
    }
    
    // Add measurement info for double cursor mode
    if (double_cursor_mode && cursors.size() >= 2) {
        auto measurement = getMeasurement();
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3);
        ss << "ΔX=" << measurement.delta_x << ", ΔY=" << measurement.delta_y;
        if (measurement.frequency > 0) {
            ss << ", f=" << measurement.frequency << "Hz";
        }
        info_strings.push_back(ss.str());
    }
    
    return info_strings;
}

void CursorManager::updateScreenPositions() {
    for (auto& cursor : cursors) {
        if (cursor.isEnabled()) {
            // Convert data coordinates to screen coordinates
            double x_ratio = (cursor.getXPosition() - x_min) / (x_max - x_min);
            double y_ratio = (cursor.getYPosition() - y_min) / (y_max - y_min);
            
            int screen_x = plot_area.x + static_cast<int>(x_ratio * plot_area.w);
            int screen_y = plot_area.y + plot_area.h - static_cast<int>(y_ratio * plot_area.h);
            
            cursor.setScreenPosition(screen_x, screen_y);
        }
    }
}

std::vector<PlotCursor*> CursorManager::getEnabledCursors() {
    std::vector<PlotCursor*> enabled_cursors;
    for (auto& cursor : cursors) {
        if (cursor.isEnabled()) {
            enabled_cursors.push_back(&cursor);
        }
    }
    return enabled_cursors;
}

void CursorManager::placeCursorAtPeak(size_t cursor_index, const std::string& signal_name, bool find_maximum) {
    if (cursor_index >= cursors.size() || !signal_data.count(signal_name)) {
        return;
    }
    
    const auto& signal = signal_data.at(signal_name);
    if (signal.empty() || signal.size() != x_data.size()) {
        return;
    }
    
    auto peak_it = find_maximum ? std::max_element(signal.begin(), signal.end()) 
                                : std::min_element(signal.begin(), signal.end());
    
    size_t peak_index = std::distance(signal.begin(), peak_it);
    
    cursors[cursor_index].setPosition(x_data[peak_index], *peak_it);
    
    ErrorManager::info("[Cursor] Placed " + cursors[cursor_index].getLabel() + 
                      " at " + (find_maximum ? "maximum" : "minimum") + " of " + signal_name);
}

void CursorManager::placeCursorAtTime(size_t cursor_index, double time) {
    if (cursor_index >= cursors.size()) {
        return;
    }
    
    // Find nearest time point
    size_t nearest_index = findNearestDataPoint(time);
    if (nearest_index < x_data.size()) {
        double y_value = 0.0;
        if (!signal_data.empty()) {
            // Use first available signal for Y coordinate
            y_value = signal_data.begin()->second[nearest_index];
        }
        
        cursors[cursor_index].setPosition(x_data[nearest_index], y_value);
    }
}

void CursorManager::updateActiveCursor(int mouse_x, int mouse_y) {
    if (active_cursor_index >= 0 && active_cursor_index < static_cast<int>(cursors.size())) {
        cursors[active_cursor_index].updateFromMouse(mouse_x, mouse_y, plot_area, 
                                                    x_min, x_max, y_min, y_max);
    }
}

size_t CursorManager::findNearestDataPoint(double x_position) const {
    if (x_data.empty()) {
        return 0;
    }
    
    auto lower = std::lower_bound(x_data.begin(), x_data.end(), x_position);
    
    if (lower == x_data.end()) {
        return x_data.size() - 1;
    }
    if (lower == x_data.begin()) {
        return 0;
    }
    
    // Check which is closer
    auto prev = lower - 1;
    if (std::abs(*lower - x_position) < std::abs(*prev - x_position)) {
        return std::distance(x_data.begin(), lower);
    } else {
        return std::distance(x_data.begin(), prev);
    }
}

void CursorManager::moveCursorByDataPoint(int direction) {
    if (active_cursor_index < 0 || active_cursor_index >= static_cast<int>(cursors.size())) {
        return;
    }
    
    PlotCursor& cursor = cursors[active_cursor_index];
    size_t current_index = findNearestDataPoint(cursor.getXPosition());
    
    if (direction > 0 && current_index < x_data.size() - 1) {
        current_index++;
    } else if (direction < 0 && current_index > 0) {
        current_index--;
    }
    
    if (current_index < x_data.size()) {
        double y_value = cursor.getYPosition();
        if (!signal_data.empty()) {
            // Update Y to match current signal if available
            y_value = signal_data.begin()->second[current_index];
        }
        cursor.setPosition(x_data[current_index], y_value);
    }
}

void CursorManager::ensureMinimumCursors() {
    if (cursors.empty()) {
        addCursor();
    }
}

void CursorManager::placeCursorAtValue(size_t cursor_index, double target_value, const std::string& signal_name) {
    if (cursor_index >= cursors.size() || !signal_data.count(signal_name)) {
        return;
    }
    
    const auto& signal = signal_data.at(signal_name);
    if (signal.empty() || signal.size() != x_data.size()) {
        return;
    }
    
    // Find the data point closest to target value
    size_t best_index = 0;
    double min_diff = std::abs(signal[0] - target_value);
    
    for (size_t i = 1; i < signal.size(); ++i) {
        double diff = std::abs(signal[i] - target_value);
        if (diff < min_diff) {
            min_diff = diff;
            best_index = i;
        }
    }
    
    cursors[cursor_index].setPosition(x_data[best_index], signal[best_index]);
    
    ErrorManager::info("[Cursor] Placed " + cursors[cursor_index].getLabel() + 
                      " at value " + std::to_string(target_value) + " on " + signal_name);
}

// --- CursorAnalyzer Implementation ---

double CursorAnalyzer::calculateFrequency(double time_period) {
    return (time_period > 0) ? 1.0 / time_period : 0.0;
}

double CursorAnalyzer::calculatePhaseDifference(const PlotCursor& cursor1, const PlotCursor& cursor2,
                                               const std::vector<double>& time_data,
                                               const std::vector<double>& signal_data) {
    double time_diff = cursor2.getXPosition() - cursor1.getXPosition();
    
    // Estimate period from signal (simplified approach)
    double estimated_period = 1.0; // Default fallback
    if (time_data.size() > 100) {
        estimated_period = (time_data.back() - time_data.front()) / 10.0; // Rough estimate
    }
    
    double phase_radians = 2.0 * M_PI * time_diff / estimated_period;
    return phase_radians * 180.0 / M_PI; // Convert to degrees
}

double CursorAnalyzer::calculateRMS(const std::vector<double>& signal_data,
                                   const std::vector<double>& time_data,
                                   double start_time, double end_time) {
    if (signal_data.size() != time_data.size() || signal_data.empty()) {
        return 0.0;
    }
    
    double sum_squares = 0.0;
    int count = 0;
    
    for (size_t i = 0; i < time_data.size(); ++i) {
        if (time_data[i] >= start_time && time_data[i] <= end_time) {
            sum_squares += signal_data[i] * signal_data[i];
            count++;
        }
    }
    
    return (count > 0) ? std::sqrt(sum_squares / count) : 0.0;
}

double CursorAnalyzer::calculateAverage(const std::vector<double>& signal_data,
                                       const std::vector<double>& time_data,
                                       double start_time, double end_time) {
    if (signal_data.size() != time_data.size() || signal_data.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    int count = 0;
    
    for (size_t i = 0; i < time_data.size(); ++i) {
        if (time_data[i] >= start_time && time_data[i] <= end_time) {
            sum += signal_data[i];
            count++;
        }
    }
    
    return (count > 0) ? sum / count : 0.0;
}

double CursorAnalyzer::calculatePeakToPeak(const std::vector<double>& signal_data,
                                          const std::vector<double>& time_data,
                                          double start_time, double end_time) {
    if (signal_data.size() != time_data.size() || signal_data.empty()) {
        return 0.0;
    }
    
    double min_val = std::numeric_limits<double>::max();
    double max_val = std::numeric_limits<double>::lowest();
    bool found_any = false;
    
    for (size_t i = 0; i < time_data.size(); ++i) {
        if (time_data[i] >= start_time && time_data[i] <= end_time) {
            min_val = std::min(min_val, signal_data[i]);
            max_val = std::max(max_val, signal_data[i]);
            found_any = true;
        }
    }
    
    return found_any ? (max_val - min_val) : 0.0;
}

double CursorAnalyzer::calculateRiseTime(const std::vector<double>& signal_data,
                                        const std::vector<double>& time_data,
                                        double start_level, double end_level) {
    if (signal_data.size() != time_data.size() || signal_data.empty()) {
        return 0.0;
    }
    
    // Find min and max of signal to determine levels
    auto minmax = std::minmax_element(signal_data.begin(), signal_data.end());
    double min_val = *minmax.first;
    double max_val = *minmax.second;
    double amplitude = max_val - min_val;
    
    double start_threshold = min_val + start_level * amplitude;
    double end_threshold = min_val + end_level * amplitude;
    
    // Find crossing points
    size_t start_idx = findLevelCrossing(signal_data, start_threshold, 0, true);
    size_t end_idx = findLevelCrossing(signal_data, end_threshold, start_idx, true);
    
    if (start_idx < signal_data.size() && end_idx < signal_data.size() && end_idx > start_idx) {
        return time_data[end_idx] - time_data[start_idx];
    }
    
    return 0.0;
}

double CursorAnalyzer::calculateFallTime(const std::vector<double>& signal_data,
                                        const std::vector<double>& time_data,
                                        double start_level, double end_level) {
    if (signal_data.size() != time_data.size() || signal_data.empty()) {
        return 0.0;
    }
    
    // Find min and max of signal to determine levels
    auto minmax = std::minmax_element(signal_data.begin(), signal_data.end());
    double min_val = *minmax.first;
    double max_val = *minmax.second;
    double amplitude = max_val - min_val;
    
    double start_threshold = min_val + start_level * amplitude;
    double end_threshold = min_val + end_level * amplitude;
    
    // Find crossing points (falling edge)
    size_t start_idx = findLevelCrossing(signal_data, start_threshold, 0, false);
    size_t end_idx = findLevelCrossing(signal_data, end_threshold, start_idx, false);
    
    if (start_idx < signal_data.size() && end_idx < signal_data.size() && end_idx > start_idx) {
        return time_data[end_idx] - time_data[start_idx];
    }
    
    return 0.0;
}

double CursorAnalyzer::calculatePropagationDelay(const std::vector<double>& input_signal,
                                                const std::vector<double>& output_signal,
                                                const std::vector<double>& time_data,
                                                double threshold) {
    if (input_signal.size() != output_signal.size() || 
        input_signal.size() != time_data.size() || input_signal.empty()) {
        return 0.0;
    }
    
    // Find threshold crossings
    size_t input_crossing = findLevelCrossing(input_signal, threshold, 0, true);
    size_t output_crossing = findLevelCrossing(output_signal, threshold, 0, true);
    
    if (input_crossing < time_data.size() && output_crossing < time_data.size()) {
        return time_data[output_crossing] - time_data[input_crossing];
    }
    
    return 0.0;
}

size_t CursorAnalyzer::findTimeIndex(const std::vector<double>& time_data, double target_time) {
    auto it = std::lower_bound(time_data.begin(), time_data.end(), target_time);
    return std::distance(time_data.begin(), it);
}

size_t CursorAnalyzer::findLevelCrossing(const std::vector<double>& signal_data, 
                                        double level, size_t start_index, bool rising) {
    for (size_t i = start_index; i < signal_data.size() - 1; ++i) {
        bool crossed = rising ? (signal_data[i] <= level && signal_data[i + 1] > level)
                             : (signal_data[i] >= level && signal_data[i + 1] < level);
        if (crossed) {
            return i + 1;
        }
    }
    
    return signal_data.size(); // Not found
}
