#include "Solvers.h"
#include "ErrorManager.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <chrono>

// --- MNAMatrix Implementation ---
MNAMatrix::MNAMatrix() = default;

void MNAMatrix::reset() {
    A_matrix.clear();
    b_vector.clear();
}

void MNAMatrix::build(const Circuit& circuit, bool is_transient, double currentTime, double timeStepIncrement) {
    auto build_start = std::chrono::high_resolution_clock::now();
    reset();
    ErrorManager::info(std::string("[MNA] build start, transient=") + (is_transient?"true":"false") + 
                       ", t=" + std::to_string(currentTime) + ", dt=" + std::to_string(timeStepIncrement));
    NodeIndexMap node_map;
    std::vector<Node*> non_ground_nodes_vec;
    circuit.getNonGroundNodes(non_ground_nodes_vec, node_map);
    int num_voltage_nodes = circuit.getNumNonGroundNodes();

    std::map<std::string, int> voltage_source_current_map;
    std::map<std::string, int> inductor_current_map;
    std::map<std::string, int> ccvs_current_map;

    int vs_idx_counter = 0;
    int l_idx_counter = 0;
    int ccvs_idx_counter = 0;

    // FIX: Correctly iterate over a vector of unique_ptrs
    for (const auto& elem : circuit.getElements()) {
        const std::string& type = elem->getType();
        if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "WaveformVoltageSource" || type == "PhaseVoltageSource" || type == "SinusoidalVoltageSource" || type == "ACVoltageSource" || type == "VoltageControlledVoltageSource") {
            voltage_source_current_map[elem->getName()] = vs_idx_counter++;
        } else if (type == "Inductor") {
            inductor_current_map[elem->getName()] = l_idx_counter++;
        } else if (type == "CurrentControlledVoltageSource") {
            ccvs_current_map[elem->getName()] = ccvs_idx_counter++;
        }
    }

    int num_voltage_sources = vs_idx_counter;
    int num_inductors = l_idx_counter;
    int num_ccvs = ccvs_idx_counter;
    int total_unknowns = num_voltage_nodes + num_voltage_sources + num_inductors + num_ccvs;

    A_matrix.assign(total_unknowns, Vector(total_unknowns, 0.0));
    b_vector.assign(total_unknowns, 0.0);
    {
        std::stringstream ss; ss << "[MNA] sizes: nodes=" << num_voltage_nodes << ", vs=" << num_voltage_sources
                                  << ", L=" << num_inductors << ", CCVS=" << num_ccvs << ", unknowns=" << total_unknowns;
        ErrorManager::info(ss.str());
    }

    const std::map<std::string, double>& prev_node_voltages = circuit.previous_node_voltages;
    const std::map<std::string, double>& prev_inductor_currents = circuit.getPreviousInductorCurrents();

    const double GMIN = 1e-12;
    for (int i = 0; i < num_voltage_nodes; ++i) {
        A_matrix[i][i] += GMIN;
    }

    // FIX: Correctly iterate over a vector of unique_ptrs
    for (const auto& elem : circuit.getElements()) {
        const std::string& type = elem->getType();

        if (type == "Resistor" || type == "Capacitor" || type == "IndependentCurrentSource" || type == "Diode") {
            elem->contributeToMNA(A_matrix, b_vector, num_voltage_nodes, node_map, prev_node_voltages, is_transient, timeStepIncrement);

        } else if (type == "PulseCurrentSource") {
            // Pulse current sources need current time for transient analysis
            static_cast<PulseCurrentSource*>(elem.get())->contributeToMNA(A_matrix, b_vector, num_voltage_nodes, node_map, prev_node_voltages, is_transient, currentTime);
        }
        else if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "WaveformVoltageSource" || type == "PhaseVoltageSource" || type == "SinusoidalVoltageSource" || type == "ACVoltageSource" || type == "VoltageControlledVoltageSource") {
            int n1_idx = node_map.count(elem->getNode1Id()) ? node_map.at(elem->getNode1Id()) : -1;
            int n2_idx = node_map.count(elem->getNode2Id()) ? node_map.at(elem->getNode2Id()) : -1;
            
            if (!voltage_source_current_map.count(elem->getName())) {
                ErrorManager::warn("[MNA] Voltage source " + elem->getName() + " not found in current map");
                continue;
            }
            int vs_curr_idx = num_voltage_nodes + voltage_source_current_map.at(elem->getName());

            if (n1_idx != -1) { A_matrix[n1_idx][vs_curr_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[n2_idx][vs_curr_idx] -= 1.0; }
            if (n1_idx != -1) { A_matrix[vs_curr_idx][n1_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[vs_curr_idx][n2_idx] -= 1.0; }

            if (type == "PulseVoltageSource") {
                b_vector[vs_curr_idx] = static_cast<const PulseVoltageSource*>(elem.get())->getVoltageAtTime(currentTime);
            } else if (type == "WaveformVoltageSource") {
                b_vector[vs_curr_idx] = static_cast<const WaveformVoltageSource*>(elem.get())->getVoltageAtTime(currentTime);
            } else if (type == "PhaseVoltageSource") {
                b_vector[vs_curr_idx] = static_cast<const PhaseVoltageSource*>(elem.get())->getVoltageAtTime(currentTime);
            } else if (type == "SinusoidalVoltageSource") {
                 b_vector[vs_curr_idx] = static_cast<const SinusoidalVoltageSource*>(elem.get())->getVoltageAtTime(currentTime);
            } else if (type == "ACVoltageSource") {
                auto* ac_vs = static_cast<const ACVoltageSource*>(elem.get());
                // For transient analysis, AC voltage sources can be treated as sinusoidal
                // V(t) = magnitude * cos(2*pi*frequency*t + phase)
                double freq = ac_vs->getFrequency();
                double mag = ac_vs->getMagnitude();
                double phase_rad = ac_vs->getPhase() * M_PI / 180.0;
                b_vector[vs_curr_idx] = mag * cos(2.0 * M_PI * freq * currentTime + phase_rad);
            } else if (type == "VoltageControlledVoltageSource") {
                auto* vcvs = static_cast<const VoltageControlledVoltageSource*>(elem.get());
                int ctrl_n1_idx = node_map.count(vcvs->getControlNode1Id()) ? node_map.at(vcvs->getControlNode1Id()) : -1;
                int ctrl_n2_idx = node_map.count(vcvs->getControlNode2Id()) ? node_map.at(vcvs->getControlNode2Id()) : -1;

                if (ctrl_n1_idx != -1) A_matrix[vs_curr_idx][ctrl_n1_idx] -= vcvs->getGain();
                if (ctrl_n2_idx != -1) A_matrix[vs_curr_idx][ctrl_n2_idx] += vcvs->getGain();

                b_vector[vs_curr_idx] = 0; // RHS is zero
            }
            else { // IndependentVoltageSource
                // For transient: treat as DC source, but ensure variable exists
                b_vector[vs_curr_idx] = elem->getValue();
            }
        }
        else if (type == "Inductor") {
            int n1_idx = node_map.count(elem->getNode1Id()) ? node_map.at(elem->getNode1Id()) : -1;
            int n2_idx = node_map.count(elem->getNode2Id()) ? node_map.at(elem->getNode2Id()) : -1;
            
            if (!inductor_current_map.count(elem->getName())) {
                ErrorManager::warn("[MNA] Inductor " + elem->getName() + " not found in current map");
                continue;
            }
            int L_curr_idx = num_voltage_nodes + num_voltage_sources + inductor_current_map.at(elem->getName());

            if (n1_idx != -1) { A_matrix[n1_idx][L_curr_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[n2_idx][L_curr_idx] -= 1.0; }

            if (n1_idx != -1) { A_matrix[L_curr_idx][n1_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[L_curr_idx][n2_idx] -= 1.0; }

            if (is_transient) {
                if (timeStepIncrement <= 0) throw std::runtime_error("Invalid timestep for inductor model.");
                double L_val = elem->getValue();
                double prev_I = prev_inductor_currents.count(elem->getName()) ? prev_inductor_currents.at(elem->getName()) : 0.0;
                A_matrix[L_curr_idx][L_curr_idx] -= L_val / timeStepIncrement;
                b_vector[L_curr_idx] = -(L_val / timeStepIncrement) * prev_I;
            } else { // DC analysis, inductor is a short (0V across it)
                b_vector[L_curr_idx] = 0.0;
            }
        }
        else if (type == "CurrentControlledCurrentSource") {
            auto* cccs = static_cast<const CurrentControlledCurrentSource*>(elem.get());
            int n1_idx = node_map.count(elem->getNode1Id()) ? node_map.at(elem->getNode1Id()) : -1;
            int n2_idx = node_map.count(elem->getNode2Id()) ? node_map.at(elem->getNode2Id()) : -1;
            
            // Find the controlling branch (voltage source or inductor)
            std::string ctrl_branch = cccs->getControllingBranchName();
            int ctrl_idx = -1;
            
            if (voltage_source_current_map.count(ctrl_branch)) {
                ctrl_idx = num_voltage_nodes + voltage_source_current_map.at(ctrl_branch);
            } else if (inductor_current_map.count(ctrl_branch)) {
                ctrl_idx = num_voltage_nodes + num_voltage_sources + inductor_current_map.at(ctrl_branch);
            }
            
            if (ctrl_idx != -1) {
                if (n1_idx != -1) A_matrix[n1_idx][ctrl_idx] += cccs->getGain();
                if (n2_idx != -1) A_matrix[n2_idx][ctrl_idx] -= cccs->getGain();
            }
        }
        else if (type == "CurrentControlledVoltageSource") {
            auto* ccvs = static_cast<const CurrentControlledVoltageSource*>(elem.get());
            int n1_idx = node_map.count(elem->getNode1Id()) ? node_map.at(elem->getNode1Id()) : -1;
            int n2_idx = node_map.count(elem->getNode2Id()) ? node_map.at(elem->getNode2Id()) : -1;
            
            // Add a new current variable for this CCVS
            int ccvs_curr_idx = num_voltage_nodes + num_voltage_sources + num_inductors + (ccvs_current_map.count(elem->getName()) ? ccvs_current_map.at(elem->getName()) : 0);
            
            // Find the controlling branch
            std::string ctrl_branch = ccvs->getControllingBranchName();
            int ctrl_idx = -1;
            
            if (voltage_source_current_map.count(ctrl_branch)) {
                ctrl_idx = num_voltage_nodes + voltage_source_current_map.at(ctrl_branch);
            } else if (inductor_current_map.count(ctrl_branch)) {
                ctrl_idx = num_voltage_nodes + num_voltage_sources + inductor_current_map.at(ctrl_branch);
            }
            
            if (ctrl_idx != -1) {
                // Voltage constraint: V1 - V2 = R * I_control
                if (n1_idx != -1) A_matrix[n1_idx][ccvs_curr_idx] += 1.0;
                if (n2_idx != -1) A_matrix[n2_idx][ccvs_curr_idx] -= 1.0;
                if (n1_idx != -1) A_matrix[ccvs_curr_idx][n1_idx] += 1.0;
                if (n2_idx != -1) A_matrix[ccvs_curr_idx][n2_idx] -= 1.0;
                
                // Current constraint: I_ccvs = R * I_control
                A_matrix[ccvs_curr_idx][ctrl_idx] -= ccvs->getTransresistance();
                b_vector[ccvs_curr_idx] = 0.0;
            }
        }
    }
    // Log small summary of RHS norm and build time
    double rhs_norm = 0.0; for (double v : b_vector) rhs_norm += v*v; 
    auto build_end = std::chrono::high_resolution_clock::now();
    auto build_duration = std::chrono::duration_cast<std::chrono::microseconds>(build_end - build_start);
    {
        std::stringstream ss; ss << "[MNA] build end, rhs_norm2=" << rhs_norm << ", build_time=" << build_duration.count() << "μs";
        ErrorManager::info(ss.str());
    }
}
const Matrix& MNAMatrix::getA() const { return A_matrix; }
const Vector& MNAMatrix::getRHS() const { return b_vector; }


// --- ComplexMNAMatrix Implementation ---
void ComplexMNAMatrix::build(const Circuit& circuit, double omega, NodeIndexMap& node_map, std::map<std::string, int>& ac_source_map) {
    std::vector<Node*> non_ground_nodes;
    circuit.getNonGroundNodes(non_ground_nodes, node_map); // <-- FIX: Corrected typo
    int num_nodes = node_map.size();
    ac_source_map.clear();
    int ac_source_count = 0;
    for (const auto& elem : circuit.getElements()) {
        if (elem->getType() == "IndependentVoltageSource" || 
            elem->getType() == "SinusoidalVoltageSource" ||
            elem->getType() == "ACVoltageSource") {
            ac_source_map[elem->getName()] = ac_source_count++;
        }
    }
    int total_unknowns = num_nodes + ac_source_count;
    A_matrix.assign(total_unknowns, ComplexVector(total_unknowns, {0.0, 0.0}));
    b_vector.assign(total_unknowns, {0.0, 0.0});
    const Complex j(0.0, 1.0);
    for (const auto& elem : circuit.getElements()) {
        int n1_idx = node_map.count(elem->getNode1Id()) ? node_map.at(elem->getNode1Id()) : -1;
        int n2_idx = node_map.count(elem->getNode2Id()) ? node_map.at(elem->getNode2Id()) : -1;
        Complex admittance = {0.0, 0.0};
        std::string type = elem->getType();
        
        if (type == "Resistor") {
            admittance = 1.0 / elem->getValue();
        } else if (type == "Capacitor") {
            admittance = j * omega * elem->getValue();
        } else if (type == "Inductor") {
            admittance = (omega > 1e-9) ? 1.0 / (j * omega * elem->getValue()) : 1e12;

        } else if (ac_source_map.count(elem->getName())) {
            // Handle voltage sources (including AC voltage sources)
            int vs_idx = num_nodes + ac_source_map.at(elem->getName());
            if (n1_idx != -1) { A_matrix[n1_idx][vs_idx] += 1.0; A_matrix[vs_idx][n1_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[n2_idx][vs_idx] -= 1.0; A_matrix[vs_idx][n2_idx] -= 1.0; }
            
            // Set the voltage for AC voltage sources
            if (type == "ACVoltageSource") {
                auto* ac_voltage = static_cast<const ACVoltageSource*>(elem.get());
                b_vector[vs_idx] = ac_voltage->getComplexVoltage();
            }
        }
        
        // Add admittance contributions for passive elements
        if (std::abs(admittance) > 0) {
            if (n1_idx != -1) A_matrix[n1_idx][n1_idx] += admittance;
            if (n2_idx != -1) A_matrix[n2_idx][n2_idx] += admittance;
            if (n1_idx != -1 && n2_idx != -1) {
                A_matrix[n1_idx][n2_idx] -= admittance;
                A_matrix[n2_idx][n1_idx] -= admittance;
            }
        }
    }
}
const ComplexMatrix& ComplexMNAMatrix::getA() const { return A_matrix; }
ComplexVector& ComplexMNAMatrix::getRHS() { return b_vector; }

// ... (Rest of the file is unchanged)

// --- GaussianEliminationSolver Implementation ---
Vector GaussianEliminationSolver::solve(const Matrix& A, const Vector& b) const {
    int n = A.size();
    if (n == 0) return {};
    // Log residual norm of input RHS
    {
        double rhs2 = 0.0; for (double v : b) rhs2 += v*v;
        std::stringstream ss; ss << "[Solver-GE] n=" << n << ", rhs_norm2=" << rhs2;
        ErrorManager::info(ss.str());
    }
    Matrix A_copy = A;
    Vector b_copy = b;
    for (int k = 0; k < n; ++k) {
        int pivot_row = k;
        for (int i = k + 1; i < n; ++i) {
            if (std::abs(A_copy[i][k]) > std::abs(A_copy[pivot_row][k])) pivot_row = i;
        }
        std::swap(A_copy[k], A_copy[pivot_row]);
        std::swap(b_copy[k], b_copy[pivot_row]);
        if (std::abs(A_copy[k][k]) < 1e-12) throw std::runtime_error("Matrix is singular.");
        for (int i = k + 1; i < n; ++i) {
            double factor = A_copy[i][k] / A_copy[k][k];
            for (int j = k; j < n; ++j) A_copy[i][j] -= factor * A_copy[k][j];
            b_copy[i] -= factor * b_copy[k];
        }
    }
    Vector x(n);
    for (int i = n - 1; i >= 0; --i) {
        double sum = 0.0;
        for (int j = i + 1; j < n; ++j) sum += A_copy[i][j] * x[j];
        x[i] = (b_copy[i] - sum) / A_copy[i][i];
    }
    return x;
}

// --- LUDecompositionSolver Implementation ---
Vector LUDecompositionSolver::solve(const Matrix& A, const Vector& b) const {
    int n = A.size();
    if (n == 0) return {};
    {
        double rhs2 = 0.0; for (double v : b) rhs2 += v*v;
        std::stringstream ss; ss << "[Solver-LU] n=" << n << ", rhs_norm2=" << rhs2;
        ErrorManager::info(ss.str());
    }
    Matrix L(n, Vector(n, 0.0)), U(n, Vector(n, 0.0));
    for (int i = 0; i < n; i++) {
        L[i][i] = 1.0;
        for (int k = i; k < n; k++) {
            double sum = 0.0;
            for (int j = 0; j < i; j++) sum += (L[i][j] * U[j][k]);
            U[i][k] = A[i][k] - sum;
        }
        for (int k = i + 1; k < n; k++) {
            if (std::abs(U[i][i]) < 1e-12) throw std::runtime_error("Singular matrix in LU.");
            double sum = 0.0;
            for (int j = 0; j < i; j++) sum += (L[k][j] * U[j][i]);
            L[k][i] = (A[k][i] - sum) / U[i][i];
        }
    }
    Vector y(n);
    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        for (int j = 0; j < i; j++) sum += L[i][j] * y[j];
        y[i] = (b[i] - sum) / L[i][i];
    }
    Vector x(n);
    for (int i = n - 1; i >= 0; i--) {
        double sum = 0.0;
        for (int j = i + 1; j < n; j++) sum += U[i][j] * x[j];
        if (std::abs(U[i][i]) < 1e-12) throw std::runtime_error("Singular matrix in LU back-sub.");
        x[i] = (y[i] - sum) / U[i][i];
    }
    return x;
}

// --- ComplexLinearSolver Implementation ---
ComplexVector ComplexLinearSolver::solve(ComplexMatrix A, ComplexVector b) const {
    int n = A.size();
    if (n == 0) return {};
    for (int k = 0; k < n; ++k) {
        int pivot_row = k;
        for (int i = k + 1; i < n; ++i) {
            if (std::abs(A[i][k]) > std::abs(A[pivot_row][k])) pivot_row = i;
        }
        std::swap(A[k], A[pivot_row]);
        std::swap(b[k], b[pivot_row]);
        if (std::abs(A[k][k]) < 1e-12) throw std::runtime_error("Complex matrix is singular.");
        for (int i = k + 1; i < n; ++i) {
            Complex factor = A[i][k] / A[k][k];
            for (int j = k; j < n; ++j) A[i][j] -= factor * A[k][j];
            b[i] -= factor * b[k];
        }
    }
    ComplexVector x(n);
    for (int i = n - 1; i >= 0; --i) {
        Complex sum = {0.0, 0.0};
        for (int j = i + 1; j < n; ++j) sum += A[i][j] * x[j];
        x[i] = (b[i] - sum) / A[i][i];
    }
    return x;
}

// --- MNASolver Implementation ---
MNASolver::MNASolver(const MNASolverConfig& config) : config(config) {
    if (config.use_lu_decomposition) {
        linear_solver = std::make_unique<LUDecompositionSolver>();
    } else {
        linear_solver = std::make_unique<GaussianEliminationSolver>();
    }
    complex_solver = std::make_unique<ComplexLinearSolver>();
}

AnalysisResult MNASolver::solve(const Circuit& circuit, AnalysisType analysis_type) {
    try {
        // Validate circuit first
        if (!validateCircuit(circuit)) {
            AnalysisResult result;
            result.success = false;
            result.error_message = "Circuit validation failed";
            return result;
        }
        
        switch (analysis_type) {
            case AnalysisType::DC_ANALYSIS:
                return solveDC(circuit);
            case AnalysisType::TRANSIENT_ANALYSIS:
                return solveTransient(circuit);
            case AnalysisType::AC_ANALYSIS:
                return solveAC(circuit);
            case AnalysisType::DC_SWEEP:
                // For DC sweep, we need parameters - this will be handled by the specific method
                return solveDC(circuit);
            default:
                AnalysisResult result;
                result.success = false;
                result.error_message = "Unknown analysis type";
                return result;
        }
    } catch (const std::exception& e) {
        AnalysisResult result;
        result.success = false;
        result.error_message = std::string("Exception during analysis: ") + e.what();
        return result;
    }
}

AnalysisResult MNASolver::solveDC(const Circuit& circuit) {
    AnalysisResult result;
    
    try {
        // Build MNA matrix for DC analysis
        MNAMatrix mna_matrix;
        mna_matrix.build(circuit, false, 0.0, 0.0);
        
        // Solve the system
        Vector solution = linear_solver->solve(mna_matrix.getA(), mna_matrix.getRHS());
        
        // Extract results
        NodeIndexMap node_map;
        std::vector<Node*> non_ground_nodes;
        circuit.getNonGroundNodes(non_ground_nodes, node_map);
        
        // Create maps for voltage sources and inductors
        std::map<std::string, int> vs_map, l_map;
        int vs_idx = 0, l_idx = 0;
        
        for (const auto& elem : circuit.getElements()) {
            if (elem->getType() == "IndependentVoltageSource" || 
                elem->getType() == "PulseVoltageSource" || 
                elem->getType() == "WaveformVoltageSource" ||
                elem->getType() == "PhaseVoltageSource" ||
                elem->getType() == "SinusoidalVoltageSource" ||
                elem->getType() == "ACVoltageSource" ||
                elem->getType() == "VoltageControlledVoltageSource") {
                vs_map[elem->getName()] = vs_idx++;
            } else if (elem->getType() == "Inductor") {
                l_map[elem->getName()] = l_idx++;
            }
        }
        
        // Extract node voltages and branch currents
        result.node_voltages = extractNodeVoltages(solution, node_map);
        result.branch_currents = extractBranchCurrents(solution, node_map, vs_map, l_map);
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("DC analysis failed: ") + e.what();
    }
    
    return result;
}

AnalysisResult MNASolver::solveTransient(const Circuit& circuit) {
    AnalysisResult result;
    
    try {
        std::vector<double> time_points;
        double current_time = 0.0;
        while (current_time <= config.transient_end_time) {
            time_points.push_back(current_time);
            current_time += config.transient_timestep;
        }
        
        // Initialize time series storage
        NodeIndexMap node_map;
        std::vector<Node*> non_ground_nodes;
        circuit.getNonGroundNodes(non_ground_nodes, node_map);
        
        for (const auto& node : non_ground_nodes) {
            result.time_series[node->getId()] = std::vector<double>();
        }
        
        // Add voltage source currents to time series
        for (const auto& elem : circuit.getElements()) {
            if (elem->getType() == "IndependentVoltageSource" || 
                elem->getType() == "PulseVoltageSource" || 
                elem->getType() == "WaveformVoltageSource" ||
                elem->getType() == "PhaseVoltageSource" ||
                elem->getType() == "SinusoidalVoltageSource" ||
                elem->getType() == "ACVoltageSource") {
                result.time_series[elem->getName() + "_current"] = std::vector<double>();
            }
        }
        
        // Time stepping
        for (double t : time_points) {
            // Build MNA matrix for current time
            MNAMatrix mna_matrix;
            mna_matrix.build(circuit, true, t, config.transient_timestep);
            
            // Solve the system
            Vector solution = linear_solver->solve(mna_matrix.getA(), mna_matrix.getRHS());
            
            // Extract and store results for this time point
            std::map<std::string, int> vs_map, l_map;
            int vs_idx = 0, l_idx = 0;
            
            for (const auto& elem : circuit.getElements()) {
                if (elem->getType() == "IndependentVoltageSource" || 
                    elem->getType() == "PulseVoltageSource" || 
                    elem->getType() == "SinusoidalVoltageSource" ||
                    elem->getType() == "ACVoltageSource") {
                    vs_map[elem->getName()] = vs_idx++;
                } else if (elem->getType() == "Inductor") {
                    l_map[elem->getName()] = l_idx++;
                }
            }
            
            // Store node voltages
            auto node_voltages = extractNodeVoltages(solution, node_map);
            for (const auto& [node_id, voltage] : node_voltages) {
                if (result.time_series.count(node_id)) {
                    result.time_series[node_id].push_back(voltage);
                }
            }
            
            // Store branch currents
            auto branch_currents = extractBranchCurrents(solution, node_map, vs_map, l_map);
            for (const auto& [branch_name, current] : branch_currents) {
                if (result.time_series.count(branch_name)) {
                    result.time_series[branch_name].push_back(current);
                }
            }
            
            // Update circuit state for next iteration
            updateCircuitState(const_cast<Circuit&>(circuit), solution, node_map, vs_map, l_map);
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Transient analysis failed: ") + e.what();
    }
    
    return result;
}

AnalysisResult MNASolver::solveAC(const Circuit& circuit) {
    AnalysisResult result;
    
    try {
        // Create frequency points (logarithmically spaced)
        std::vector<double> frequencies;
        double freq_step = pow(config.ac_end_freq / config.ac_start_freq, 1.0 / (config.ac_points - 1));
        double current_freq = config.ac_start_freq;
        
        for (int i = 0; i < config.ac_points; ++i) {
            frequencies.push_back(current_freq);
            current_freq *= freq_step;
        }
        
        // Initialize frequency series storage
        NodeIndexMap node_map;
        std::vector<Node*> non_ground_nodes;
        circuit.getNonGroundNodes(non_ground_nodes, node_map);
        
        for (const auto& node : non_ground_nodes) {
            result.frequency_series[node->getId()] = std::vector<std::complex<double>>();
        }
        
        // AC analysis at each frequency
        for (double freq : frequencies) {
            double omega = 2.0 * M_PI * freq;
            
            // Build complex MNA matrix
            ComplexMNAMatrix complex_mna;
            std::map<std::string, int> ac_source_map;
            complex_mna.build(circuit, omega, node_map, ac_source_map);
            
            // Solve the complex system
            ComplexVector solution = complex_solver->solve(complex_mna.getA(), complex_mna.getRHS());
            
            // Store complex node voltages
            for (size_t i = 0; i < non_ground_nodes.size() && i < solution.size(); ++i) {
                const std::string& node_id = non_ground_nodes[i]->getId();
                if (result.frequency_series.count(node_id)) {
                    result.frequency_series[node_id].push_back(solution[i]);
                }
            }
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("AC analysis failed: ") + e.what();
    }
    
    return result;
}

AnalysisResult MNASolver::solveDCSweep(const Circuit& circuit, const std::string& sweep_component, 
                                      double start_value, double end_value, int steps) {
    AnalysisResult result;
    
    try {
        // Find the component to sweep
        Element* sweep_elem = circuit.getElement(sweep_component);
        if (!sweep_elem) {
            result.success = false;
            result.error_message = "Sweep component not found: " + sweep_component;
            return result;
        }
        
        double value_step = (end_value - start_value) / (steps - 1);
        
        // Initialize sweep storage
        NodeIndexMap node_map;
        std::vector<Node*> non_ground_nodes;
        circuit.getNonGroundNodes(non_ground_nodes, node_map);
        
        for (const auto& node : non_ground_nodes) {
            result.time_series[node->getId()] = std::vector<double>();
        }
        
        // Perform DC sweep
        for (int i = 0; i < steps; ++i) {
            double current_value = start_value + i * value_step;
            
            // Temporarily modify the component value
            double original_value = sweep_elem->getValue();
            const_cast<Element*>(sweep_elem)->setValue(current_value);
            
            // Solve DC analysis
            auto dc_result = solveDC(circuit);
            
            if (dc_result.success) {
                // Store results for this sweep point
                for (const auto& [node_id, voltage] : dc_result.node_voltages) {
                    if (result.time_series.count(node_id)) {
                        result.time_series[node_id].push_back(voltage);
                    }
                }
            }
            
            // Restore original value
            const_cast<Element*>(sweep_elem)->setValue(original_value);
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("DC sweep failed: ") + e.what();
    }
    
    return result;
}

void MNASolver::updateCircuitState(Circuit& circuit, const Vector& solution, 
                                  const NodeIndexMap& node_map, 
                                  const std::map<std::string, int>& vs_map,
                                  const std::map<std::string, int>& l_map) {
    // Update previous node voltages
    std::map<std::string, double> current_voltages;
    for (const auto& [node_id, index] : node_map) {
        if (index < solution.size()) {
            current_voltages[node_id] = solution[index];
        }
    }
    circuit.updatePreviousNodeVoltages(current_voltages);
    
    // Update previous inductor currents
    std::map<std::string, double> current_inductor_currents;
    int num_nodes = node_map.size();
    int num_vs = vs_map.size();
    
    for (const auto& [inductor_name, index] : l_map) {
        int solution_index = num_nodes + num_vs + index;
        if (solution_index < solution.size()) {
            current_inductor_currents[inductor_name] = solution[solution_index];
        }
    }
    circuit.updatePreviousInductorCurrents(current_inductor_currents);
}

std::map<std::string, double> MNASolver::extractNodeVoltages(const Vector& solution, 
                                                            const NodeIndexMap& node_map) {
    std::map<std::string, double> node_voltages;
    
    for (const auto& [node_id, index] : node_map) {
        if (index < solution.size()) {
            node_voltages[node_id] = solution[index];
        }
    }
    
    return node_voltages;
}

std::map<std::string, double> MNASolver::extractBranchCurrents(const Vector& solution, 
                                                              const NodeIndexMap& node_map,
                                                              const std::map<std::string, int>& vs_map,
                                                              const std::map<std::string, int>& l_map) {
    std::map<std::string, double> branch_currents;
    
    int num_nodes = node_map.size();
    
    // Extract voltage source currents
    for (const auto& [vs_name, index] : vs_map) {
        int solution_index = num_nodes + index;
        if (solution_index < solution.size()) {
            branch_currents[vs_name + "_current"] = solution[solution_index];
        }
    }
    
    // Extract inductor currents
    int num_vs = vs_map.size();
    for (const auto& [inductor_name, index] : l_map) {
        int solution_index = num_nodes + num_vs + index;
        if (solution_index < solution.size()) {
            branch_currents[inductor_name + "_current"] = solution[solution_index];
        }
    }
    
    return branch_currents;
}

void MNASolver::setConfig(const MNASolverConfig& new_config) {
    config = new_config;
    
    // Update solver based on new configuration
    if (config.use_lu_decomposition) {
        linear_solver = std::make_unique<LUDecompositionSolver>();
    } else {
        linear_solver = std::make_unique<GaussianEliminationSolver>();
    }
}

const MNASolverConfig& MNASolver::getConfig() const {
    return config;
}

bool MNASolver::validateCircuit(const Circuit& circuit) const {
    // Check if circuit has elements
    if (circuit.getElements().empty()) {
        return false;
    }
    
    // Check if ground node exists
    if (!circuit.checkGroundNodeExists()) {
        return false;
    }
    
    // Check connectivity
    if (!circuit.checkConnectivity()) {
        return false;
    }
    
    return true;
}

std::string MNASolver::getCircuitInfo(const Circuit& circuit) const {
    std::string info = "Circuit Information:\n";
    info += "Number of nodes: " + std::to_string(circuit.getNodes().size()) + "\n";
    info += "Number of elements: " + std::to_string(circuit.getElements().size()) + "\n";
    info += "Ground node: " + circuit.getGroundNodeId() + "\n";
    
    // Count element types
    std::map<std::string, int> element_counts;
    for (const auto& elem : circuit.getElements()) {
        element_counts[elem->getType()]++;
    }
    
    info += "Element breakdown:\n";
    for (const auto& [type, count] : element_counts) {
        info += "  " + type + ": " + std::to_string(count) + "\n";
    }
    
    return info;
}
