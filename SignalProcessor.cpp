#include "SignalProcessor.h"
#include "ErrorManager.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cmath>
#include <limits>

// --- SignalProcessor Implementation ---

std::vector<double> SignalProcessor::add(const std::vector<double>& signal1, const std::vector<double>& signal2) {
    if (!areCompatible(signal1, signal2)) {
        ErrorManager::warn("[SignalProcessor] Signals not compatible for addition");
        return {};
    }
    
    std::vector<double> result;
    result.reserve(std::min(signal1.size(), signal2.size()));
    
    for (size_t i = 0; i < std::min(signal1.size(), signal2.size()); ++i) {
        result.push_back(signal1[i] + signal2[i]);
    }
    
    return result;
}

std::vector<double> SignalProcessor::subtract(const std::vector<double>& signal1, const std::vector<double>& signal2) {
    if (!areCompatible(signal1, signal2)) {
        ErrorManager::warn("[SignalProcessor] Signals not compatible for subtraction");
        return {};
    }
    
    std::vector<double> result;
    result.reserve(std::min(signal1.size(), signal2.size()));
    
    for (size_t i = 0; i < std::min(signal1.size(), signal2.size()); ++i) {
        result.push_back(signal1[i] - signal2[i]);
    }
    
    return result;
}

std::vector<double> SignalProcessor::multiply(const std::vector<double>& signal1, const std::vector<double>& signal2) {
    if (!areCompatible(signal1, signal2)) {
        ErrorManager::warn("[SignalProcessor] Signals not compatible for multiplication");
        return {};
    }
    
    std::vector<double> result;
    result.reserve(std::min(signal1.size(), signal2.size()));
    
    for (size_t i = 0; i < std::min(signal1.size(), signal2.size()); ++i) {
        result.push_back(signal1[i] * signal2[i]);
    }
    
    return result;
}

std::vector<double> SignalProcessor::divide(const std::vector<double>& signal1, const std::vector<double>& signal2) {
    if (!areCompatible(signal1, signal2)) {
        ErrorManager::warn("[SignalProcessor] Signals not compatible for division");
        return {};
    }
    
    std::vector<double> result;
    result.reserve(std::min(signal1.size(), signal2.size()));
    
    for (size_t i = 0; i < std::min(signal1.size(), signal2.size()); ++i) {
        if (std::abs(signal2[i]) < 1e-12) {
            ErrorManager::warn("[SignalProcessor] Division by zero detected, using infinity");
            result.push_back(signal1[i] >= 0 ? std::numeric_limits<double>::infinity() : 
                                              -std::numeric_limits<double>::infinity());
        } else {
            result.push_back(signal1[i] / signal2[i]);
        }
    }
    
    return result;
}

std::vector<double> SignalProcessor::scale(const std::vector<double>& signal, double factor) {
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (double value : signal) {
        result.push_back(value * factor);
    }
    
    return result;
}

std::vector<double> SignalProcessor::offset(const std::vector<double>& signal, double dc_offset) {
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (double value : signal) {
        result.push_back(value + dc_offset);
    }
    
    return result;
}

std::vector<double> SignalProcessor::absolute(const std::vector<double>& signal) {
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (double value : signal) {
        result.push_back(std::abs(value));
    }
    
    return result;
}

std::vector<double> SignalProcessor::power(const std::vector<double>& signal, double exponent) {
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (double value : signal) {
        result.push_back(std::pow(value, exponent));
    }
    
    return result;
}

std::vector<double> SignalProcessor::log10(const std::vector<double>& signal) {
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (double value : signal) {
        if (value <= 0) {
            ErrorManager::warn("[SignalProcessor] Log10 of non-positive value, using -infinity");
            result.push_back(-std::numeric_limits<double>::infinity());
        } else {
            result.push_back(std::log10(value));
        }
    }
    
    return result;
}

std::vector<double> SignalProcessor::ln(const std::vector<double>& signal) {
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (double value : signal) {
        if (value <= 0) {
            ErrorManager::warn("[SignalProcessor] Natural log of non-positive value, using -infinity");
            result.push_back(-std::numeric_limits<double>::infinity());
        } else {
            result.push_back(std::log(value));
        }
    }
    
    return result;
}

