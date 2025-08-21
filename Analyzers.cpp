#include "Analyzers.h"
#include "ErrorManager.h"
#include "Solvers.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- TransientAnalysis Implementation ---
TransientAnalysis::TransientAnalysis(double t_step, double t_stop, bool uic_flag)
    : Tstep(t_step), Tstop(t_stop), use_uic(uic_flag) {
    if (Tstep <= 0 || Tstop < 0) { throw std::runtime_error("Invalid parameters for Transient Analysis."); }
}

void TransientAnalysis::analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) {
    auto analysis_start = std::chrono::high_resolution_clock::now();
    results.clear();
    time_points.clear();
    plot_vars.clear();

    std::cout << "[TransientAnalysis] Starting analysis with UIC=" << (use_uic ? "true" : "false")
              << ", Tstep=" << Tstep << ", Tstop=" << Tstop << std::endl;

    NodeIndexMap node_map;
    std::vector<Node*> non_ground_nodes;
    circuit.getNonGroundNodes(non_ground_nodes, node_map);

    std::map<std::string, int> vs_map, l_map;
    int vs_counter = 0, l_counter = 0;
    for (const auto& elem_ptr : circuit.getElements()) {
        const std::string& type = elem_ptr->getType();
        if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "WaveformVoltageSource" || type == "PhaseVoltageSource" || type == "SinusoidalVoltageSource" || type == "ACVoltageSource" || type == "VoltageControlledVoltageSource" || type == "CurrentControlledVoltageSource") {
            vs_map[elem_ptr->getName()] = vs_counter++;
        } else if (type == "Inductor") {
            l_map[elem_ptr->getName()] = l_counter++;
        }
    }
    initializeResults(circuit, node_map, vs_map, l_map);

    Vector x_current;

    if (use_uic) {
        std::cout << "UIC specified: Skipping DC operating point analysis. Starting from zero." << std::endl;
        x_current.assign(node_map.size() + vs_map.size() + l_map.size(), 0.0);
        std::map<std::string, double> zero_voltages;
        for(const auto& pair : node_map) zero_voltages[pair.first] = 0.0;
        if(circuit.checkGroundNodeExists()) zero_voltages[circuit.getGroundNodeId()] = 0.0;
        circuit.updatePreviousNodeVoltages(zero_voltages);
        circuit.updatePreviousInductorCurrents({});
    } else {
        std::cout << "Performing DC operating point analysis for initial conditions..." << std::endl;

        std::map<std::string, double> initial_guess;
        for (const auto& pair : node_map) initial_guess[pair.first] = 0.0;
        if (circuit.checkGroundNodeExists()) initial_guess[circuit.getGroundNodeId()] = 0.0;
        circuit.updatePreviousNodeVoltages(initial_guess);
        circuit.updatePreviousInductorCurrents({});

        const int max_dc_iterations = 100;
        const double dc_tolerance = 1e-6;
        bool converged = false;

        for (int i = 0; i < max_dc_iterations; ++i) {
            try {
                mna_matrix.build(circuit, false, 0.0, 0.0);
                x_current = solver.solve(mna_matrix.getA(), mna_matrix.getRHS());

                double max_delta = 0.0;
                std::map<std::string, double> new_voltages;
                for (const auto& pair : node_map) {
                    double old_v = circuit.previous_node_voltages.at(pair.first);
                    double new_v = x_current[pair.second];
                    max_delta = std::max(max_delta, std::abs(new_v - old_v));
                    new_voltages[pair.first] = new_v;
                }

                circuit.updatePreviousNodeVoltages(new_voltages);

                if (max_delta < dc_tolerance) {
                    converged = true;
                    std::cout << "DC operating point converged after " << i + 1 << " iterations." << std::endl;
                    break;
                }
            } catch (const std::runtime_error& e) {
                 ErrorManager::displayError("DC operating point analysis failed during iteration " + std::to_string(i+1) + ": " + std::string(e.what()));
                 return;
            }
        }

        if (!converged) {
            ErrorManager::displayError("DC operating point did not converge after " + std::to_string(max_dc_iterations) + " iterations.");
            return;
        }
    }

    // Now that the initial state (either from DC OP or UIC) is set, start the transient simulation
    int step_count = 0;
    for (double t = 0; t <= Tstop + Tstep/2.0; t += Tstep) {
        try {
            // Build MNA matrix and solve for ALL time steps, including t=0
            mna_matrix.build(circuit, true, t, Tstep);
            x_current = solver.solve(mna_matrix.getA(), mna_matrix.getRHS());

            time_points.push_back(t);
            extractResults(x_current, circuit, node_map, vs_map, l_map);

            step_count++;
            if (step_count <= 5 || step_count % 1000 == 0) {
                std::cout << "[TransientAnalysis] Step " << step_count << " at t=" << std::fixed << std::setprecision(6) << t << "s" << std::endl;
                if (!x_current.empty()) {
                    std::cout << "[TransientAnalysis]   Solution size: " << x_current.size() << std::endl;
                    for (size_t i = 0; i < std::min(size_t(3), x_current.size()); ++i) {
                        std::cout << "[TransientAnalysis]   x[" << i << "] = " << std::fixed << std::setprecision(6) << x_current[i] << std::endl;
                    }
                }
            }
        } catch (const std::runtime_error& e) {
            ErrorManager::displayError("Solver failed at t=" + std::to_string(t) + "s: " + e.what());
            break;
        }
    }
    std::cout << "Transient analysis completed." << std::endl;


}

