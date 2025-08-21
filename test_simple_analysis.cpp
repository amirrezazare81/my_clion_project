#include <iostream>
#include <memory>
#include "Circuit.h"
#include "Element.h"
#include "SimpleAnalysis.h"

int main() {
    std::cout << "Testing Simple Analysis Implementation" << std::endl;

    // Create a simple circuit: V1 - R1 - C1 - GND
    Circuit circuit;

    // Add elements
    circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "N1", "GND", 5.0));
    circuit.addElement(std::make_unique<Resistor>("R1", "N1", "N2", 1000.0));
    circuit.addElement(std::make_unique<Capacitor>("C1", "N2", "GND", 1e-6));

    // Set ground
    circuit.setGroundNode("GND");

    std::cout << "Circuit created:" << std::endl;
    circuit.listElements();

    // Test DC analysis
    std::cout << "\n=== DC Analysis ===" << std::endl;
    bool dc_success = circuit.analyzeDC();
    if (dc_success) {
        std::cout << "DC analysis successful!" << std::endl;
    } else {
        std::cout << "DC analysis failed!" << std::endl;
    }

    // Test transient analysis
    std::cout << "\n=== Transient Analysis ===" << std::endl;
    std::vector<std::string> signals = {"V(N1)", "V(N2)", "V(GND)", "I(R1)", "I(C1)"};
    TransientSeries tran_result = circuit.analyzeTransientMulti(1e-6, 0.001, signals);

    std::cout << "Transient analysis completed:" << std::endl;
    std::cout << "Time points: " << tran_result.t.size() << std::endl;
    std::cout << "Signals: " << tran_result.names.size() << std::endl;

    // Test AC analysis
    std::cout << "\n=== AC Analysis ===" << std::endl;
    double start_omega = 2 * 3.14159 * 1.0;    // 1 Hz
    double stop_omega = 2 * 3.14159 * 10000.0; // 10 kHz
    int points = 100;

    ACSweepVals ac_result = circuit.ACsweep(start_omega, stop_omega, points, "V(N2)");

    std::cout << "AC analysis completed:" << std::endl;
    std::cout << "Frequency points: " << ac_result.freq.size() << std::endl;
    std::cout << "Signals: " << ac_result.names.size() << std::endl;

    return 0;
}
