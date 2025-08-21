#include "SimpleAnalysis.h"
#include "ErrorManager.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <limits>
#include <iomanip>

// Debug: Confirm this file is being compiled
static bool debug_flag = []() {
    LOG_INFO("SimpleAnalysis.cpp compiled and loaded");
    return true;
}();

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// MNASystem Implementation
MNASystem::MNASystem(const Circuit& circuit) {
    cout << "[MNASystem] Constructing from Circuit..." << endl;
    cout << "[MNASystem] Ground node: " << circuit.getGroundNodeId() << endl;
    cout << "[MNASystem] Elements: " << circuit.getElements().size() << endl;

    // First, merge nodes connected by wires
    unordered_map<string, string> node_equivalence;
    string gnd = circuit.getGroundNodeId();
    
    // Initialize each node to map to itself
    for (const auto& elem_ptr : circuit.getElements()) {
        string n1 = elem_ptr->getNode1Id();
        string n2 = elem_ptr->getNode2Id();
        if (!n1.empty()) node_equivalence[n1] = n1;
        if (!n2.empty()) node_equivalence[n2] = n2;
    }
    
    // Merge nodes connected by wires
    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getType() == "Wire") {
            string n1 = elem_ptr->getNode1Id();
            string n2 = elem_ptr->getNode2Id();
            
            if (!n1.empty() && !n2.empty()) {
                // Find the root representative for each node
                string root1 = n1;
                while (node_equivalence[root1] != root1) {
                    root1 = node_equivalence[root1];
                }
                string root2 = n2;
                while (node_equivalence[root2] != root2) {
                    root2 = node_equivalence[root2];
                }
                
                // Merge the two equivalence classes
                if (root1 != root2) {
                    // Choose the smaller node name as the representative
                    string representative = (root1 < root2) ? root1 : root2;
                    string other = (root1 < root2) ? root2 : root1;
                    
                    node_equivalence[other] = representative;
                    cout << "[MNASystem] Wire " << elem_ptr->getName() << ": merging " << other << " -> " << representative << endl;
                }
            }
        }
    }
    
    // Flatten the equivalence map
    for (auto& pair : node_equivalence) {
        string current = pair.first;
        while (node_equivalence[current] != current) {
            current = node_equivalence[current];
        }
        pair.second = current;
    }
    
    // Use the circuit's method to get non-ground nodes for consistency
    vector<Node*> non_ground_nodes;
    NodeIndexMap circuit_node_map;
    circuit.getNonGroundNodes(non_ground_nodes, circuit_node_map);

    unordered_set<string> nodes;
    for (const auto& elem_ptr : circuit.getElements()) {
        string n1 = elem_ptr->getNode1Id();
        string n2 = elem_ptr->getNode2Id();
        
        // Map to representative nodes
        if (!n1.empty()) n1 = node_equivalence[n1];
        if (!n2.empty()) n2 = node_equivalence[n2];
        
        cout << "[MNASystem] Element " << elem_ptr->getName() << ": " << elem_ptr->getNode1Id() << "->" << elem_ptr->getNode2Id() 
             << " (mapped to " << n1 << "->" << n2 << ")" << endl;
        
        // Only add nodes that are in the circuit's non-ground node list and not ground
        if (circuit_node_map.count(elem_ptr->getNode1Id()) && n1 != gnd) nodes.insert(n1);
        if (circuit_node_map.count(elem_ptr->getNode2Id()) && n2 != gnd) nodes.insert(n2);
    }
    n = nodes.size();
    cout << "[MNASystem] Independent non-ground nodes after wire merging: " << n << endl;
    for (const auto& node : nodes) {
        cout << "[MNASystem]   " << node << endl;
    }

    nodeNames.assign(nodes.begin(), nodes.end());
    for (int i = 0; i < n; i++) nodeIndexMap[nodeNames[i]] = i;
    
    // Store the node equivalence map for use during stamping
    nodeEquivalence = node_equivalence;

    // Count voltage sources and check for shorted ones
    for (const auto& elem_ptr : circuit.getElements()) {
        string type = elem_ptr->getType();
        cout << "[MNASystem] Checking element: " << elem_ptr->getName() << " type: " << type << endl;
        if (type == "IndependentVoltageSource" || type == "PulseVoltageSource" ||
            type == "SinusoidalVoltageSource" || type == "ACVoltageSource" ||
            type == "VoltageControlledVoltageSource" || type == "CurrentControlledVoltageSource") {
            
            string n1 = elem_ptr->getNode1Id();
            string n2 = elem_ptr->getNode2Id();
            string gnd = circuit.getGroundNodeId();
            
            // Check if voltage source is shorted
            bool is_shorted = false;
            if (n1 == n2) {
                cout << "[MNASystem] WARNING: Voltage source " << elem_ptr->getName() << " is shorted (same node on both terminals)" << endl;
                is_shorted = true;
            } else if (n1 == gnd && n2 == gnd) {
                cout << "[MNASystem] WARNING: Voltage source " << elem_ptr->getName() << " is shorted to ground" << endl;
                is_shorted = true;
            } else {
                // Check if either terminal is shorted to ground via wires
                bool n1_shorted_to_gnd = false;
                bool n2_shorted_to_gnd = false;
                
                for (const auto& wire_elem : circuit.getElements()) {
                    if (wire_elem->getType() == "Wire") {
                        string w1 = wire_elem->getNode1Id();
                        string w2 = wire_elem->getNode2Id();
                        
                        // Check if n1 is connected to ground via this wire
                        if ((w1 == n1 && w2 == gnd) || (w1 == gnd && w2 == n1)) {
                            n1_shorted_to_gnd = true;
                        }
                        // Check if n2 is connected to ground via this wire
                        if ((w1 == n2 && w2 == gnd) || (w1 == gnd && w2 == n2)) {
                            n2_shorted_to_gnd = true;
                        }
                    }
                }
                
                if (n1_shorted_to_gnd && n2_shorted_to_gnd) {
                    cout << "[MNASystem] WARNING: Voltage source " << elem_ptr->getName() << " is shorted to ground via wires" << endl;
                    is_shorted = true;
                } else if (n1_shorted_to_gnd || n2_shorted_to_gnd) {
                    // Only one terminal connected to ground - this is NORMAL for voltage sources
                    // Check if the other terminal has a valid path to circuit elements
                    bool has_valid_path = false;
                    string other_terminal = n1_shorted_to_gnd ? n2 : n1;
                    
                    // Check if the other terminal connects to any non-ground elements
                    for (const auto& elem_ptr2 : circuit.getElements()) {
                        if (elem_ptr2->getType() != "Wire" && elem_ptr2->getType() != "Ground") {
                            if (elem_ptr2->getNode1Id() == other_terminal || elem_ptr2->getNode2Id() == other_terminal) {
                                has_valid_path = true;
                                break;
                            }
                        }
                    }
                    
                    if (has_valid_path) {
                        cout << "[MNASystem] INFO: Voltage source " << elem_ptr->getName() << " has one terminal connected to ground (NORMAL)" << endl;
                        cout << "[MNASystem] INFO: Other terminal connects to circuit elements - voltage source is USABLE" << endl;
                    } else {
                        cout << "[MNASystem] WARNING: Voltage source " << elem_ptr->getName() << " has no valid path to circuit elements" << endl;
                        is_shorted = true;
                    }
                }
            }
            
            if (!is_shorted) {
                vsNames.push_back(elem_ptr->getName());
                cout << "[MNASystem] Added voltage source: " << elem_ptr->getName() << endl;
            } else {
                cout << "[MNASystem] Skipping shorted voltage source: " << elem_ptr->getName() << endl;
            }
        }
    }
    m = vsNames.size();
    size = n + m;
    for (int i = 0; i < m; i++) vsIndexMap[vsNames[i]] = i;

    cout << "[MNASystem] Final system size: " << size << " (n=" << n << " nodes, m=" << m << " voltage sources)" << endl;
    cout << "[MNASystem] Voltage sources: ";
    for (const auto& vs : vsNames) {
        cout << vs << " ";
    }
    cout << endl;
    
    cout << "[MNASystem] vsIndexMap contents:" << endl;
    for (const auto& pair : vsIndexMap) {
        cout << "[MNASystem]   " << pair.first << " -> " << pair.second << endl;
    }

    mat.resize(size, vector<double>(size, 0));
    rhs.resize(size, 0);
    solution.resize(size, 0);
}