void TransientAnalysis::initializeResults(const Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map) {
    plot_vars.push_back("Time");
    std::vector<std::string> voltage_vars, current_vars;

    for (const auto& pair : node_map) voltage_vars.push_back("V(" + pair.first + ")");
    if (circuit.checkGroundNodeExists()) voltage_vars.push_back("V(" + circuit.getGroundNodeId() + ")");

    for (const auto& pair : vs_map) current_vars.push_back("I(" + pair.first + ")");
    for (const auto& pair : l_map) current_vars.push_back("I(" + pair.first + ")");
    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getType() == "Resistor") current_vars.push_back("I(" + elem_ptr->getName() + ")");
    }

    sort(voltage_vars.begin(), voltage_vars.end());
    sort(current_vars.begin(), current_vars.end());

    for(const auto& v : voltage_vars) { plot_vars.push_back(v); results[v]; }
    for(const auto& i : current_vars) { plot_vars.push_back(i); results[i]; }
}

void TransientAnalysis::extractResults(const Vector& x, Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map) {
    int v_nodes = circuit.getNumNonGroundNodes();

    std::map<std::string, double> current_voltages;
    for (const auto& pair : node_map) current_voltages[pair.first] = x[pair.second];
    if (circuit.checkGroundNodeExists()) current_voltages[circuit.getGroundNodeId()] = 0.0;

    std::map<std::string, double> current_inductor_currents;
    for (const auto& pair : l_map) current_inductor_currents[pair.first] = x[v_nodes + vs_map.size() + pair.second];

    // First, populate the results table for plotting with the current solution
    for(const auto& pair : node_map) results.at("V(" + pair.first + ")").push_back(x[pair.second]);
    if(circuit.checkGroundNodeExists()) results.at("V(" + circuit.getGroundNodeId() + ")").push_back(0.0);

    for (const auto& pair : vs_map) results.at("I(" + pair.first + ")").push_back(x[v_nodes + pair.second]);
    for (const auto& pair : l_map) results.at("I(" + pair.first + ")").push_back(x[v_nodes + vs_map.size() + pair.second]);

    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getType() == "Resistor") {
            double v1 = current_voltages.at(elem_ptr->getNode1Id());
            double v2 = current_voltages.at(elem_ptr->getNode2Id());
            results.at("I(" + elem_ptr->getName() + ")").push_back((v1 - v2) / elem_ptr->getValue());
        }
    }

    // Finally, update the circuit's internal state for the *next* iteration
    circuit.updatePreviousNodeVoltages(current_voltages);
    circuit.updatePreviousInductorCurrents(current_inductor_currents);
}

