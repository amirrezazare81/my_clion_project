#pragma once
#include <vector>
#include <complex>
#include <map>
#include <string>
#include <memory>
#include "Circuit.h"

// --- Type Aliases ---
using ComplexMatrix = std::vector<std::vector<std::complex<double>>>;
using ComplexVector = std::vector<std::complex<double>>;

// --- Forward Declarations ---
class LinearSolver;
class ComplexLinearSolver;

// --- Analysis Types ---
enum class AnalysisType {
    DC_ANALYSIS,
    TRANSIENT_ANALYSIS,
    AC_ANALYSIS,
    DC_SWEEP
};

// --- Analysis Results ---
struct AnalysisResult {
    std::map<std::string, double> node_voltages;
    std::map<std::string, double> branch_currents;
    std::map<std::string, std::vector<double>> time_series; // For transient analysis
    std::map<std::string, std::vector<std::complex<double>>> frequency_series; // For AC analysis
    bool success;
    std::string error_message;
    
    AnalysisResult() : success(false) {}
};

// --- MNA Solver Configuration ---
struct MNASolverConfig {
    double tolerance = 1e-12;
    int max_iterations = 1000;
    bool use_lu_decomposition = true;
    double transient_timestep = 1e-6;
    double transient_end_time = 1e-3;
    double ac_start_freq = 1.0;
    double ac_end_freq = 1e6;
    int ac_points = 100;
    
    MNASolverConfig() = default;
};

// --- 6. Analysis and Solving Engine ---

class MNAMatrix {
private:
    // FIX: Renamed variables to match the .cpp file
    Matrix A_matrix;
    Vector b_vector;

public:
    MNAMatrix();
    void build(const Circuit& circuit, bool is_transient, double currentTime, double timeStepIncrement);
    const Matrix& getA() const;
    const Vector& getRHS() const;
    void reset();
};

class ComplexMNAMatrix {
private:
    ComplexMatrix A_matrix;
    ComplexVector b_vector;
public:
    void build(const Circuit& circuit, double omega, NodeIndexMap& node_map, std::map<std::string, int>& ac_source_map);
    const ComplexMatrix& getA() const;
    ComplexVector& getRHS();
};

// --- Main MNA Solver Class ---
class MNASolver {
private:
    std::unique_ptr<LinearSolver> linear_solver;
    std::unique_ptr<ComplexLinearSolver> complex_solver;
    MNASolverConfig config;
    
    // Helper methods
    AnalysisResult solveDC(const Circuit& circuit);
    AnalysisResult solveTransient(const Circuit& circuit);
    AnalysisResult solveAC(const Circuit& circuit);
    AnalysisResult solveDCSweep(const Circuit& circuit, const std::string& sweep_component, 
                               double start_value, double end_value, int steps);
    
    void updateCircuitState(Circuit& circuit, const Vector& solution, 
                           const NodeIndexMap& node_map, 
                           const std::map<std::string, int>& vs_map,
                           const std::map<std::string, int>& l_map);
    
    std::map<std::string, double> extractNodeVoltages(const Vector& solution, 
                                                     const NodeIndexMap& node_map);
    std::map<std::string, double> extractBranchCurrents(const Vector& solution, 
                                                       const NodeIndexMap& node_map,
                                                       const std::map<std::string, int>& vs_map,
                                                       const std::map<std::string, int>& l_map);

public:
    explicit MNASolver(const MNASolverConfig& config = MNASolverConfig());
    ~MNASolver() = default;
    
    // Main solving method
    AnalysisResult solve(const Circuit& circuit, AnalysisType analysis_type);
    
    // Specific analysis methods - these are already declared above
    
    // Configuration
    void setConfig(const MNASolverConfig& config);
    const MNASolverConfig& getConfig() const;
    
    // Utility methods
    bool validateCircuit(const Circuit& circuit) const;
    std::string getCircuitInfo(const Circuit& circuit) const;
};

class LinearSolver {
public:
    virtual ~LinearSolver() = default;
    virtual Vector solve(const Matrix& A, const Vector& b) const = 0;
};

class GaussianEliminationSolver : public LinearSolver {
public:
    Vector solve(const Matrix& A, const Vector& b) const override;
};

class LUDecompositionSolver : public LinearSolver {
public:
    Vector solve(const Matrix& A, const Vector& b) const override;
};

class ComplexLinearSolver {
public:
    ComplexVector solve(ComplexMatrix A, ComplexVector b) const;
};