void MNASystem::reset() {
    for (int i = 0; i < size; i++) {
        fill(mat[i].begin(), mat[i].end(), 0);
        rhs[i] = 0;
    }
}

void MNASystem::stampResistor(const string& n1, const string& n2, double R) {
    // Map nodes to their representative nodes
    string mapped_n1 = nodeEquivalence.count(n1) ? nodeEquivalence[n1] : n1;
    string mapped_n2 = nodeEquivalence.count(n2) ? nodeEquivalence[n2] : n2;
    
    double g = 1.0 / R;
    int i = nodeIndexMap.count(mapped_n1) ? nodeIndexMap[mapped_n1] : -1;
    int j = nodeIndexMap.count(mapped_n2) ? nodeIndexMap[mapped_n2] : -1;
    if (i != -1) {
        mat[i][i] += g;
        if (j != -1) {
            mat[i][j] -= g;
            mat[j][i] -= g;
            mat[j][j] += g;
        }
    } else if (j != -1) {
        mat[j][j] += g;
    }
}

void MNASystem::stampCurrentSource(const string& n1, const string& n2, double I) {
    // Map nodes to their representative nodes
    string mapped_n1 = nodeEquivalence.count(n1) ? nodeEquivalence[n1] : n1;
    string mapped_n2 = nodeEquivalence.count(n2) ? nodeEquivalence[n2] : n2;
    
    int i = nodeIndexMap.count(mapped_n1) ? nodeIndexMap[mapped_n1] : -1;
    int j = nodeIndexMap.count(mapped_n2) ? nodeIndexMap[mapped_n2] : -1;
    if (i != -1) rhs[i] -= I;
    if (j != -1) rhs[j] += I;
}

void MNASystem::stampVoltageSource(const string& name, const string& n1, const string& n2, double V) {
    // Map nodes to their representative nodes
    string mapped_n1 = nodeEquivalence.count(n1) ? nodeEquivalence[n1] : n1;
    string mapped_n2 = nodeEquivalence.count(n2) ? nodeEquivalence[n2] : n2;
    
    cout << "[stampVoltageSource] Stamping " << name << ": " << n1 << " -> " << n2 << " (mapped to " << mapped_n1 << " -> " << mapped_n2 << ") = " << V << "V" << endl;
    cout << "[stampVoltageSource] vsIndexMap has " << name << ": " << (vsIndexMap.count(name) ? "YES" : "NO") << endl;
    
    if (!vsIndexMap.count(name)) {
        cout << "[stampVoltageSource] ERROR: Voltage source " << name << " not found in vsIndexMap!" << endl;
        return;
    }
    
    int row = n + vsIndexMap[name];
    int i = nodeIndexMap.count(mapped_n1) ? nodeIndexMap[mapped_n1] : -1;
    int j = nodeIndexMap.count(mapped_n2) ? nodeIndexMap[mapped_n2] : -1;
    
    cout << "[stampVoltageSource] Row: " << row << ", i: " << i << ", j: " << j << endl;

    // Check if both terminals are connected to the same node (shorted voltage source)
    if (i == j && i != -1) {
        // Both terminals connected to the same non-ground node
        // This creates a short circuit - set the voltage source current to 0
        cout << "[stampVoltageSource] Both terminals connected to same node - shorted" << endl;
        mat[row][i] = 1;
        mat[i][row] = 1;
        rhs[row] = 0; // Force current to be zero
        return;
    }
    
    // Check if both terminals are connected to ground (both indices are -1)
    if (i == -1 && j == -1) {
        // Both terminals connected to ground - voltage source is shorted
        // Set the voltage source current to 0
        cout << "[stampVoltageSource] Both terminals connected to ground - shorted" << endl;
        rhs[row] = 0;
        return;
    }

    // Normal case: voltage source between different nodes
    cout << "[stampVoltageSource] Normal case: stamping voltage source" << endl;
    if (i != -1) {
        mat[row][i] = 1;
        mat[i][row] = 1;
        cout << "[stampVoltageSource] Stamped row " << row << " <-> node " << i << endl;
    }
    if (j != -1) {
        mat[row][j] = -1;
        mat[j][row] = -1;
        cout << "[stampVoltageSource] Stamped row " << row << " <-> node " << j << endl;
    }
    rhs[row] = V;
    cout << "[stampVoltageSource] Set RHS[" << row << "] = " << V << endl;
}

void MNASystem::stampCapacitor(const string& n1, const string& n2, double C, double dt) {
    double geq = C / dt;
    stampResistor(n1, n2, 1.0 / geq);
}

void MNASystem::stampInductor(const string& n1, const string& n2, double L, double dt) {
    double geq = dt / L;
    stampResistor(n1, n2, 1.0 / geq);
}

bool MNASystem::solveGaussian() {
    static int solve_count = 0;
    solve_count++;
    bool debug_solve = (solve_count <= 3); // Debug first 3 solves
    
    if (debug_solve) {
        cout << "[Gaussian] Starting solve #" << solve_count << " (system size: " << size << ")" << endl;
    }
    
    vector<vector<double>> A = mat;
    vector<double> b = rhs;
    
    // Forward elimination with partial pivoting
    for (int p = 0; p < size; p++) {
        if (debug_solve) {
            cout << "[Gaussian] Step " << p << "/" << (size-1) << " - Finding pivot for column " << p << endl;
        }
        
        int max = p;
        for (int i = p + 1; i < size; i++)
            if (fabs(A[i][p]) > fabs(A[max][p])) max = i;

        if (debug_solve) {
            cout << "[Gaussian]   Pivot element A[" << max << "][" << p << "] = " << A[max][p] << endl;
        }

        if (fabs(A[max][p]) < 1e-12) {
            if (debug_solve) {
                cout << "[Gaussian]   ❌ FAILED: Pivot too small (" << A[max][p] << ") - singular matrix!" << endl;
            }
            return false;
        }

        if (max != p) {
            if (debug_solve) {
                cout << "[Gaussian]   Swapping rows " << p << " and " << max << endl;
            }
            swap(A[p], A[max]);
            swap(b[p], b[max]);
        }

        // Eliminate column below pivot
        for (int i = p + 1; i < size; i++) {
            double alpha = A[i][p] / A[p][p];
            if (debug_solve && abs(alpha) > 1e-6) {
                cout << "[Gaussian]     Eliminating row " << i << " with alpha = " << alpha << endl;
            }
            b[i] -= alpha * b[p];
            for (int j = p; j < size; j++)
                A[i][j] -= alpha * A[p][j];
        }
        
        if (debug_solve) {
            cout << "[Gaussian]   After elimination, column " << p << ":" << endl;
            for (int i = 0; i < size; i++) {
                cout << "[Gaussian]     A[" << i << "][" << p << "] = " << A[i][p] << endl;
            }
        }
    }

    if (debug_solve) {
        cout << "[Gaussian] Forward elimination complete. Starting back substitution..." << endl;
    }

    // Back substitution
    for (int i = size - 1; i >= 0; i--) {
        double sum = 0;
        for (int j = i + 1; j < size; j++)
            sum += A[i][j] * solution[j];
        solution[i] = (b[i] - sum) / A[i][i];
        
        if (debug_solve) {
            cout << "[Gaussian]   x[" << i << "] = (" << b[i] << " - " << sum << ") / " << A[i][i] 
                 << " = " << solution[i] << endl;
        }
    }
    
    if (debug_solve) {
        cout << "[Gaussian] ✅ Solve #" << solve_count << " completed successfully!" << endl;
        cout << "[Gaussian] Final solution:" << endl;
        for (int i = 0; i < size; i++) {
            cout << "[Gaussian]   x[" << i << "] = " << solution[i] << endl;
        }
    }
    
    return true;
}