void TransientAnalysis::displayResults() const {
    if (time_points.empty()) { std::cout << "No transient results to display." << std::endl; return; }

    const int COL_WIDTH = 14;

    for (const auto& var : plot_vars) {
        std::cout << std::left << std::setw(COL_WIDTH) << var;
    }
    std::cout << std::endl;
    std::cout << std::string(COL_WIDTH * plot_vars.size(), '-') << std::endl;

    for (size_t i = 0; i < time_points.size(); ++i) {
        std::cout << std::fixed << std::setprecision(4) << std::left << std::setw(COL_WIDTH) << time_points[i];
        for (size_t j = 1; j < plot_vars.size(); ++j) {
            const std::string& var = plot_vars[j];
            if (results.count(var) && i < results.at(var).size()) {
                std::cout << std::left << std::setw(COL_WIDTH) << results.at(var)[i];
            } else {
                std::cout << std::left << std::setw(COL_WIDTH) << "N/A";
            }
        }
        std::cout << std::endl;
    }
}
const std::map<std::string, std::vector<double>>& TransientAnalysis::getResults() const {
    std::cout << "[TransientAnalysis] getResults() called, returning " << results.size() << " signal(s)" << std::endl;
    for (const auto& pair : results) {
        std::cout << "[TransientAnalysis]   " << pair.first << ": " << pair.second.size() << " points" << std::endl;
        if (!pair.second.empty()) {
            std::cout << "[TransientAnalysis]     First value: " << pair.second[0] << std::endl;
        }
    }
    return results;
}
const std::vector<double>& TransientAnalysis::getTimePoints() const {
    std::cout << "[TransientAnalysis] getTimePoints() called, returning " << time_points.size() << " points" << std::endl;
    if (!time_points.empty()) {
        std::cout << "[TransientAnalysis]   Time range: " << time_points[0] << " to " << time_points.back() << "s" << std::endl;
    }
    return time_points;
}

// --- DCSweepAnalysis Implementation ---
DCSweepAnalysis::DCSweepAnalysis(const std::string& src, double start, double end, double inc)
    : source_name(src), start_value(start), end_value(end), increment(inc) {
    if (inc == 0 || (start_value < end_value && inc < 0) || (start_value > end_value && inc > 0)) {
        throw std::runtime_error("Invalid increment for DC Sweep.");
    }
}

void DCSweepAnalysis::analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) {
    results.clear();
    sweep_values.clear();
    
    NodeIndexMap node_map;
    std::vector<Node*> non_ground_nodes;
    circuit.getNonGroundNodes(non_ground_nodes, node_map);
    
    std::map<std::string, int> vs_map, l_map;
    int vs_counter = 0, l_counter = 0;
    for (const auto& elem_ptr : circuit.getElements()) {
        const std::string& type = elem_ptr->getType();
        if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "WaveformVoltageSource" || type == "PhaseVoltageSource" || type == "SinusoidalVoltageSource" || type == "ACVoltageSource" || type == "VoltageControlledVoltageSource" || type == "CurrentControlledVoltageSource") {
            vs_map[elem_ptr->getName()] = vs_counter++;
        } else if (type == "Inductor") {
            l_map[elem_ptr->getName()] = l_counter++;
        }
    }
    
    initializeResults(circuit, node_map, vs_map, l_map);
    
    // Find the source to sweep
    Element* sweep_source = nullptr;
    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getName() == source_name) {
            sweep_source = elem_ptr.get();
            break;
        }
    }
    
    if (!sweep_source) {
        ErrorManager::displayError("Source " + source_name + " not found for DC sweep.");
        return;
    }
    
    // Perform DC sweep
    for (double value = start_value; 
         (start_value < end_value && value <= end_value) || (start_value > end_value && value >= end_value); 
         value += increment) {
        
        try {
            // Set the source value
            if (sweep_source->getType() == "IndependentVoltageSource") {
                static_cast<IndependentVoltageSource*>(sweep_source)->setValue(value);
            } else if (sweep_source->getType() == "IndependentCurrentSource") {
                static_cast<IndependentCurrentSource*>(sweep_source)->setValue(value);
            }
            
            // Build and solve the MNA matrix
            mna_matrix.build(circuit, false, 0.0, 0.0);
            Vector solution = solver.solve(mna_matrix.getA(), mna_matrix.getRHS());
            
            sweep_values.push_back(value);
            extractResults(solution, circuit, node_map, vs_map, l_map);
            
        } catch (const std::runtime_error& e) {
            ErrorManager::displayError("DC sweep failed at value " + std::to_string(value) + ": " + e.what());
            break;
        }
    }
}

