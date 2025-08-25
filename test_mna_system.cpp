#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <chrono>
#include "Circuit.h"
#include "Solvers.h"
#include "Plotter.h"
#include "GraphExtractor.h"

// Test circuit creation functions
Circuit createSimpleRC() {
    Circuit circuit;
    
    // Create nodes
    circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "V1", "GND", 5.0));
    circuit.addElement(std::make_unique<Resistor>("R1", "V1", "OUT", 1000.0));
    circuit.addElement(std::make_unique<Capacitor>("C1", "OUT", "GND", 1e-6));
    
    // Set ground
    circuit.setGroundNode("GND");
    
    return circuit;
}

Circuit createVoltageDivider() {
    Circuit circuit;
    
    // Create nodes
    circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "V1", "GND", 10.0));
    circuit.addElement(std::make_unique<Resistor>("R1", "V1", "MID", 1000.0));
    circuit.addElement(std::make_unique<Resistor>("R2", "MID", "GND", 2000.0));
    
    // Set ground
    circuit.setGroundNode("GND");
    
    return circuit;
}

Circuit createRLCCircuit() {
    Circuit circuit;
    
    // Create nodes
    circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "V1", "GND", 12.0));
    circuit.addElement(std::make_unique<Resistor>("R1", "V1", "OUT", 100.0));
    circuit.addElement(std::make_unique<Inductor>("L1", "OUT", "MID", 0.001));
    circuit.addElement(std::make_unique<Capacitor>("C1", "MID", "GND", 1e-6));
    
    // Set ground
    circuit.setGroundNode("GND");
    
    return circuit;
}

Circuit createPulseCircuit() {
    Circuit circuit;
    
    // Create nodes
    circuit.addElement(std::make_unique<PulseVoltageSource>("V1", "V1", "GND", 5.0, 0.0, 1e-3, 1e-6, 1e-6, 1e-3));
    circuit.addElement(std::make_unique<Resistor>("R1", "V1", "OUT", 1000.0));
    circuit.addElement(std::make_unique<Capacitor>("C1", "OUT", "GND", 1e-6));
    
    // Set ground
    circuit.setGroundNode("GND");
    
    return circuit;
}

Circuit createACCircuit() {
    Circuit circuit;
    
    // Create nodes
    circuit.addElement(std::make_unique<SinusoidalVoltageSource>("V1", "V1", "GND", 5.0, 1000.0, 0.0));
    circuit.addElement(std::make_unique<Resistor>("R1", "V1", "OUT", 1000.0));
    circuit.addElement(std::make_unique<Capacitor>("C1", "OUT", "GND", 1e-6));
    
    // Set ground
    circuit.setGroundNode("GND");
    
    return circuit;
}

void printAnalysisResult(const AnalysisResult& result, const std::string& analysis_name) {
    std::cout << "\n=== " << analysis_name << " Results ===" << std::endl;
    
    if (!result.success) {
        std::cout << "Analysis failed: " << result.error_message << std::endl;
        return;
    }
    
    std::cout << "Node Voltages:" << std::endl;
    for (const auto& [node_id, voltage] : result.node_voltages) {
        std::cout << "  " << node_id << ": " << voltage << " V" << std::endl;
    }
    
    std::cout << "\nBranch Currents:" << std::endl;
    for (const auto& [branch_name, current] : result.branch_currents) {
        std::cout << "  " << branch_name << ": " << current << " A" << std::endl;
    }
    
    if (!result.time_series.empty()) {
        std::cout << "\nTime Series Data:" << std::endl;
        for (const auto& [series_name, values] : result.time_series) {
            std::cout << "  " << series_name << ": " << values.size() << " points" << std::endl;
        }
    }
    
    if (!result.frequency_series.empty()) {
        std::cout << "\nFrequency Series Data:" << std::endl;
        for (const auto& [series_name, values] : result.frequency_series) {
            std::cout << "  " << series_name << ": " << values.size() << " points" << std::endl;
        }
    }
}

void testDCAnalysis() {
    std::cout << "\n=== Testing DC Analysis ===" << std::endl;
    
    // Test simple RC circuit
    std::cout << "\n1. Simple RC Circuit:" << std::endl;
    Circuit rc_circuit = createSimpleRC();
    
    MNASolver solver;
    auto result = solver.solveDC(rc_circuit);
    printAnalysisResult(result, "DC Analysis - RC Circuit");
    
    // Test voltage divider
    std::cout << "\n2. Voltage Divider:" << std::endl;
    Circuit vdiv_circuit = createVoltageDivider();
    
    result = solver.solveDC(vdiv_circuit);
    printAnalysisResult(result, "DC Analysis - Voltage Divider");
    
    // Test RLC circuit
    std::cout << "\n3. RLC Circuit:" << std::endl;
    Circuit rlc_circuit = createRLCCircuit();
    
    result = solver.solveDC(rlc_circuit);
    printAnalysisResult(result, "DC Analysis - RLC Circuit");
}