double MNASystem::getNodeVoltage(const string& node) {
    if (nodeIndexMap.count(node)) {
        return solution[nodeIndexMap[node]];
    }
    // If node is not in the map, it might be ground (which is always 0V)
    // or it might be an error. Let's add some debug info.
    static int ground_warnings = 0;
    if (ground_warnings < 5) {
        cout << "[MNASystem] WARNING: Node '" << node << "' not found in node map (might be ground)" << endl;
        ground_warnings++;
    }
    return 0.0;  // Ground is at 0V
}

double MNASystem::getCurrentThroughVS(const string& vsName) {
    if (vsIndexMap.count(vsName)) return solution[n + vsIndexMap[vsName]];
    return 0;
}

void MNASystem::printSolution() {
    for (int i = 0; i < n; i++)
        cout << "  V(" << nodeNames[i] << ") = " << solution[i] << " V" << endl;
    for (int i = 0; i < m; i++)
        cout << "  I(" << vsNames[i] << ") = " << solution[n + i] << " A" << endl;
}

// ComplexMNASystem Implementation
ComplexMNASystem::ComplexMNASystem(const Circuit& circuit) {
    unordered_set<string> nodes;
    for (const auto& elem_ptr : circuit.getElements()) {
        string n1 = elem_ptr->getNode1Id();
        string n2 = elem_ptr->getNode2Id();
        if (n1 != "0" && circuit.getGroundNodeId() != n1) nodes.insert(n1);
        if (n2 != "0" && circuit.getGroundNodeId() != n2) nodes.insert(n2);
    }
    n = nodes.size();
    nodeNames.assign(nodes.begin(), nodes.end());
    for (int i = 0; i < n; i++) nodeIndexMap[nodeNames[i]] = i;

    for (const auto& elem_ptr : circuit.getElements()) {
        string type = elem_ptr->getType();
        if (type == "IndependentVoltageSource" || type == "SinusoidalVoltageSource" ||
            type == "ACVoltageSource" || type == "PulseVoltageSource" ||
            type == "VoltageControlledVoltageSource" || type == "CurrentControlledVoltageSource") {
            vsNames.push_back(elem_ptr->getName());
        }
    }
    m = vsNames.size();
    size = n + m;
    for (int i = 0; i < m; i++) vsIndexMap[vsNames[i]] = i;

    mat.assign(size, vector<cd>(size, cd(0.0, 0.0)));
    rhs.assign(size, cd(0.0, 0.0));
    solution.assign(size, cd(0.0, 0.0));
}

void ComplexMNASystem::reset() {
    for (int i = 0; i < size; i++) {
        fill(mat[i].begin(), mat[i].end(), cd(0.0, 0.0));
        rhs[i] = cd(0.0, 0.0);
    }
}

void ComplexMNASystem::stampImpedance(const string& n1, const string& n2, cd Z) {
    if (abs(Z) == 0.0) return;
    cd g = cd(1.0, 0.0) / Z;
    int i = nodeIndexMap.count(n1) ? nodeIndexMap[n1] : -1;
    int j = nodeIndexMap.count(n2) ? nodeIndexMap[n2] : -1;
    if (i != -1) {
        mat[i][i] += g;
        if (j != -1) {
            mat[i][j] -= g;
            mat[j][i] -= g;
            mat[j][j] += g;
        }
    } else if (j != -1) {
        mat[j][j] += g;
    }
}

void ComplexMNASystem::stampCurrentSource(const string& n1, const string& n2, cd I) {
    int i = nodeIndexMap.count(n1) ? nodeIndexMap[n1] : -1;
    int j = nodeIndexMap.count(n2) ? nodeIndexMap[n2] : -1;
    if (i != -1) rhs[i] -= I;
    if (j != -1) rhs[j] += I;
}

void ComplexMNASystem::stampVoltageSource(const string& name, const string& n1, const string& n2, cd V) {
    int row = n + vsIndexMap[name];
    int i = nodeIndexMap.count(n1) ? nodeIndexMap[n1] : -1;
    int j = nodeIndexMap.count(n2) ? nodeIndexMap[n2] : -1;

    if (i != -1) {
        mat[row][i] = cd(1.0, 0.0);
        mat[i][row] = cd(1.0, 0.0);
    }
    if (j != -1) {
        mat[row][j] = cd(-1.0, 0.0);
        mat[j][row] = cd(-1.0, 0.0);
    }
    rhs[row] = V;
}

bool ComplexMNASystem::solveGaussian() {
    auto A = mat;
    auto b = rhs;
    for (int p = 0; p < size; p++) {
        int max = p;
        for (int i = p + 1; i < size; i++)
            if (abs(A[i][p]) > abs(A[max][p])) max = i;

        if (abs(A[max][p]) < 1e-18) return false;

        swap(A[p], A[max]);
        swap(b[p], b[max]);

        for (int i = p + 1; i < size; i++) {
            cd alpha = A[i][p] / A[p][p];
            b[i] -= alpha * b[p];
            for (int j = p; j < size; j++)
                A[i][j] -= alpha * A[p][j];
        }
    }

    for (int i = size - 1; i >= 0; i--) {
        cd sum = cd(0.0, 0.0);
        for (int j = i + 1; j < size; j++)
            sum += A[i][j] * solution[j];
        solution[i] = (b[i] - sum) / A[i][i];
    }
    return true;
}

cd ComplexMNASystem::getNodeVoltageComplex(const string& node) const {
    if (nodeIndexMap.count(node)) return solution[nodeIndexMap.at(node)];
    return cd(0.0, 0.0);
}

cd ComplexMNASystem::getCurrentThroughVSComplex(const string& vsName) const {
    if (vsIndexMap.count(vsName)) return solution[n + vsIndexMap.at(vsName)];
    return cd(0.0, 0.0);
}

// TransientSimulator Implementation
void TransientSimulator::init(const Circuit& circuit) {
    // Initialize with zero initial conditions for capacitors and inductors
    // This assumes the circuit starts from rest (all voltages/currents = 0)
    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getType() == "Capacitor") {
            capVoltages[elem_ptr->getName()] = 0.0;  // Initial capacitor voltage = 0V
        }
        if (elem_ptr->getType() == "Inductor") {
            indCurrents[elem_ptr->getName()] = 0.0;  // Initial inductor current = 0A
        }
    }
}

void TransientSimulator::update(const Circuit& circuit, MNASystem& sys, double dt) {
    for (const auto& elem_ptr : circuit.getElements()) {
        const string& name = elem_ptr->getName();
        string ty = elem_ptr->getType();
        if (ty == "Capacitor") {
            double v = sys.getNodeVoltage(elem_ptr->getNode1Id()) - sys.getNodeVoltage(elem_ptr->getNode2Id());
            capVoltages[name] = v;
        } else if (ty == "Inductor") {
            double v = sys.getNodeVoltage(elem_ptr->getNode1Id()) - sys.getNodeVoltage(elem_ptr->getNode2Id());
            double i = indCurrents[name] + (v * dt) / elem_ptr->getValue();
            indCurrents[name] = i;
        }
    }
}

double TransientSimulator::getCapCurrent(const string& name, double C, double dt) {
    double v_prev = capVoltages.count(name) ? capVoltages[name] : 0.0;
    return (C / dt) * v_prev;
}

double TransientSimulator::getIndCurrent(const string& name) {
    return indCurrents.count(name) ? indCurrents[name] : 0.0;
}