void DCSweepAnalysis::displayResults() const {
    std::cout << "\n=== DC Sweep Analysis Results ===\n";
    std::cout << "Source: " << source_name << "\n";
    std::cout << "Sweep range: " << start_value << " to " << end_value << " (increment: " << increment << ")\n\n";
    
    if (results.empty() || sweep_values.empty()) {
        std::cout << "No results to display.\n";
        return;
    }
    
    // Display header
    std::cout << std::setw(12) << "Value";
    for (const auto& var : plot_vars) {
        std::cout << std::setw(15) << var;
    }
    std::cout << "\n";
    
    // Display results
    for (size_t i = 0; i < sweep_values.size(); ++i) {
        std::cout << std::setw(12) << std::fixed << std::setprecision(6) << sweep_values[i];
        for (const auto& var : plot_vars) {
            if (results.count(var) && i < results.at(var).size()) {
                std::cout << std::setw(15) << std::fixed << std::setprecision(6) << results.at(var)[i];
            } else {
                std::cout << std::setw(15) << "N/A";
            }
        }
        std::cout << "\n";
    }
}

void DCSweepAnalysis::extractResults(const Vector& x, const Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map) {
    int v_nodes = circuit.getNumNonGroundNodes();

    for(const auto& pair : node_map) {
        results.at("V(" + pair.first + ")").push_back(x[pair.second]);
    }
    if(circuit.checkGroundNodeExists()) {
        results.at("V(" + circuit.getGroundNodeId() + ")").push_back(0.0);
    }

    for (const auto& pair : vs_map) {
        results.at("I(" + pair.first + ")").push_back(x[v_nodes + pair.second]);
    }
    for (const auto& pair : l_map) {
        results.at("I(" + pair.first + ")").push_back(x[v_nodes + vs_map.size() + pair.second]);
    }

    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getType() == "Resistor") {
            double v1 = x[node_map.at(elem_ptr->getNode1Id())];
            double v2 = x[node_map.at(elem_ptr->getNode2Id())];
            results.at("I(" + elem_ptr->getName() + ")").push_back((v1 - v2) / elem_ptr->getValue());
        }
    }
}

void DCSweepAnalysis::initializeResults(const Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map) {
    plot_vars.clear();
    std::vector<std::string> voltage_vars, current_vars;

    for (const auto& pair : node_map) voltage_vars.push_back("V(" + pair.first + ")");
    if (circuit.checkGroundNodeExists()) voltage_vars.push_back("V(" + circuit.getGroundNodeId() + ")");

    for (const auto& pair : vs_map) current_vars.push_back("I(" + pair.first + ")");
    for (const auto& pair : l_map) current_vars.push_back("I(" + pair.first + ")");
    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getType() == "Resistor") current_vars.push_back("I(" + elem_ptr->getName() + ")");
    }

    sort(voltage_vars.begin(), voltage_vars.end());
    sort(current_vars.begin(), current_vars.end());

    for(const auto& v : voltage_vars) { plot_vars.push_back(v); results[v]; }
    for(const auto& i : current_vars) { plot_vars.push_back(i); results[i]; }
}

// --- ACSweepAnalysis Implementation ---
ACSweepAnalysis::ACSweepAnalysis(const std::string& src, double start_freq, double end_freq, int points, const std::string& type)
    : source_name(src), start_freq_hz(start_freq), end_freq_hz(end_freq), num_points(points), sweep_type(type) {}