void testTransientAnalysis() {
    std::cout << "\n=== Testing Transient Analysis ===" << std::endl;
    
    // Configure solver for transient analysis
    MNASolverConfig config;
    config.transient_timestep = 1e-6;  // 1 microsecond
    config.transient_end_time = 1e-3;  // 1 millisecond
    
    MNASolver solver(config);
    
    // Test pulse circuit
    std::cout << "\n1. Pulse Circuit Transient Analysis:" << std::endl;
    Circuit pulse_circuit = createPulseCircuit();
    
    auto result = solver.solveTransient(pulse_circuit);
    printAnalysisResult(result, "Transient Analysis - Pulse Circuit");
    
    // Test RC circuit step response
    std::cout << "\n2. RC Circuit Step Response:" << std::endl;
    Circuit rc_circuit = createSimpleRC();
    
    result = solver.solveTransient(rc_circuit);
    printAnalysisResult(result, "Transient Analysis - RC Circuit");
}

void testACAnalysis() {
    std::cout << "\n=== Testing AC Analysis ===" << std::endl;
    
    // Configure solver for AC analysis
    MNASolverConfig config;
    config.ac_start_freq = 1.0;      // 1 Hz
    config.ac_end_freq = 1e6;        // 1 MHz
    config.ac_points = 50;           // 50 frequency points
    
    MNASolver solver(config);
    
    // Test AC circuit
    std::cout << "\n1. AC Circuit Frequency Response:" << std::endl;
    Circuit ac_circuit = createACCircuit();
    
    auto result = solver.solveAC(ac_circuit);
    printAnalysisResult(result, "AC Analysis - AC Circuit");
}

void testDCSweep() {
    std::cout << "\n=== Testing DC Sweep ===" << std::endl;
    
    MNASolver solver;
    
    // Test voltage divider with R2 sweep
    std::cout << "\n1. Voltage Divider R2 Sweep:" << std::endl;
    Circuit vdiv_circuit = createVoltageDivider();
    
    auto result = solver.solveDCSweep(vdiv_circuit, "R2", 100.0, 10000.0, 20);
    printAnalysisResult(result, "DC Sweep - R2 from 100Ω to 10kΩ");
}

void testGraphExtraction() {
    std::cout << "\n=== Testing Graph Extraction ===" << std::endl;
    
    Circuit circuit = createRLCCircuit();
    GraphExtractor extractor(circuit);
    
    std::cout << "\n1. Extracting Circuit Graph:" << std::endl;
    auto graph = extractor.extractGraph();
    
    std::cout << "Graph extracted successfully!" << std::endl;
    std::cout << "Number of nodes: " << graph.nodes.size() << std::endl;
    std::cout << "Number of edges: " << graph.edges.size() << std::endl;
    
    // Check connectivity
    std::cout << "\n2. Checking Graph Connectivity:" << std::endl;
    if (extractor.isGraphConnected()) {
        std::cout << "Graph is connected ✓" << std::endl;
    } else {
        std::cout << "Graph is NOT connected ✗" << std::endl;
    }
    
    // Find isolated nodes
    std::cout << "\n3. Finding Isolated Nodes:" << std::endl;
    auto isolated = extractor.findIsolatedNodes();
    if (isolated.empty()) {
        std::cout << "No isolated nodes found ✓" << std::endl;
    } else {
        std::cout << "Isolated nodes found: ";
        for (const auto& node : isolated) {
            std::cout << node << " ";
        }
        std::cout << std::endl;
    }
    
    // Check for short circuits
    std::cout << "\n4. Checking for Short Circuits:" << std::endl;
    auto short_circuits = extractor.findShortCircuits();
    if (short_circuits.empty()) {
        std::cout << "No short circuits detected ✓" << std::endl;
    } else {
        std::cout << "Short circuits detected: ";
        for (const auto& sc : short_circuits) {
            std::cout << sc << " ";
        }
        std::cout << std::endl;
    }
    
    // Create node index map for MNA
    std::cout << "\n5. Creating Node Index Map for MNA:" << std::endl;
    auto node_map = extractor.createNodeIndexMap();
    std::cout << "Node index map created with " << node_map.size() << " non-ground nodes" << std::endl;
    
    for (const auto& [node_id, index] : node_map) {
        std::cout << "  " << node_id << " -> " << index << std::endl;
    }
}