// CircuitAnalyzer Implementation
bool CircuitAnalyzer::analyzeDC(Circuit& circuit) {
    if (circuit.getGroundNodeId().empty()) {
        cout << "Error: Define ground first" << endl;
        return false;
    }

    MNASystem sys(circuit);
    sys.reset();

    for (const auto& elem_ptr : circuit.getElements()) {
        string ty = elem_ptr->getType();
        if (ty == "Resistor") {
            sys.stampResistor(elem_ptr->getNode1Id(), elem_ptr->getNode2Id(), elem_ptr->getValue());
        } else if (ty == "IndependentVoltageSource" || ty == "SinusoidalVoltageSource" || ty == "ACVoltageSource" || ty == "PulseVoltageSource") {
            double v = elem_ptr->getValue();
            if (ty == "PulseVoltageSource") {
                // Use the current voltage value for pulse sources
                v = dynamic_cast<const PulseVoltageSource*>(elem_ptr.get())->getCurrentVoltageValue();
            }
            sys.stampVoltageSource(elem_ptr->getName(), elem_ptr->getNode1Id(), elem_ptr->getNode2Id(), v);
        } else if (ty == "IndependentCurrentSource") {
            sys.stampCurrentSource(elem_ptr->getNode1Id(), elem_ptr->getNode2Id(), elem_ptr->getValue());
        }
    }

    cout << "\nDC Analysis Results:" << endl;
    if (sys.solveGaussian()) {
        sys.printSolution();
        return true;
    } else {
        cout << "Error: Singular matrix" << endl;
        return false;
    }
}