void ACSweepAnalysis::analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) {
    results.clear();
    frequency_points.clear();
    ComplexMNAMatrix complex_mna;
    ComplexLinearSolver complex_solver;

    double step;
    if (sweep_type == "DEC") {
        step = pow(end_freq_hz / start_freq_hz, 1.0 / (num_points - 1));
    } else { // LIN
        step = (end_freq_hz - start_freq_hz) / (num_points - 1);
    }

    for (int i = 0; i < num_points; ++i) {
        double freq;
        if (sweep_type == "DEC") {
            freq = start_freq_hz * pow(step, i);
        } else { // LIN
            freq = start_freq_hz + i * step;
        }
        double omega = 2 * M_PI * freq;
        frequency_points.push_back(freq);

        try {
            NodeIndexMap node_map;
            std::map<std::string, int> ac_source_map;
            complex_mna.build(circuit, omega, node_map, ac_source_map);

            if (ac_source_map.count(source_name)) {
                int source_idx = node_map.size() + ac_source_map.at(source_name);
                complex_mna.getRHS()[source_idx] = 1.0;
            }

            ComplexVector solution = complex_solver.solve(complex_mna.getA(), complex_mna.getRHS());

            for (const auto& pair : node_map) {
                results["V(" + pair.first + ")"].push_back(solution[pair.second]);
            }

        } catch (const std::runtime_error& e) {
            ErrorManager::displayError("AC analysis failed at " + std::to_string(freq) + " Hz: " + e.what());
            return;
        }
    }
}
void ACSweepAnalysis::displayResults() const {}
const std::map<std::string, std::vector<Complex>>& ACSweepAnalysis::getComplexResults() const { return results; }
const std::vector<double>& ACSweepAnalysis::getFrequencyPoints() const { return frequency_points; }

// --- PhaseSweepAnalysis Implementation ---
PhaseSweepAnalysis::PhaseSweepAnalysis(const std::string& src, double start_phase, double end_phase, double base_freq, int points)
    : source_name(src), start_phase_deg(start_phase), end_phase_deg(end_phase), base_freq_hz(base_freq), num_points(points) {}

void PhaseSweepAnalysis::analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) {
    results.clear();
    phase_points.clear();
    ComplexMNAMatrix complex_mna;
    ComplexLinearSolver complex_solver;

    double omega = 2 * M_PI * base_freq_hz;
    double phase_step = (end_phase_deg - start_phase_deg) / (num_points - 1);

    for (int i = 0; i < num_points; ++i) {
        double phase_deg = start_phase_deg + i * phase_step;
        double phase_rad = phase_deg * M_PI / 180.0;
        phase_points.push_back(phase_deg);

        try {
            NodeIndexMap node_map;
            std::map<std::string, int> ac_source_map;
            complex_mna.build(circuit, omega, node_map, ac_source_map);

            // Set the source with the current phase
            if (ac_source_map.count(source_name)) {
                int source_idx = node_map.size() + ac_source_map.at(source_name);
                complex_mna.getRHS()[source_idx] = std::polar(1.0, phase_rad); // 1V with current phase
            }

            ComplexVector solution = complex_solver.solve(complex_mna.getA(), complex_mna.getRHS());

            for (const auto& pair : node_map) {
                results["V(" + pair.first + ")"].push_back(solution[pair.second]);
            }

        } catch (const std::runtime_error& e) {
            ErrorManager::displayError("Phase analysis failed at " + std::to_string(phase_deg) + "°: " + e.what());
            return;
        }
    }
}

void PhaseSweepAnalysis::displayResults() const {
    std::cout << "[Phase Sweep] Analysis complete: " << phase_points.size() << " phase points" << std::endl;
    for (const auto& pair : results) {
        std::cout << "[Phase Sweep] " << pair.first << ": " << pair.second.size() << " complex values" << std::endl;
    }
}

const std::map<std::string, std::vector<Complex>>& PhaseSweepAnalysis::getComplexResults() const { return results; }
const std::vector<double>& PhaseSweepAnalysis::getPhasePoints() const { return phase_points; }