std::vector<double> SignalProcessor::toDecibels(const std::vector<double>& signal) {
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (double value : signal) {
        if (value <= 0) {
            result.push_back(-std::numeric_limits<double>::infinity());
        } else {
            result.push_back(20.0 * std::log10(value));
        }
    }
    
    return result;
}

std::vector<double> SignalProcessor::derivative(const std::vector<double>& signal, const std::vector<double>& time_points) {
    if (signal.size() != time_points.size() || signal.size() < 2) {
        ErrorManager::warn("[SignalProcessor] Invalid signal or time data for derivative");
        return {};
    }
    
    std::vector<double> result;
    result.reserve(signal.size() - 1);
    
    for (size_t i = 0; i < signal.size() - 1; ++i) {
        double dt = time_points[i + 1] - time_points[i];
        if (std::abs(dt) < 1e-12) {
            result.push_back(0.0);
        } else {
            double dy = signal[i + 1] - signal[i];
            result.push_back(dy / dt);
        }
    }
    
    return result;
}

std::vector<double> SignalProcessor::integral(const std::vector<double>& signal, const std::vector<double>& time_points) {
    if (signal.size() != time_points.size() || signal.size() < 2) {
        ErrorManager::warn("[SignalProcessor] Invalid signal or time data for integration");
        return {};
    }
    
    std::vector<double> result;
    result.reserve(signal.size());
    result.push_back(0.0); // Initial value
    
    double integral_value = 0.0;
    for (size_t i = 0; i < signal.size() - 1; ++i) {
        double dt = time_points[i + 1] - time_points[i];
        double avg_value = (signal[i] + signal[i + 1]) * 0.5; // Trapezoidal rule
        integral_value += avg_value * dt;
        result.push_back(integral_value);
    }
    
    return result;
}

double SignalProcessor::rms(const std::vector<double>& signal) {
    if (signal.empty()) return 0.0;
    
    double sum_squares = 0.0;
    for (double value : signal) {
        sum_squares += value * value;
    }
    
    return std::sqrt(sum_squares / signal.size());
}

double SignalProcessor::average(const std::vector<double>& signal) {
    if (signal.empty()) return 0.0;
    
    double sum = std::accumulate(signal.begin(), signal.end(), 0.0);
    return sum / signal.size();
}

std::pair<double, size_t> SignalProcessor::minimum(const std::vector<double>& signal) {
    if (signal.empty()) return {0.0, 0};
    
    auto min_it = std::min_element(signal.begin(), signal.end());
    return {*min_it, static_cast<size_t>(std::distance(signal.begin(), min_it))};
}

std::pair<double, size_t> SignalProcessor::maximum(const std::vector<double>& signal) {
    if (signal.empty()) return {0.0, 0};
    
    auto max_it = std::max_element(signal.begin(), signal.end());
    return {*max_it, static_cast<size_t>(std::distance(signal.begin(), max_it))};
}

double SignalProcessor::peakToPeak(const std::vector<double>& signal) {
    if (signal.empty()) return 0.0;
    
    auto min_max = std::minmax_element(signal.begin(), signal.end());
    return *min_max.second - *min_max.first;
}

std::vector<double> SignalProcessor::movingAverage(const std::vector<double>& signal, size_t window_size) {
    if (signal.empty() || window_size == 0 || window_size > signal.size()) {
        ErrorManager::warn("[SignalProcessor] Invalid parameters for moving average");
        return signal;
    }
    
    std::vector<double> result;
    result.reserve(signal.size());
    
    for (size_t i = 0; i < signal.size(); ++i) {
        size_t start = i >= window_size / 2 ? i - window_size / 2 : 0;
        size_t end = std::min(start + window_size, signal.size());
        start = end >= window_size ? end - window_size : 0;
        
        double sum = 0.0;
        for (size_t j = start; j < end; ++j) {
            sum += signal[j];
        }
        
        result.push_back(sum / (end - start));
    }
    
    return result;
}