TransientSeries CircuitAnalyzer::analyzeTransientMulti(Circuit& circuit, double t_step, double t_total, const vector<string>& desiredSignals) {
    TransientSeries out;

    // Debug: Confirm enhanced logging is being called
    LOG_INFO("===== ENHANCED TRANSIENT ANALYSIS FUNCTION CALLED =====");
    LOG_INFO("t_step = " + std::to_string(t_step) + ", t_total = " + std::to_string(t_total));
    LOG_INFO("desiredSignals size = " + std::to_string(desiredSignals.size()));

    // Enhanced logging header
    LOG_INFO("================================================================================ TRANSIENT ANALYSIS STARTED ================================================================================");
    LOG_INFO("Parameters: Time step (dt): " + std::to_string(t_step) + " seconds, Total time: " + std::to_string(t_total) + " seconds");
    LOG_INFO("Expected steps: " + std::to_string(static_cast<int>(t_total / t_step) + 1) + " time points");
    LOG_INFO("Signals to analyze: " + std::to_string(desiredSignals.size()));

    // Detailed circuit topology analysis
    LOG_INFO("CIRCUIT TOPOLOGY ANALYSIS:");
    LOG_INFO("Total elements: " + std::to_string(circuit.getElements().size()));
    LOG_INFO("Total nodes: " + std::to_string(circuit.getNodes().size()));
    LOG_INFO("Ground node: " + circuit.getGroundNodeId());
    LOG_INFO("Non-ground nodes: " + std::to_string(circuit.getNodes().size() - 1));

    // Count different element types for better insight
    int voltage_sources = 0, resistors = 0, capacitors = 0, inductors = 0, grounds = 0, wires = 0;
    LOG_INFO("ELEMENT INVENTORY:");
    for (const auto& elem_ptr : circuit.getElements()) {
        string type = elem_ptr->getType();
        string element_info = elem_ptr->getName() + " (" + type + ") [" + elem_ptr->getNode1Id() + " → " + elem_ptr->getNode2Id() + "]";

        if (type == "Resistor") {
            element_info += " R = " + std::to_string(elem_ptr->getValue()) + " Ω";
            resistors++;
        } else if (type == "Capacitor") {
            element_info += " C = " + std::to_string(elem_ptr->getValue()) + " F";
            capacitors++;
        } else if (type == "Inductor") {
            element_info += " L = " + std::to_string(elem_ptr->getValue()) + " H";
            inductors++;
        } else if (type == "SinusoidalVoltageSource" || type == "IndependentVoltageSource" || type == "PulseVoltageSource" || type == "ACVoltageSource") {
            element_info += " V = " + std::to_string(elem_ptr->getValue()) + " V";
            voltage_sources++;
        } else if (type == "Ground") {
            grounds++;
        } else if (type == "Wire") {
            wires++;
        }
        LOG_INFO(element_info);
    }

    LOG_INFO("ELEMENT SUMMARY:");
    LOG_INFO("Voltage Sources: " + std::to_string(voltage_sources));
    LOG_INFO("Resistors: " + std::to_string(resistors));
    LOG_INFO("Capacitors: " + std::to_string(capacitors));
    LOG_INFO("Inductors: " + std::to_string(inductors));
    LOG_INFO("Grounds: " + std::to_string(grounds));
    LOG_INFO("Wires: " + std::to_string(wires));

    // Parameter validation
    if (t_step <= 0.0) {
        cerr << "[SimpleAnalysis] ERROR: Time step must be positive! Got t_step=" << t_step << endl;
        return out;
    }
    if (t_total <= 0.0) {
        cerr << "[SimpleAnalysis] ERROR: Total time must be positive! Got t_total=" << t_total << endl;
        return out;
    }
    if (t_step > t_total) {
        cerr << "[SimpleAnalysis] ERROR: Time step (" << t_step << ") cannot be larger than total time (" << t_total << ")!" << endl;
        return out;
    }

    cout << "\n🎛️  ANALYSIS SIGNALS:" << endl;
    cout << "   • Circuit connectivity: " << (circuit.checkConnectivity() ? "✅ CONNECTED" : "❌ DISCONNECTED") << endl;
    cout << "   • Signals to monitor: " << desiredSignals.size() << endl;

    for (size_t i = 0; i < desiredSignals.size(); ++i) {
        const string& sig = desiredSignals[i];
        cout << "     [" << (i+1) << "] " << sig;

        // Parse signal type for better description
        if (sig.substr(0, 2) == "V(") {
            string node = sig.substr(2, sig.size() - 3);
            cout << " → Node voltage at " << node;
        } else if (sig.substr(0, 2) == "I(") {
            string elem = sig.substr(2, sig.size() - 3);
            cout << " → Current through " << elem;
        }
        cout << endl;
    }

    // Use voltage_sources count from element inventory above
    
    if (voltage_sources == 0) {
        cout << "\n⚠️  VALIDATION WARNING:" << endl;
        cout << "   • No voltage sources found in circuit!" << endl;
        cout << "   • Circuit will produce zero response" << endl;
        cout << "   • Please add a voltage source to the circuit" << endl;
    }

    for (const auto& elem_ptr : circuit.getElements()) {
        cout << "[SimpleAnalysis]   " << elem_ptr->getName() << " (" << elem_ptr->getType() << ") "
             << elem_ptr->getNode1Id() << " -> " << elem_ptr->getNode2Id()
             << " Value: " << elem_ptr->getValue();

        // Special handling for sinusoidal sources to show their parameters
        if (elem_ptr->getType() == "SinusoidalVoltageSource") {
            auto* sin_src = dynamic_cast<SinusoidalVoltageSource*>(elem_ptr.get());
            if (sin_src) {
                cout << " [Amp: " << sin_src->getAmplitude()
                     << "V, Freq: " << sin_src->getFrequency()
                     << "Hz, DC: " << sin_src->getDCOffset() << "V]";

                // Check for zero values that would result in zero voltage
                if (sin_src->getAmplitude() == 0.0) {
                    cout << " ⚠️ WARNING: Zero amplitude!";
                }
                if (sin_src->getFrequency() == 0.0) {
                    cout << " ⚠️ WARNING: Zero frequency!";
                }
            }
        }
        cout << endl;
    }

    // Check for short-circuited voltage sources
    bool hasShortCircuitedVS = false;
    for (const auto& elem_ptr : circuit.getElements()) {
        if (elem_ptr->getType() == "SinusoidalVoltageSource" ||
            elem_ptr->getType() == "IndependentVoltageSource" ||
            elem_ptr->getType() == "PulseVoltageSource") {
            string n1 = elem_ptr->getNode1Id();
            string n2 = elem_ptr->getNode2Id();
            string gnd = circuit.getGroundNodeId();
            if ((n1 == gnd && n2 == gnd) || (n1 == n2)) {
                cout << "[SimpleAnalysis] WARNING: Voltage source " << elem_ptr->getName()
                     << " is short-circuited (both terminals at same potential)!" << endl;
                hasShortCircuitedVS = true;
            }
        }
    }

    if (circuit.getGroundNodeId().empty()) {
        cerr << "[SimpleAnalysis] Error: Define ground first" << endl;
        return out;
    }

    // Initialize transient simulator
    cout << "\n🚀 INITIALIZING TRANSIENT SIMULATOR:" << endl;
    TransientSimulator transSim;
    transSim.init(circuit);
    cout << "   • Transient simulator initialized" << endl;
    cout << "   • Capacitor voltages initialized: " << transSim.getCapVoltages().size() << endl;
    cout << "   • Inductor currents initialized: " << transSim.getIndCurrents().size() << endl;

    // Validate voltage sources
    LOG_INFO("VOLTAGE SOURCE VALIDATION:");
    LOG_INFO("Checking " + std::to_string(circuit.getElements().size()) + " elements for voltage sources");
    bool found_voltage_source = false;
    bool found_usable_voltage_source = false;
    for (const auto& elem_ptr : circuit.getElements()) {
        string type = elem_ptr->getType();
        LOG_INFO("Element: " + elem_ptr->getName() + " type: " + type);
        if (type == "SinusoidalVoltageSource" || type == "IndependentVoltageSource" ||
            type == "PulseVoltageSource" || type == "ACVoltageSource") {
            found_voltage_source = true;
            LOG_INFO("Found voltage source: " + elem_ptr->getName());
            
            // Check if voltage source is shorted
            string n1 = elem_ptr->getNode1Id();
            string n2 = elem_ptr->getNode2Id();
            string gnd = circuit.getGroundNodeId();
            bool is_shorted = false;
            
            if (n1 == n2) {
                LOG_INFO("WARNING: Voltage source " + elem_ptr->getName() + " is shorted (same node on both terminals)");
                is_shorted = true;
            } else if (n1 == gnd && n2 == gnd) {
                LOG_INFO("WARNING: Voltage source " + elem_ptr->getName() + " is shorted to ground");
                is_shorted = true;
            } else {
                // Check if either terminal is shorted to ground via wires
                bool n1_shorted_to_gnd = false;
                bool n2_shorted_to_gnd = false;
                
                LOG_INFO("Checking voltage source " + elem_ptr->getName() + " terminals: n1=" + n1 + ", n2=" + n2 + ", gnd=" + gnd);
                
                for (const auto& wire_elem : circuit.getElements()) {
                    if (wire_elem->getType() == "Wire") {
                        string w1 = wire_elem->getNode1Id();
                        string w2 = wire_elem->getNode2Id();
                        LOG_INFO("Checking wire: " + w1 + " -> " + w2);
                        
                        // Check if n1 is connected to ground via this wire
                        if ((w1 == n1 && w2 == gnd) || (w1 == gnd && w2 == n1)) {
                            n1_shorted_to_gnd = true;
                            LOG_INFO("Found n1 shorted to ground via wire: " + w1 + " -> " + w2);
                        }
                        // Check if n2 is connected to ground via this wire
                        if ((w1 == n2 && w2 == gnd) || (w1 == gnd && w2 == n2)) {
                            n2_shorted_to_gnd = true;
                            LOG_INFO("Found n2 shorted to ground via wire: " + w1 + " -> " + w2);
                        }
                    }
                }
                
                LOG_INFO("Short detection results: n1_shorted=" + string(n1_shorted_to_gnd ? "true" : "false") + 
                        ", n2_shorted=" + string(n2_shorted_to_gnd ? "true" : "false"));
                
                if (n1_shorted_to_gnd && n2_shorted_to_gnd) {
                    LOG_INFO("WARNING: Voltage source " + elem_ptr->getName() + " is shorted to ground via wires");
                    is_shorted = true;
                } else if (n1_shorted_to_gnd || n2_shorted_to_gnd) {
                    // Only one terminal connected to ground - this is NORMAL for voltage sources
                    // Check if the other terminal has a valid path to circuit elements
                    bool has_valid_path = false;
                    string other_terminal = n1_shorted_to_gnd ? n2 : n1;
                    
                    // Check if the other terminal connects to any non-ground elements
                    for (const auto& elem_ptr2 : circuit.getElements()) {
                        if (elem_ptr2->getType() != "Wire" && elem_ptr2->getType() != "Ground") {
                            if (elem_ptr2->getNode1Id() == other_terminal || elem_ptr2->getNode2Id() == other_terminal) {
                                has_valid_path = true;
                                break;
                            }
                        }
                    }
                    
                    if (has_valid_path) {
                        LOG_INFO("INFO: Voltage source " + elem_ptr->getName() + " has one terminal connected to ground (NORMAL)");
                        LOG_INFO("INFO: Other terminal connects to circuit elements - voltage source is USABLE");
                    } else {
                        LOG_INFO("WARNING: Voltage source " + elem_ptr->getName() + " has no valid path to circuit elements");
                        is_shorted = true;
                    }
                }
            }
            
            if (!is_shorted) {
                found_usable_voltage_source = true;
            }
            
            string vs_info = elem_ptr->getName() + " (" + type + ")";
            if (is_shorted) {
                vs_info += " [SHORTED - NOT USABLE]";
            }
            
            if (type == "SinusoidalVoltageSource") {
                auto* sin_src = dynamic_cast<SinusoidalVoltageSource*>(elem_ptr.get());
                if (sin_src) {
                    double v0 = sin_src->getVoltageAtTime(0.0);
                    vs_info += ": A=" + std::to_string(sin_src->getAmplitude()) + "V, f=" + std::to_string(sin_src->getFrequency())
                             + "Hz, DC=" + std::to_string(sin_src->getDCOffset()) + "V";
                    vs_info += " → V(t=0)=" + std::to_string(v0) + "V";
                }
            } else if (type == "IndependentVoltageSource") {
                double v = elem_ptr->getValue();
                vs_info += ": DC=" + std::to_string(v) + "V";
            } else if (type == "PulseVoltageSource") {
                auto* pulse_src = dynamic_cast<PulseVoltageSource*>(elem_ptr.get());
                if (pulse_src) {
                    double v0 = pulse_src->getVoltageAtTime(0.0);
                    vs_info += ": Pulse source → V(t=0)=" + std::to_string(v0) + "V";
                }
            }
            LOG_INFO(vs_info);
        }
    }

    LOG_INFO("Voltage source search complete. found_voltage_source = " + string(found_voltage_source ? "true" : "false"));
    LOG_INFO("Usable voltage sources = " + string(found_usable_voltage_source ? "true" : "false"));
    
    if (!found_voltage_source) {
        LOG_INFO("CRITICAL ERROR: No voltage sources found in circuit!");
        LOG_INFO("Cannot perform transient analysis");
        LOG_INFO("Please add at least one voltage source");
        LOG_INFO("Returning early due to no voltage sources found");
        return out;
    }
    
    if (!found_usable_voltage_source) {
        LOG_INFO("CRITICAL ERROR: All voltage sources are shorted!");
        LOG_INFO("Cannot perform transient analysis");
        LOG_INFO("Please fix the circuit by removing shorting wires");
        LOG_INFO("Returning early due to all voltage sources being shorted");
        return out;
    }

    LOG_INFO("Usable voltage sources found, continuing with analysis");

    // Simulation setup
    LOG_INFO("SIMULATION SETUP:");
    const int steps = static_cast<int>(t_total / t_step);
    const int totalSamples = steps + 1;
    LOG_INFO("Total simulation time: " + std::to_string(t_total) + " seconds");
    LOG_INFO("Time step size: " + std::to_string(t_step) + " seconds");
    LOG_INFO("Number of steps: " + std::to_string(steps));
    LOG_INFO("Total samples: " + std::to_string(totalSamples) + " (including t=0)");
    LOG_INFO("Sample rate: " + std::to_string(1.0 / t_step) + " Hz");

    out.t.reserve(totalSamples);
    out.ys.assign(desiredSignals.size(), vector<double>{});
    for (auto& y : out.ys) y.reserve(totalSamples);
    out.names = desiredSignals;

    LOG_INFO("STARTING SIMULATION:");
    LOG_INFO("Progress updates: Every 500 steps or major milestones");
    LOG_INFO("Detailed logging: First few steps and final step");
    LOG_INFO("--------------------------------------------------------------------------------");

    int executed_steps = 0;
    int solver_failures = 0;
    int detailed_steps = 0; // Count how many steps get detailed logging

    for (int step = 0; step <= steps; ++step) {
        double t = step * t_step;
        out.t.push_back(t);

        // Progress indicator
        bool show_progress = (step == 0 || step == steps || step % 500 == 0);
        bool show_detailed = (step <= 5 || step == steps); // First 5 steps + last step

        if (show_progress) {
            double progress_percent = (static_cast<double>(step) / steps) * 100.0;
            LOG_INFO("STEP " + std::to_string(step) + "/" + std::to_string(steps) +
                     " (t=" + std::to_string(t) + "s, " + std::to_string(progress_percent) + "%)");
        }

        if (show_detailed) {
            detailed_steps++;
            LOG_INFO("Time point details: Current time: " + std::to_string(t) + " seconds, Step: " + std::to_string(step) +
                     ", Progress: " + std::to_string((static_cast<double>(step) / steps * 100.0)) + "%");
        }

        executed_steps++;

        // Build MNA system for this time step
        MNASystem sys(circuit);
        sys.reset();

        if (show_detailed) {
            LOG_INFO("Building MNA system: " + std::to_string(sys.getSystemSize()) + "x" + std::to_string(sys.getSystemSize()) +
                     " (" + std::to_string(sys.getNumNodes()) + " nodes, " + std::to_string(sys.getNumVoltageSources()) + " VS)");
        } else if (step == 0) {
            LOG_INFO("MNA system: " + std::to_string(sys.getSystemSize()) + "x" + std::to_string(sys.getSystemSize()) +
                     " (" + std::to_string(sys.getNumNodes()) + " nodes, " + std::to_string(sys.getNumVoltageSources()) + " VS)");
        }

        // Stamp circuit elements into MNA system
        int stamped_elements = 0;
        if (show_detailed) {
            LOG_INFO("Processing " + std::to_string(circuit.getElements().size()) + " circuit elements for stamping");
        }
        for (const auto& elem_ptr : circuit.getElements()) {
            string ty = elem_ptr->getType();
            string elem_name = elem_ptr->getName();
            string n1 = elem_ptr->getNode1Id();
            string n2 = elem_ptr->getNode2Id();
            
            if (show_detailed) {
                LOG_INFO("Processing element: " + elem_name + " (" + ty + ") " + n1 + " -> " + n2);
            }

            if (ty == "Resistor") {
                sys.stampResistor(n1, n2, elem_ptr->getValue());
                if (show_detailed) {
                    LOG_INFO(elem_name + " (R): G=" + std::to_string(1.0/elem_ptr->getValue()) + "S between " + n1 + "<->" + n2);
                }
                stamped_elements++;

            } else if (ty == "Capacitor") {
                // Backward Euler: conductance C/dt + current source C/dt * V_prev
                sys.stampCapacitor(n1, n2, elem_ptr->getValue(), t_step);
                double Ieq = transSim.getCapCurrent(elem_name, elem_ptr->getValue(), t_step);
                sys.stampCurrentSource(n2, n1, Ieq); // Note direction

                if (show_detailed) {
                    auto& capVoltages = transSim.getCapVoltages();
                    double v_prev = capVoltages.count(elem_name) ? capVoltages.at(elem_name) : 0.0;
                    double conductance = elem_ptr->getValue() / t_step;
                    LOG_INFO(elem_name + " (C): G=" + std::to_string(conductance) + "S, I_eq=" + std::to_string(Ieq) + "A (V_prev=" + std::to_string(v_prev) + "V)");
                }
                stamped_elements++;

            } else if (ty == "Inductor") {
                // Backward Euler: conductance dt/L + current source I_prev
                sys.stampInductor(n1, n2, elem_ptr->getValue(), t_step);
                double Ieq = transSim.getIndCurrent(elem_name);
                sys.stampCurrentSource(n1, n2, Ieq); // Note direction

                if (show_detailed) {
                    double conductance = t_step / elem_ptr->getValue();
                    LOG_INFO(elem_name + " (L): G=" + std::to_string(conductance) + "S, I_eq=" + std::to_string(Ieq) + "A");
                }
                stamped_elements++;
            } else if (ty == "IndependentVoltageSource" || ty == "SinusoidalVoltageSource" || ty == "ACVoltageSource" || ty == "PulseVoltageSource") {
                // Calculate voltage at current time step
                double v = 0.0;
                string source_type = "Unknown";

                if (ty == "PulseVoltageSource") {
                    auto* pulse_src = dynamic_cast<const PulseVoltageSource*>(elem_ptr.get());
                    v = pulse_src->getVoltageAtTime(t);
                    source_type = "Pulse";
                } else if (ty == "SinusoidalVoltageSource") {
                    auto* sin_src = dynamic_cast<const SinusoidalVoltageSource*>(elem_ptr.get());
                    v = sin_src->getVoltageAtTime(t);
                    source_type = "Sinusoidal";
                    if (show_detailed) {
                        cout << "   │  🌊 " << elem_name << " (" << source_type << "): V(t)=" << fixed << setprecision(6) << v
                             << "V [A=" << sin_src->getAmplitude() << "V, f=" << sin_src->getFrequency()
                             << "Hz, DC=" << sin_src->getDCOffset() << "V]" << endl;
                    }
                } else if (ty == "IndependentVoltageSource") {
                    v = elem_ptr->getValue();
                    source_type = "DC";
                    if (show_detailed) {
                        cout << "   │  🔋 " << elem_name << " (" << source_type << "): V=" << fixed << setprecision(6) << v << "V" << endl;
                    }
                    if (abs(v) < 1e-12) {
                        cout << "   │  ⚠️  WARNING: " << elem_name << " has zero voltage!" << endl;
                    }
                } else if (ty == "ACVoltageSource") {
                    v = elem_ptr->getValue();
                    source_type = "AC";
                    if (show_detailed) {
                        cout << "   │  📡 " << elem_name << " (" << source_type << "): V=" << fixed << setprecision(6) << v << "V" << endl;
                    }
                }

                // Check if voltage source is in MNA system
                if (show_detailed) {
                    LOG_INFO("Processing voltage source: " + elem_name + " (n1=" + n1 + ", n2=" + n2 + ", v=" + std::to_string(v) + "V)");
                    LOG_INFO("MNA system has " + std::to_string(sys.getNumVoltageSources()) + " voltage sources");
                }

                // Stamp voltage source into MNA system
                if (!show_detailed && step == 0) {
                    cout << "   │  🔌 " << elem_name << " (" << source_type << "): V=" << fixed << setprecision(3) << v << "V" << endl;
                }

                sys.stampVoltageSource(elem_name, n1, n2, v);
                stamped_elements++;

            } else if (ty == "IndependentCurrentSource") {
                sys.stampCurrentSource(n1, n2, elem_ptr->getValue());
                if (show_detailed) {
                    cout << "   │  💡 " << elem_name << " (I): I=" << fixed << setprecision(6) << elem_ptr->getValue() << "A" << endl;
                }
                stamped_elements++;
            } else if (ty == "Wire") {
                // Wire elements create a short circuit between two nodes
                // This is equivalent to a very small resistance (ideal wire)
                // For numerical stability, use a very small resistance instead of zero
                double wire_resistance = 1e-12; // 1 pico-ohm (essentially zero)
                sys.stampResistor(n1, n2, wire_resistance);
                
                if (show_detailed) {
                    LOG_INFO(elem_name + " (Wire): R=" + std::to_string(wire_resistance) + "Ω between " + n1 + "<->" + n2);
                }
                stamped_elements++;
            }
        }

        // Stamping completion summary
        if (show_detailed) {
            LOG_INFO("Stamping completed: " + std::to_string(stamped_elements) + "/" + std::to_string(circuit.getElements().size()) + " elements stamped");
            LOG_INFO("MNA system ready for solving");
        }

        // Solve the MNA system
        bool solver_success = sys.solveGaussian();

        if (!solver_success) {
            solver_failures++;
            if (show_detailed || show_progress) {
                LOG_INFO("SOLVER FAILED at t=" + std::to_string(t) + "s - Possible causes: singular matrix, numerical instability");
                LOG_INFO("Total solver failures so far: " + std::to_string(solver_failures));
            }

            // Debug matrix for failed solves on detailed steps
            if (show_detailed && step <= 2) {
                LOG_INFO("Matrix diagnostics:");
                auto& mat = sys.getMatrix();
                auto& rhs = sys.getRHS();
                for (int i = 0; i < min(5, sys.getSystemSize()); i++) { // Show first 5 rows
                    double row_sum = 0.0;
                    for (int j = 0; j < sys.getSystemSize(); j++) {
                        row_sum += abs(mat[i][j]);
                    }
                    LOG_INFO("Row " + std::to_string(i) + ": sum=" + std::to_string(row_sum) + ", RHS=" + std::to_string(rhs[i]));
                }
            }

            // Skip state updates and signal sampling for failed solves
            if (show_detailed) {
                LOG_INFO("Skipping state updates and signal sampling due to solver failure");
            }
                    } else {
            if (show_detailed) {
                LOG_INFO("SOLVER SUCCESS at t=" + std::to_string(t) + "s");
            } else if (show_progress && step == 0) {
                LOG_INFO("Solver: SUCCESS");
            }

            // Show solution results for detailed steps
            if (show_detailed) {
                LOG_INFO("Solution results:");

                // Node voltages
                LOG_INFO("Node voltages:");
                bool has_non_ground_nodes = false;
                for (const auto& pair : circuit.getNodes()) {
                    if (pair.first != circuit.getGroundNodeId()) {
                        double v = sys.getNodeVoltage(pair.first);
                        LOG_INFO("V(" + pair.first + ") = " + std::to_string(v) + "V");
                        has_non_ground_nodes = true;
                    }
                }
                if (!has_non_ground_nodes) {
                    LOG_INFO("No non-ground nodes in circuit");
                }

                // Voltage source currents
                if (sys.getNumVoltageSources() > 0) {
                    LOG_INFO("Voltage source currents:");
                    for (const auto& elem_ptr : circuit.getElements()) {
                        string ty = elem_ptr->getType();
                        if (ty == "IndependentVoltageSource" || ty == "SinusoidalVoltageSource" ||
                            ty == "ACVoltageSource" || ty == "PulseVoltageSource") {
                            double i = sys.getCurrentThroughVS(elem_ptr->getName());
                            LOG_INFO("I(" + elem_ptr->getName() + ") = " + std::to_string(i) + "A");
                        }
                    }
                }
            }

            // Update transient states only if solver succeeded
            // NOTE: We update AFTER sampling to ensure we use the current solution
            // for the next time step's equivalent current sources
        }

        // Sample requested signals using current solution
        for (size_t i = 0; i < desiredSignals.size(); ++i) {
            const string& signal = desiredSignals[i];
            double val = 0.0;
            bool signal_found = false;

            if (signal.substr(0, 2) == "V(") {
                // Node voltage measurement
                string node = signal.substr(2, signal.size() - 3);
                val = sys.getNodeVoltage(node);
                signal_found = true;

                if (show_detailed) {
                    LOG_INFO(signal + " = " + std::to_string(val) + "V (node " + node + ")");
                }

            } else if (signal.substr(0, 2) == "I(") {
                // Element current measurement
                string elemName = signal.substr(2, signal.size() - 3);
                for (const auto& elem_ptr : circuit.getElements()) {
                    if (elem_ptr->getName() == elemName) {
                        vector<double> currents = getElementCurrent(elem_ptr.get(), sys, t_step);
                        val = currents[0];
                        signal_found = true;

                        if (show_detailed) {
                            LOG_INFO(signal + " = " + std::to_string(val) + "A (through " + elemName + ")");
                        }
                        break;
                    }
                }
            }

            if (!signal_found) {
                if (show_detailed) {
                    LOG_INFO("WARNING: " + signal + " - unknown signal format");
                }
                val = 0.0; // Default value for unknown signals
            }

            out.ys[i].push_back(val);
        }

        if (show_detailed) {
            LOG_INFO("Step completed: Time=" + std::to_string(t) + "s, Signals sampled=" + std::to_string(desiredSignals.size()));
        }

    }

    // Analysis completion summary with enhanced logging
    LOG_INFO("================================================================================ TRANSIENT ANALYSIS COMPLETED ================================================================================");

    // Performance summary
    LOG_INFO("PERFORMANCE SUMMARY:");
    LOG_INFO("Total steps executed: " + std::to_string(executed_steps));
    LOG_INFO("Expected steps: " + std::to_string(steps + 1));
    if (!out.t.empty()) {
        LOG_INFO("Time range: " + std::to_string(out.t[0]) + "s -> " + std::to_string(out.t.back()) + "s");
    } else {
        LOG_INFO("Time range: EMPTY");
    }
    LOG_INFO("Sample rate: " + std::to_string(1.0 / t_step) + " Hz");

    // Solver statistics
    double success_rate = (executed_steps > 0) ? ((executed_steps - solver_failures) * 100.0 / executed_steps) : 0.0;
    LOG_INFO("SOLVER STATISTICS:");
    LOG_INFO("Successful solves: " + std::to_string(executed_steps - solver_failures));
    LOG_INFO("Failed solves: " + std::to_string(solver_failures));
    LOG_INFO("Success rate: " + std::to_string(success_rate) + "%");

    if (solver_failures > 0) {
        LOG_INFO(std::to_string(solver_failures) + " solver failures detected - Possible causes: numerical instability, singular matrices");
    } else {
        LOG_INFO("All solves successful");
    }

    // Signal data summary
    LOG_INFO("SIGNAL DATA SUMMARY:");
    LOG_INFO("Signals analyzed: " + std::to_string(out.names.size()));
    LOG_INFO("Data points per signal: " + std::to_string(out.t.empty() ? 0 : out.t.size()));

    for (size_t i = 0; i < out.names.size(); ++i) {
        LOG_INFO(out.names[i] + ":");
        if (!out.ys[i].empty()) {
            LOG_INFO("Samples: " + std::to_string(out.ys[i].size()));

            // Determine units based on signal type
            std::string unit = "";
            if (out.names[i].substr(0, 2) == "V(") {
                unit = "V";
            } else if (out.names[i].substr(0, 2) == "I(") {
                unit = "A";
            }

            LOG_INFO("First: " + std::to_string(out.ys[i][0]) + unit);
            LOG_INFO("Last: " + std::to_string(out.ys[i].back()) + unit);

            // Check for zero signals
            bool all_zero = true;
            double max_val = 0.0;
            double min_val = 0.0;

            for (size_t j = 0; j < out.ys[i].size(); ++j) {
                double val = out.ys[i][j];
                if (abs(val) > 1e-12) {
                    all_zero = false;
                }
                if (j == 0 || val > max_val) max_val = val;
                if (j == 0 || val < min_val) min_val = val;
            }

            if (all_zero) {
                LOG_INFO("WARNING: All values are zero! Check circuit connectivity and sources");
            } else {
                LOG_INFO("Range: " + std::to_string(min_val) + " -> " + std::to_string(max_val) + unit);
            }
        } else {
            LOG_INFO("No data collected");
        }
    }

    LOG_INFO("ANALYSIS COMPLETE - Ready for plotting and further analysis");
    LOG_INFO("================================================================================================================================================");

    // Build SignalView list (with colors) - like other.cpp
    out.series.clear();
    out.series.reserve(out.ys.size());
    for (size_t i = 0; i < out.ys.size(); ++i) {
        out.series.push_back(SignalView{ &out.t, &out.ys[i], kPalette[i % kPalette.size()] });
    }

    return out;
}

