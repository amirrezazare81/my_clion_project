#include "Analyzers.h"
#include "ErrorManager.h"
#include "Solvers.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- TransientAnalysis Implementation ---
TransientAnalysis::TransientAnalysis(double t_step, double t_stop, bool uic_flag)
    : Tstep(t_step), Tstop(t_stop), use_uic(uic_flag) {
    if (Tstep <= 0 || Tstop < 0) { throw std::runtime_error("Invalid parameters for Transient Analysis."); }
}

void TransientAnalysis::analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) {
    results.clear();
    time_points.clear();
    plot_vars.clear();

    NodeIndexMap node_map;
    std::vector<Node*> non_ground_nodes;
    circuit.getNonGroundNodes(non_ground_nodes, node_map);

    std::map<std::string, int> vs_map, l_map;
    int vs_counter = 0, l_counter = 0;
    for (const auto& elem : circuit.getElements()) {
        const std::string& type = elem->getType();
        if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "SinusoidalVoltageSource" || type == "VoltageControlledVoltageSource" || type == "CurrentControlledVoltageSource") {
            vs_map[elem->getName()] = vs_counter++;
        } else if (type == "Inductor") {
            l_map[elem->getName()] = l_counter++;
        }
    }
    initializeResults(circuit, node_map, vs_map, l_map);

    Vector x_current;

    if (use_uic) {
        x_current.assign(node_map.size() + vs_map.size() + l_map.size(), 0.0);
        std::map<std::string, double> zero_voltages;
        for(const auto& pair : node_map) zero_voltages[pair.first] = 0.0;
        if(circuit.checkGroundNodeExists()) zero_voltages[circuit.getGroundNodeId()] = 0.0;
        circuit.updatePreviousNodeVoltages(zero_voltages);
        circuit.updatePreviousInductorCurrents({});
    } else {
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
                    break;
                }
            } catch (const std::runtime_error& e) {
                 ErrorManager::displayError("DC operating point analysis failed: " + std::string(e.what()));
                 return;
            }
        }
        if (!converged) {
            ErrorManager::displayError("DC operating point did not converge.");
            return;
        }
    }

    for (double t = 0; t <= Tstop + Tstep/2.0; t += Tstep) {
        try {
            if (t > 0) {
                mna_matrix.build(circuit, true, t, Tstep);
                x_current = solver.solve(mna_matrix.getA(), mna_matrix.getRHS());
            }
            time_points.push_back(t);
            extractResults(x_current, circuit, node_map, vs_map, l_map);
        } catch (const std::runtime_error& e) {
            ErrorManager::displayError("Solver failed at t=" + std::to_string(t) + "s: " + e.what());
            break;
        }
    }
}

void TransientAnalysis::initializeResults(const Circuit& circuit, const NodeIndexMap& node_map, const std::map<std::string, int>& vs_map, const std::map<std::string, int>& l_map) {
    plot_vars.push_back("Time");
    std::vector<std::string> voltage_vars, current_vars;

    for (const auto& pair : node_map) voltage_vars.push_back("V(" + pair.first + ")");
    if (circuit.checkGroundNodeExists()) voltage_vars.push_back("V(" + circuit.getGroundNodeId() + ")");

    for (const auto& pair : vs_map) current_vars.push_back("I(" + pair.first + ")");
    for (const auto& pair : l_map) current_vars.push_back("I(" + pair.first + ")");
    for (const auto& elem : circuit.getElements()) {
        if (elem->getType() == "Resistor") current_vars.push_back("I(" + elem->getName() + ")");
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

    for(const auto& pair : node_map) results.at("V(" + pair.first + ")").push_back(x[pair.second]);
    if(circuit.checkGroundNodeExists()) results.at("V(" + circuit.getGroundNodeId() + ")").push_back(0.0);

    for (const auto& pair : vs_map) results.at("I(" + pair.first + ")").push_back(x[v_nodes + pair.second]);
    for (const auto& pair : l_map) results.at("I(" + pair.first + ")").push_back(x[v_nodes + vs_map.size() + pair.second]);

    for (const auto& elem : circuit.getElements()) {
        if (elem->getType() == "Resistor") {
            double v1 = current_voltages.at(elem->getNode1Id());
            double v2 = current_voltages.at(elem->getNode2Id());
            results.at("I(" + elem->getName() + ")").push_back((v1 - v2) / elem->getValue());
        }
    }

    circuit.updatePreviousNodeVoltages(current_voltages);
    circuit.updatePreviousInductorCurrents(current_inductor_currents);
}

void TransientAnalysis::displayResults() const {}
const std::map<std::string, std::vector<double>>& TransientAnalysis::getResults() const { return results; }
const std::vector<double>& TransientAnalysis::getTimePoints() const { return time_points; }

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
    for (const auto& elem : circuit.getElements()) {
        const std::string& type = elem->getType();
        if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "SinusoidalVoltageSource" || type == "VoltageControlledVoltageSource" || type == "CurrentControlledVoltageSource") {
            vs_map[elem->getName()] = vs_counter++;
        } else if (type == "Inductor") {
            l_map[elem->getName()] = l_counter++;
        }
    }
    
    initializeResults(circuit, node_map, vs_map, l_map);
    
    // Find the source to sweep
    Element* sweep_source = nullptr;
    for (const auto& elem : circuit.getElements()) {
        if (elem->getName() == source_name) {
            sweep_source = elem.get();
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

    for (const auto& elem : circuit.getElements()) {
        if (elem->getType() == "Resistor") {
            double v1 = x[node_map.at(elem->getNode1Id())];
            double v2 = x[node_map.at(elem->getNode2Id())];
            results.at("I(" + elem->getName() + ")").push_back((v1 - v2) / elem->getValue());
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
    for (const auto& elem : circuit.getElements()) {
        if (elem->getType() == "Resistor") current_vars.push_back("I(" + elem->getName() + ")");
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

void PhaseSweepAnalysis::analyze(Circuit& circuit, MNAMatrix& mna_matrix, const LinearSolver& solver) {}
void PhaseSweepAnalysis::displayResults() const {}
const std::map<std::string, std::vector<Complex>>& PhaseSweepAnalysis::getComplexResults() const { return results; }
const std::vector<double>& PhaseSweepAnalysis::getPhasePoints() const { return phase_points; }