std::vector<double> SignalProcessor::lowPassFilter(const std::vector<double>& signal, 
                                                  const std::vector<double>& time_points, 
                                                  double cutoff_freq) {
    if (signal.size() != time_points.size() || signal.size() < 2) {
        ErrorManager::warn("[SignalProcessor] Invalid data for low-pass filter");
        return signal;
    }
    
    std::vector<double> result;
    result.reserve(signal.size());
    result.push_back(signal[0]); // Initial condition
    
    double alpha = 1.0; // Filter coefficient
    
    for (size_t i = 1; i < signal.size(); ++i) {
        double dt = time_points[i] - time_points[i - 1];
        double rc = 1.0 / (2.0 * M_PI * cutoff_freq);
        alpha = dt / (rc + dt);
        
        double filtered_value = alpha * signal[i] + (1.0 - alpha) * result[i - 1];
        result.push_back(filtered_value);
    }
    
    return result;
}

std::vector<double> SignalProcessor::highPassFilter(const std::vector<double>& signal, 
                                                   const std::vector<double>& time_points, 
                                                   double cutoff_freq) {
    if (signal.size() != time_points.size() || signal.size() < 2) {
        ErrorManager::warn("[SignalProcessor] Invalid data for high-pass filter");
        return signal;
    }
    
    std::vector<double> result;
    result.reserve(signal.size());
    result.push_back(0.0); // Initial condition (assuming no DC at start)
    
    double prev_input = signal[0];
    double prev_output = 0.0;
    
    for (size_t i = 1; i < signal.size(); ++i) {
        double dt = time_points[i] - time_points[i - 1];
        double rc = 1.0 / (2.0 * M_PI * cutoff_freq);
        double alpha = rc / (rc + dt);
        
        double filtered_value = alpha * (prev_output + signal[i] - prev_input);
        result.push_back(filtered_value);
        
        prev_input = signal[i];
        prev_output = filtered_value;
    }
    
    return result;
}

void SignalProcessor::alignSignals(std::vector<double>& signal1, std::vector<double>& signal2) {
    size_t min_size = std::min(signal1.size(), signal2.size());
    signal1.resize(min_size);
    signal2.resize(min_size);
}

std::vector<double> SignalProcessor::interpolate(const std::vector<double>& signal, 
                                                const std::vector<double>& old_time, 
                                                const std::vector<double>& new_time) {
    if (signal.size() != old_time.size() || signal.empty() || new_time.empty()) {
        ErrorManager::warn("[SignalProcessor] Invalid data for interpolation");
        return {};
    }
    
    std::vector<double> result;
    result.reserve(new_time.size());
    
    for (double t : new_time) {
        if (t <= old_time.front()) {
            result.push_back(signal.front());
        } else if (t >= old_time.back()) {
            result.push_back(signal.back());
        } else {
            // Find surrounding points
            size_t i = 0;
            while (i < old_time.size() - 1 && old_time[i + 1] < t) {
                i++;
            }
            
            double interpolated = linearInterpolation(old_time[i], signal[i], 
                                                    old_time[i + 1], signal[i + 1], t);
            result.push_back(interpolated);
        }
    }
    
    return result;
}

bool SignalProcessor::areCompatible(const std::vector<double>& signal1, const std::vector<double>& signal2) {
    return !signal1.empty() && !signal2.empty() && 
           isValid(signal1) && isValid(signal2);
}

bool SignalProcessor::isValid(const std::vector<double>& signal) {
    for (double value : signal) {
        if (std::isnan(value)) {
            return false;
        }
    }
    return true;
}