ACSweepVals CircuitAnalyzer::ACsweep(Circuit& circuit, double startOmega, double stopOmega, int stepCount, string desiredSignal) {
    ACSweepVals out;
    
    cout << "[AC SWEEP] Starting AC sweep analysis" << endl;
    cout << "[AC SWEEP] Parameters: startOmega=" << startOmega << ", stopOmega=" << stopOmega << ", stepCount=" << stepCount << endl;
    cout << "[AC SWEEP] Desired signal: " << desiredSignal << endl;

    // Find AC sources
    string sName;
    int sCount = 0;
    for (const auto& elem_ptr : circuit.getElements()) {
        string type = elem_ptr->getType();
        cout << "[AC SWEEP] Found element: " << elem_ptr->getName() << " type: " << type << endl;
        if (type == "ACVoltageSource" || type == "SinusoidalVoltageSource" || type == "IndependentVoltageSource") {
            sName = elem_ptr->getName();
            sCount++;
            cout << "[AC SWEEP] AC source found: " << sName << endl;
        }
    }
    if (sCount != 1) {
        cerr << "Error: AC sweep requires exactly one AC source, found " << sCount << endl;
        return out;
    }
    
    cout << "[AC SWEEP] Using AC source: " << sName << endl;

    out.names.push_back(desiredSignal);
    out.ys.assign(1, vector<double>{});
    out.freq.reserve(stepCount);
    out.ys[0].reserve(stepCount);

    for (int step = 0; step < stepCount; ++step) {
        double omega = startOmega + (stopOmega - startOmega) * step / (stepCount - 1);
        double freq_hz = omega / (2.0 * M_PI);  // Convert to Hz for display
        out.freq.push_back(freq_hz);  // Store frequency in Hz, not angular frequency
        
        if (step % 10 == 0) {  // Debug every 10th step
            cout << "[AC SWEEP] Step " << step << "/" << stepCount << ": omega=" << omega << " rad/s, freq=" << freq_hz << " Hz" << endl;
        }

        ComplexMNASystem sys(circuit);
        sys.reset();

        for (const auto& elem_ptr : circuit.getElements()) {
            string ty = elem_ptr->getType();
            if (ty == "Resistor") {
                sys.stampImpedance(elem_ptr->getNode1Id(), elem_ptr->getNode2Id(), cd(elem_ptr->getValue(), 0.0));
            } else if (ty == "Capacitor") {
                cd z = cd(0.0, -1.0 / (omega * elem_ptr->getValue()));
                sys.stampImpedance(elem_ptr->getNode1Id(), elem_ptr->getNode2Id(), z);
            } else if (ty == "Inductor") {
                cd z = cd(0.0, omega * elem_ptr->getValue());
                sys.stampImpedance(elem_ptr->getNode1Id(), elem_ptr->getNode2Id(), z);
            } else if (ty == "ACVoltageSource" || ty == "SinusoidalVoltageSource" || ty == "IndependentVoltageSource") {
                sys.stampVoltageSource(elem_ptr->getName(), elem_ptr->getNode1Id(), elem_ptr->getNode2Id(), cd(1.0, 0.0));
            }
        }

        if (!sys.solveGaussian()) {
            cerr << "Error: solver failed at omega=" << omega << " (freq=" << freq_hz << " Hz)" << endl;
            out.ys[0].push_back(0.0);
            continue;
        }

        double mag = 0.0;
        if (desiredSignal.substr(0, 2) == "V(") {
            string node = desiredSignal.substr(2, desiredSignal.size() - 3);
            cd v = sys.getNodeVoltageComplex(node);
            mag = abs(v);
            if (step % 10 == 0) {
                cout << "[AC SWEEP] Node " << node << " voltage: " << v << " (mag=" << mag << ")" << endl;
            }
        } else if (desiredSignal.substr(0, 2) == "I(") {
            string elemName = desiredSignal.substr(2, desiredSignal.size() - 3);
            for (const auto& elem_ptr : circuit.getElements()) {
                if (elem_ptr->getName() == elemName) {
                    cd v1 = sys.getNodeVoltageComplex(elem_ptr->getNode1Id());
                    cd v2 = sys.getNodeVoltageComplex(elem_ptr->getNode2Id());
                    cd z;
                    if (elem_ptr->getType() == "Resistor") {
                        z = cd(elem_ptr->getValue(), 0.0);
                    } else if (elem_ptr->getType() == "Capacitor") {
                        z = cd(0.0, -1.0 / (omega * elem_ptr->getValue()));
                    } else if (elem_ptr->getType() == "Inductor") {
                        z = cd(0.0, omega * elem_ptr->getValue());
                    }
                    if (abs(z) > 0.0) mag = abs((v1 - v2) / z);
                    break;
                }
            }
        }
        out.ys[0].push_back(mag);
    }
    
    cout << "[AC SWEEP] Analysis complete. Generated " << out.freq.size() << " frequency points and " << out.ys[0].size() << " magnitude values" << endl;
    if (!out.freq.empty()) {
        cout << "[AC SWEEP] Frequency range: " << out.freq[0] << " Hz to " << out.freq.back() << " Hz" << endl;
    }
    if (!out.ys[0].empty()) {
        cout << "[AC SWEEP] Magnitude range: " << *min_element(out.ys[0].begin(), out.ys[0].end()) << " to " << *max_element(out.ys[0].begin(), out.ys[0].end()) << endl;
    }

    return out;
}

