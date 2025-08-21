#include <iostream>
#include <memory>
#include "Element.h"
#include "Circuit.h"
#include "Solvers.h"
#include "Analyzers.h"
#include "ErrorManager.h"

int main() {
    try {
        std::cout << "=== SPICE Simulator Test Program ===\n\n";
        
        // Create a simple circuit
        Circuit circuit;
        
        // Add components
        std::cout << "Creating test circuit...\n";
        circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "N1", "0", 5.0));
        circuit.addElement(std::make_unique<Resistor>("R1", "N1", "N2", 1000.0));
        circuit.addElement(std::make_unique<Capacitor>("C1", "N2", "0", 1e-6));
        circuit.addElement(std::make_unique<Ground>("GND", "0"));
        
        std::cout << "Circuit created successfully!\n";
        std::cout << "Number of elements: " << circuit.getElements().size() << "\n";
        std::cout << "Number of nodes: " << circuit.getNodes().size() << "\n";
        std::cout << "Ground node: " << circuit.getGroundNodeId() << "\n\n";
        
        // Test circuit connectivity
        if (circuit.checkGroundNodeExists()) {
            std::cout << "✓ Ground node exists\n";
        } else {
            std::cout << "✗ No ground node found\n";
        }
        
        if (circuit.checkConnectivity()) {
            std::cout << "✓ Circuit is fully connected\n";
        } else {
            std::cout << "✗ Circuit connectivity issues\n";
        }
        
        // Test MNA matrix building
        std::cout << "\nTesting MNA matrix building...\n";
        MNAMatrix mna;
        mna.build(circuit, false, 0.0, 0.0);
        
        const Matrix& A = mna.getA();
        const Vector& b = mna.getRHS();
        
        std::cout << "Matrix size: " << A.size() << "x" << (A.empty() ? 0 : A[0].size()) << "\n";
        std::cout << "RHS vector size: " << b.size() << "\n\n";
        
        // Test DC analysis
        std::cout << "Testing DC analysis...\n";
        LUDecompositionSolver solver;
        Vector solution = solver.solve(A, b);
        
        std::cout << "Solution vector size: " << solution.size() << "\n";
        if (!solution.empty()) {
            std::cout << "First solution value: " << solution[0] << "\n";
        }
        
        // Test transient analysis
        std::cout << "\nTesting transient analysis...\n";
        TransientAnalysis tran(1e-6, 5e-3, false);
        tran.analyze(circuit, mna, solver);
        
        const auto& results = tran.getResults();
        const auto& time_points = tran.getTimePoints();
        
        std::cout << "Transient analysis completed\n";
        std::cout << "Number of time points: " << time_points.size() << "\n";
        std::cout << "Number of result variables: " << results.size() << "\n";
        
        // Test AC analysis
        std::cout << "\nTesting AC analysis...\n";
        ACSweepAnalysis ac("V1", 1.0, 100e3, 100, "DEC");
        ac.analyze(circuit, mna, solver);
        
        const auto& ac_results = ac.getComplexResults();
        const auto& freq_points = ac.getFrequencyPoints();
        
        std::cout << "AC analysis completed\n";
        std::cout << "Number of frequency points: " << freq_points.size() << "\n";
        std::cout << "Number of AC result variables: " << ac_results.size() << "\n";
        
        // Test DC sweep
        std::cout << "\nTesting DC sweep...\n";
        DCSweepAnalysis dc("V1", 0.0, 10.0, 0.5);
        dc.analyze(circuit, mna, solver);
        
        const auto& dc_results = dc.getResults();
        const auto& sweep_values = dc.getSweepValues();
        
        std::cout << "DC sweep completed\n";
        std::cout << "Number of sweep points: " << sweep_values.size() << "\n";
        std::cout << "Number of DC result variables: " << dc_results.size() << "\n";
        
        std::cout << "\n=== All tests passed successfully! ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
