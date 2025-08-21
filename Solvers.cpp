#include "Solvers.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>

// --- MNAMatrix Implementation ---
MNAMatrix::MNAMatrix() = default;

void MNAMatrix::reset() {
    A_matrix.clear();
    b_vector.clear();
}

void MNAMatrix::build(const Circuit& circuit, bool is_transient, double currentTime, double timeStepIncrement) {
    reset();
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
        if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "SinusoidalVoltageSource" || type == "VoltageControlledVoltageSource") {
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
        }
        else if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "SinusoidalVoltageSource" || type == "VoltageControlledVoltageSource") {
            int n1_idx = node_map.count(elem->getNode1Id()) ? node_map.at(elem->getNode1Id()) : -1;
            int n2_idx = node_map.count(elem->getNode2Id()) ? node_map.at(elem->getNode2Id()) : -1;
            int vs_curr_idx = num_voltage_nodes + voltage_source_current_map.at(elem->getName());

            if (n1_idx != -1) { A_matrix[n1_idx][vs_curr_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[n2_idx][vs_curr_idx] -= 1.0; }
            if (n1_idx != -1) { A_matrix[vs_curr_idx][n1_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[vs_curr_idx][n2_idx] -= 1.0; }

            if (type == "PulseVoltageSource") {
                b_vector[vs_curr_idx] = static_cast<const PulseVoltageSource*>(elem.get())->getVoltageAtTime(currentTime);
            } else if (type == "SinusoidalVoltageSource") {
                 b_vector[vs_curr_idx] = static_cast<const SinusoidalVoltageSource*>(elem.get())->getVoltageAtTime(currentTime);
            } else if (type == "VoltageControlledVoltageSource") {
                auto* vcvs = static_cast<const VoltageControlledVoltageSource*>(elem.get());
                int ctrl_n1_idx = node_map.count(vcvs->getControlNode1Id()) ? node_map.at(vcvs->getControlNode1Id()) : -1;
                int ctrl_n2_idx = node_map.count(vcvs->getControlNode2Id()) ? node_map.at(vcvs->getControlNode2Id()) : -1;

                if (ctrl_n1_idx != -1) A_matrix[vs_curr_idx][ctrl_n1_idx] -= vcvs->getGain();
                if (ctrl_n2_idx != -1) A_matrix[vs_curr_idx][ctrl_n2_idx] += vcvs->getGain();

                b_vector[vs_curr_idx] = 0; // RHS is zero
            }
            else { // IndependentVoltageSource
                b_vector[vs_curr_idx] = elem->getValue();
            }
        }
        else if (type == "Inductor") {
            int n1_idx = node_map.count(elem->getNode1Id()) ? node_map.at(elem->getNode1Id()) : -1;
            int n2_idx = node_map.count(elem->getNode2Id()) ? node_map.at(elem->getNode2Id()) : -1;
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
            int ccvs_curr_idx = num_voltage_nodes + num_voltage_sources + num_inductors + ccvs_current_map.at(elem->getName());
            
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
        if (elem->getType() == "IndependentVoltageSource" || elem->getType() == "SinusoidalVoltageSource") {
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
        if (type == "Resistor") admittance = 1.0 / elem->getValue();
        else if (type == "Capacitor") admittance = j * omega * elem->getValue();
        else if (type == "Inductor") admittance = (omega > 1e-9) ? 1.0 / (j * omega * elem->getValue()) : 1e12;
        else if (ac_source_map.count(elem->getName())) {
            int vs_idx = num_nodes + ac_source_map.at(elem->getName());
            if (n1_idx != -1) { A_matrix[n1_idx][vs_idx] += 1.0; A_matrix[vs_idx][n1_idx] += 1.0; }
            if (n2_idx != -1) { A_matrix[n2_idx][vs_idx] -= 1.0; A_matrix[vs_idx][n2_idx] -= 1.0; }
        }
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
