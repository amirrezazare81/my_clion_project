#pragma once
#include <map>
#include <vector>
#include <string>
#include <complex>
#include "Circuit.h"
#include "Solvers.h"

// --- Analyzer Classes ---

class Analyzer {
public:
    virtual ~Analyzer() = default;
    virtual void analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) = 0;
    virtual void displayResults() const = 0;
};

class TransientAnalysis : public Analyzer {
private:
    double Tstep, Tstop;
    bool use_uic;
    std::map<std::string, std::vector<double>> results;
    std::vector<double> time_points;
    std::vector<std::string> plot_vars;

public:
    TransientAnalysis(double t_step, double t_stop, bool uic_flag = false);
    void analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) override;
    void displayResults() const override;
    const std::map<std::string, std::vector<double>>& getResults() const;
    const std::vector<double>& getTimePoints() const;
private:
    void initializeResults(const Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map);
    void extractResults(const Vector& x, Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map);
};

class DCSweepAnalysis : public Analyzer {
private:
    std::string source_name;
    double start_value, end_value, increment;
    std::map<std::string, std::vector<double>> results;
    std::vector<double> sweep_values;
    std::vector<std::string> plot_vars;
public:
    DCSweepAnalysis(const std::string& src, double start, double end, double inc);
    void analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) override;
    void displayResults() const override;
    const std::map<std::string, std::vector<double>>& getResults() const { return results; }
    const std::vector<double>& getSweepValues() const { return sweep_values; }
private:
    void extractResults(const Vector& solution, const Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map);
    void initializeResults(const Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map);
};

class ACSweepAnalysis : public Analyzer {
private:
    std::string source_name;
    double start_freq_hz, end_freq_hz;
    int num_points;
    std::string sweep_type;
    std::map<std::string, std::vector<Complex>> results;
    std::vector<double> frequency_points;
public:
    ACSweepAnalysis(const std::string& src, double start_freq, double end_freq, int points, const std::string& type);
    // Corrected: This now overrides the base class method correctly. The solver param will be ignored.
    void analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) override;
    void displayResults() const override;
    const std::map<std::string, std::vector<Complex>>& getComplexResults() const;
    const std::vector<double>& getFrequencyPoints() const;
};

class PhaseSweepAnalysis : public Analyzer {
private:
    std::string source_name;
    double start_phase_deg, end_phase_deg;
    double base_freq_hz;
    int num_points;
    std::map<std::string, std::vector<Complex>> results;
    std::vector<double> phase_points;
public:
    PhaseSweepAnalysis(const std::string& src, double start_phase, double end_phase, double base_freq, int points);
    void analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) override;
    void displayResults() const override;
    const std::map<std::string, std::vector<Complex>>& getComplexResults() const;
    const std::vector<double>& getPhasePoints() const;
};