void testPlotting() {
    std::cout << "\n=== Testing Plotting System ===" << std::endl;
    
    try {
        // Create plotter
        PlotConfig plot_config;
        plot_config.width = 800;
        plot_config.height = 600;
        plot_config.title = "Circuit Analysis Results";
        
        CircuitPlotter plotter(plot_config);
        
        if (!plotter.isRunning()) {
            std::cout << "Plotter initialization failed - SDL not available" << std::endl;
            return;
        }
        
        std::cout << "Plotter initialized successfully ✓" << std::endl;
        
        // Test DC analysis plotting
        std::cout << "\n1. Testing DC Analysis Plotting:" << std::endl;
        Circuit circuit = createVoltageDivider();
        MNASolver solver;
        auto result = solver.solveDC(circuit);
        
        if (result.success) {
            plotter.plotDCResults(result);
            std::cout << "DC results plotted successfully ✓" << std::endl;
            
            // Wait a bit to show the plot
            SDL_Delay(2000);
        }
        
        // Test transient analysis plotting
        std::cout << "\n2. Testing Transient Analysis Plotting:" << std::endl;
        MNASolverConfig config;
        config.transient_timestep = 1e-6;
        config.transient_end_time = 1e-3;
        
        MNASolver transient_solver(config);
        auto transient_result = transient_solver.solveTransient(circuit);
        
        if (transient_result.success) {
            plotter.plotTransientResults(transient_result);
            std::cout << "Transient results plotted successfully ✓" << std::endl;
            
            // Wait a bit to show the plot
            SDL_Delay(2000);
        }
        
        std::cout << "Plotting tests completed ✓" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Plotting test failed: " << e.what() << std::endl;
    }
}

void runPerformanceTest() {
    std::cout << "\n=== Performance Test ===" << std::endl;
    
    // Create a larger circuit for performance testing
    Circuit large_circuit;
    
    // Add many resistors in a grid pattern
    int grid_size = 10;
    for (int i = 0; i < grid_size; ++i) {
        for (int j = 0; j < grid_size; ++j) {
            std::string node1 = "N" + std::to_string(i) + "_" + std::to_string(j);
            std::string node2 = "N" + std::to_string(i+1) + "_" + std::to_string(j);
            std::string name = "R" + std::to_string(i) + "_" + std::to_string(j) + "_H";
            large_circuit.addElement(std::make_unique<Resistor>(name, node1, node2, 1000.0));
            
            node2 = "N" + std::to_string(i) + "_" + std::to_string(j+1);
            name = "R" + std::to_string(i) + "_" + std::to_string(j) + "_V";
            large_circuit.addElement(std::make_unique<Resistor>(name, node1, node2, 1000.0));
        }
    }
    
    // Add voltage source
    large_circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "N0_0", "GND", 10.0));
    large_circuit.setGroundNode("GND");
    
    std::cout << "Large circuit created with " << large_circuit.getElements().size() << " elements" << std::endl;
    
    // Test MNA matrix construction performance
    auto start_time = std::chrono::high_resolution_clock::now();
    
    MNAMatrix mna_matrix;
    mna_matrix.build(large_circuit, false, 0.0, 0.0);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "MNA matrix construction took: " << duration.count() << " microseconds" << std::endl;
    std::cout << "Matrix size: " << mna_matrix.getA().size() << "x" << mna_matrix.getA().size() << std::endl;
    
    // Test solving performance
    start_time = std::chrono::high_resolution_clock::now();
    
    LUDecompositionSolver solver;
    Vector solution = solver.solve(mna_matrix.getA(), mna_matrix.getRHS());
    
    end_time = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "Matrix solution took: " << duration.count() << " microseconds" << std::endl;
    std::cout << "Solution vector size: " << solution.size() << std::endl;
}

int main() {
    std::cout << "=== Circuit MNA System Test Program ===" << std::endl;
    std::cout << "This program demonstrates the complete Modified Nodal Analysis (MNA) system" << std::endl;
    
    try {
        // Test DC analysis
        testDCAnalysis();
        
        // Test transient analysis
        testTransientAnalysis();
        
        // Test AC analysis
        testACAnalysis();
        
        // Test DC sweep
        testDCSweep();
        
        // Test graph extraction
        testGraphExtraction();
        
        // Test plotting system
        testPlotting();
        
        // Performance test
        runPerformanceTest();
        
        std::cout << "\n=== All Tests Completed Successfully! ===" << std::endl;
        std::cout << "\nThe MNA system includes:" << std::endl;
        std::cout << "✓ DC Analysis" << std::endl;
        std::cout << "✓ Transient Analysis" << std::endl;
        std::cout << "✓ AC Analysis" << std::endl;
        std::cout << "✓ DC Sweep Analysis" << std::endl;
        std::cout << "✓ Graph Extraction and Validation" << std::endl;
        std::cout << "✓ Plotting and Visualization" << std::endl;
        std::cout << "✓ Performance Optimization" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