// Test function to verify enhanced logging is working
void testEnhancedLogging() {
    LOG_INFO("================================================================================ ENHANCED LOGGING TEST FUNCTION CALLED ================================================================================");
    LOG_INFO("Enhanced logging system is active and working!");
    LOG_INFO("This confirms the SimpleAnalysis enhanced logging is compiled.");
    LOG_INFO("If you see this message, the enhanced logging is ready to use.");
    LOG_INFO("================================================================================================================================================");
}

// Helper: get element current using Norton equivalent (like other.cpp)
double computeStampedCurrentNorton(const Element* elem, MNASystem& sys, double t_step) {
    string ty = elem->getType();

    // Node voltages at element terminals
    double v1 = sys.getNodeVoltage(elem->getNode1Id());
    double v2 = sys.getNodeVoltage(elem->getNode2Id());
    double dv = v1 - v2;

    // Ohm's law with stamped conductance G (Norton equivalent), current defined from n1 -> n2
    // - Resistor:      G = 1/R
    // - Capacitor (BE):G =  C / dt
    // - Inductor (BE): G = dt / L
    // This returns the branch (conductive) current portion.
    if (ty == "Resistor") {
        if (elem->getValue() != 0.0) return dv / elem->getValue();
        return 0.0;
    } else if (ty == "Capacitor") {
        if (t_step > 0.0) return (elem->getValue() / t_step) * dv;
        return 0.0;
    } else if (ty == "Inductor") {
        if (elem->getValue() != 0.0) return (t_step / elem->getValue()) * dv;
        return 0.0;
    } else if (ty == "IndependentVoltageSource" || ty == "SinusoidalVoltageSource" ||
               ty == "ACVoltageSource" || ty == "PulseVoltageSource") {
        return sys.getCurrentThroughVS(elem->getName());
    }

    // For other types, return 0
    return 0.0;
}

vector<double> CircuitAnalyzer::getElementCurrent(const Element* elem, MNASystem& sys, double t_step) {
    vector<double> currents;
    double current = computeStampedCurrentNorton(elem, sys, t_step);
    currents.push_back(current);
    return currents;
}














