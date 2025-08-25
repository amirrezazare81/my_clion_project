#pragma once

#include <vector>
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <cmath>

/**
 * @brief Mathematical operations and signal processing utilities
 * 
 * Provides functionality for performing mathematical operations on signals
 * such as addition, subtraction, multiplication, FFT, filtering, etc.
 */
class SignalProcessor {
public:
    // --- Basic Mathematical Operations ---
    
    /**
     * @brief Add two signals element-wise
     */
    static std::vector<double> add(const std::vector<double>& signal1, const std::vector<double>& signal2);
    
    /**
     * @brief Subtract two signals element-wise (signal1 - signal2)
     */
    static std::vector<double> subtract(const std::vector<double>& signal1, const std::vector<double>& signal2);
    
    /**
     * @brief Multiply two signals element-wise
     */
    static std::vector<double> multiply(const std::vector<double>& signal1, const std::vector<double>& signal2);
    
    /**
     * @brief Divide two signals element-wise (signal1 / signal2)
     */
    static std::vector<double> divide(const std::vector<double>& signal1, const std::vector<double>& signal2);
    
    /**
     * @brief Scale a signal by a constant factor
     */
    static std::vector<double> scale(const std::vector<double>& signal, double factor);
    
    /**
     * @brief Add a DC offset to a signal
     */
    static std::vector<double> offset(const std::vector<double>& signal, double dc_offset);
    
    // --- Advanced Mathematical Operations ---
    
    /**
     * @brief Calculate absolute value of each signal element
     */
    static std::vector<double> absolute(const std::vector<double>& signal);
    
    /**
     * @brief Calculate power (square) of each signal element
     */
    static std::vector<double> power(const std::vector<double>& signal, double exponent = 2.0);
    
    /**
     * @brief Calculate logarithm (base 10) of each signal element
     */
    static std::vector<double> log10(const std::vector<double>& signal);
    
    /**
     * @brief Calculate natural logarithm of each signal element
     */
    static std::vector<double> ln(const std::vector<double>& signal);
    
    /**
     * @brief Calculate decibel conversion (20*log10(signal))
     */
    static std::vector<double> toDecibels(const std::vector<double>& signal);
    
    /**
     * @brief Calculate derivative (discrete differential)
     */
    static std::vector<double> derivative(const std::vector<double>& signal, const std::vector<double>& time_points);
    
    /**
     * @brief Calculate integral (discrete integration using trapezoidal rule)
     */
    static std::vector<double> integral(const std::vector<double>& signal, const std::vector<double>& time_points);
    
    // --- Signal Analysis ---
    
    /**
     * @brief Calculate RMS (Root Mean Square) value
     */
    static double rms(const std::vector<double>& signal);
    
    /**
     * @brief Calculate average (mean) value
     */
    static double average(const std::vector<double>& signal);
    
    /**
     * @brief Find minimum value and its index
     */
    static std::pair<double, size_t> minimum(const std::vector<double>& signal);
    
    /**
     * @brief Find maximum value and its index
     */
    static std::pair<double, size_t> maximum(const std::vector<double>& signal);
    
    /**
     * @brief Calculate peak-to-peak value
     */
    static double peakToPeak(const std::vector<double>& signal);
    
    // --- Filtering Operations ---
    
    /**
     * @brief Simple moving average filter
     */
    static std::vector<double> movingAverage(const std::vector<double>& signal, size_t window_size);
    
    /**
     * @brief Low-pass filter (simple RC filter approximation)
     */
    static std::vector<double> lowPassFilter(const std::vector<double>& signal, 
                                           const std::vector<double>& time_points, 
                                           double cutoff_freq);
    
    /**
     * @brief High-pass filter (simple RC filter approximation) 
     */
    static std::vector<double> highPassFilter(const std::vector<double>& signal, 
                                            const std::vector<double>& time_points, 
                                            double cutoff_freq);
    
    // --- Utility Functions ---
    
    /**
     * @brief Resize signals to match the shortest length
     */
    static void alignSignals(std::vector<double>& signal1, std::vector<double>& signal2);
    
    /**
     * @brief Interpolate signal to new time points
     */
    static std::vector<double> interpolate(const std::vector<double>& signal, 
                                         const std::vector<double>& old_time, 
                                         const std::vector<double>& new_time);
    
    /**
     * @brief Check if signals are compatible for operations
     */
    static bool areCompatible(const std::vector<double>& signal1, const std::vector<double>& signal2);
    
    /**
     * @brief Validate signal (check for NaN, infinite values)
     */
    static bool isValid(const std::vector<double>& signal);
    
private:
    // Helper functions
    static double linearInterpolation(double x1, double y1, double x2, double y2, double x);
};

/**
 * @brief Expression evaluator for mathematical signal operations
 * 
 * Allows users to define mathematical expressions like "V(N1) + 2*V(N2) - 0.5"
 * and evaluate them on signal data.
 */
class SignalExpression {
private:
    std::string expression;
    std::map<std::string, std::vector<double>> signal_variables;
    
public:
    SignalExpression(const std::string& expr);
    
    /**
     * @brief Set signal variables for the expression
     */
    void setVariables(const std::map<std::string, std::vector<double>>& variables);
    
    /**
     * @brief Evaluate the expression and return the result signal
     */
    std::vector<double> evaluate();
    
    /**
     * @brief Get list of variables used in the expression
     */
    std::vector<std::string> getVariableNames() const;
    
    /**
     * @brief Validate expression syntax
     */
    bool isValid() const;
    
private:
    // Parser helper functions
    std::vector<std::string> tokenize(const std::string& expr) const;
    bool isOperator(const std::string& token) const;
    bool isFunction(const std::string& token) const;
    double evaluateConstant(const std::string& token) const;
};

/**
 * @brief Management class for mathematical signal operations
 */
class MathOperationManager {
private:
    std::map<std::string, std::vector<double>> derived_signals;
    std::map<std::string, std::string> signal_expressions;
    
public:
    /**
     * @brief Add a derived signal from mathematical operation
     */
    void addDerivedSignal(const std::string& name, const std::vector<double>& signal, 
                         const std::string& expression = "");
    
    /**
     * @brief Remove a derived signal
     */
    bool removeDerivedSignal(const std::string& name);
    
    /**
     * @brief Get all derived signals
     */
    const std::map<std::string, std::vector<double>>& getDerivedSignals() const;
    
    /**
     * @brief Get a specific derived signal
     */
    const std::vector<double>* getDerivedSignal(const std::string& name) const;
    
    /**
     * @brief Get the expression used to create a derived signal
     */
    std::string getExpression(const std::string& name) const;
    
    /**
     * @brief Clear all derived signals
     */
    void clear();
    
    /**
     * @brief Apply mathematical operation to create new derived signal
     */
    bool applyOperation(const std::string& result_name, 
                       const std::string& operation,
                       const std::vector<std::string>& input_signals,
                       const std::map<std::string, std::vector<double>>& available_signals,
                       const std::vector<double>& parameters = {});
};