double SignalProcessor::linearInterpolation(double x1, double y1, double x2, double y2, double x) {
    if (std::abs(x2 - x1) < 1e-12) {
        return y1; // Avoid division by zero
    }
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

// --- MathOperationManager Implementation ---

void MathOperationManager::addDerivedSignal(const std::string& name, const std::vector<double>& signal, 
                                           const std::string& expression) {
    derived_signals[name] = signal;
    if (!expression.empty()) {
        signal_expressions[name] = expression;
    }
    
    ErrorManager::info("[MathOp] Added derived signal: " + name + 
                      " (" + std::to_string(signal.size()) + " points)");
}

bool MathOperationManager::removeDerivedSignal(const std::string& name) {
    bool removed = derived_signals.erase(name) > 0;
    signal_expressions.erase(name);
    
    if (removed) {
        ErrorManager::info("[MathOp] Removed derived signal: " + name);
    }
    
    return removed;
}

const std::map<std::string, std::vector<double>>& MathOperationManager::getDerivedSignals() const {
    return derived_signals;
}

const std::vector<double>* MathOperationManager::getDerivedSignal(const std::string& name) const {
    auto it = derived_signals.find(name);
    return (it != derived_signals.end()) ? &it->second : nullptr;
}

std::string MathOperationManager::getExpression(const std::string& name) const {
    auto it = signal_expressions.find(name);
    return (it != signal_expressions.end()) ? it->second : "";
}

void MathOperationManager::clear() {
    derived_signals.clear();
    signal_expressions.clear();
    ErrorManager::info("[MathOp] Cleared all derived signals");
}

bool MathOperationManager::applyOperation(const std::string& result_name, 
                                         const std::string& operation,
                                         const std::vector<std::string>& input_signals,
                                         const std::map<std::string, std::vector<double>>& available_signals,
                                         const std::vector<double>& parameters) {
    try {
        std::vector<double> result;
        std::string expression = operation;
        
        if (operation == "add" && input_signals.size() == 2) {
            auto sig1_it = available_signals.find(input_signals[0]);
            auto sig2_it = available_signals.find(input_signals[1]);
            
            if (sig1_it != available_signals.end() && sig2_it != available_signals.end()) {
                result = SignalProcessor::add(sig1_it->second, sig2_it->second);
                expression = input_signals[0] + " + " + input_signals[1];
            }
        } else if (operation == "subtract" && input_signals.size() == 2) {
            auto sig1_it = available_signals.find(input_signals[0]);
            auto sig2_it = available_signals.find(input_signals[1]);
            
            if (sig1_it != available_signals.end() && sig2_it != available_signals.end()) {
                result = SignalProcessor::subtract(sig1_it->second, sig2_it->second);
                expression = input_signals[0] + " - " + input_signals[1];
            }
        } else if (operation == "multiply" && input_signals.size() == 2) {
            auto sig1_it = available_signals.find(input_signals[0]);
            auto sig2_it = available_signals.find(input_signals[1]);
            
            if (sig1_it != available_signals.end() && sig2_it != available_signals.end()) {
                result = SignalProcessor::multiply(sig1_it->second, sig2_it->second);
                expression = input_signals[0] + " * " + input_signals[1];
            }
        } else if (operation == "scale" && input_signals.size() == 1 && !parameters.empty()) {
            auto sig_it = available_signals.find(input_signals[0]);
            
            if (sig_it != available_signals.end()) {
                result = SignalProcessor::scale(sig_it->second, parameters[0]);
                expression = std::to_string(parameters[0]) + " * " + input_signals[0];
            }
        } else if (operation == "abs" && input_signals.size() == 1) {
            auto sig_it = available_signals.find(input_signals[0]);
            
            if (sig_it != available_signals.end()) {
                result = SignalProcessor::absolute(sig_it->second);
                expression = "abs(" + input_signals[0] + ")";
            }
        } else if (operation == "power" && input_signals.size() == 1) {
            auto sig_it = available_signals.find(input_signals[0]);
            double exponent = parameters.empty() ? 2.0 : parameters[0];
            
            if (sig_it != available_signals.end()) {
                result = SignalProcessor::power(sig_it->second, exponent);
                expression = input_signals[0] + "^" + std::to_string(exponent);
            }
        } else if (operation == "rms" && input_signals.size() == 1) {
            auto sig_it = available_signals.find(input_signals[0]);
            
            if (sig_it != available_signals.end()) {
                double rms_value = SignalProcessor::rms(sig_it->second);
                result = std::vector<double>(sig_it->second.size(), rms_value);
                expression = "rms(" + input_signals[0] + ")";
            }
        }
        
        if (!result.empty()) {
            addDerivedSignal(result_name, result, expression);
            return true;
        } else {
            ErrorManager::warn("[MathOp] Operation '" + operation + "' failed or not supported");
            return false;
        }
        
    } catch (const std::exception& e) {
        ErrorManager::warn("[MathOp] Operation failed: " + std::string(e.what()));
        return false;
    }
}
