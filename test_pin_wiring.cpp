#include <iostream>
#include <memory>
#include "Circuit.h"
#include "Element.h"
#include "GraphExtractor.h"
#include "Pin.h"
#include "Wire.h"

int main() {
    std::cout << "=== Pin-Based Circuit Wiring Test ===" << std::endl;
    
    // Create a circuit
    auto circuit = std::make_unique<Circuit>();
    
    // Add some components
    try {
        // Add a voltage source
        auto v1 = std::make_unique<IndependentVoltageSource>("V1", "n1", "gnd", 5.0);
        circuit->addElement(std::move(v1));
        
        // Add a resistor
        auto r1 = std::make_unique<Resistor>("R1", "n1", "n2", 1000.0);
        circuit->addElement(std::move(r1));
        
        // Add a capacitor
        auto c1 = std::make_unique<Capacitor>("C1", "n2", "gnd", 1e-6);
        circuit->addElement(std::move(c1));
        
        // Add ground
        auto gnd = std::make_unique<Ground>("GND", "gnd");
        circuit->addElement(std::move(gnd));
        
        std::cout << "Circuit created successfully!" << std::endl;
        std::cout << "Components: " << circuit->getElements().size() << std::endl;
        std::cout << "Nodes: " << circuit->getNodes().size() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error creating circuit: " << e.what() << std::endl;
        return 1;
    }
    
    // Test pin creation
    std::cout << "\n=== Testing Pin Creation ===" << std::endl;
    
    // Create pins for the components
    auto pin_v1_pos = std::make_shared<Pin>("V1_pin1", "V1", 1, SDL_Point{100, 100});
    auto pin_v1_neg = std::make_shared<Pin>("V1_pin2", "V1", 2, SDL_Point{100, 200});
    
    auto pin_r1_left = std::make_shared<Pin>("R1_pin1", "R1", 1, SDL_Point{200, 100});
    auto pin_r1_right = std::make_shared<Pin>("R1_pin2", "R1", 2, SDL_Point{300, 100});
    
    auto pin_c1_left = std::make_shared<Pin>("C1_pin1", "C1", 1, SDL_Point{300, 100});
    auto pin_c1_right = std::make_shared<Pin>("C1_pin2", "C1", 2, SDL_Point{300, 200});
    
    // Set node IDs for pins
    pin_v1_pos->setNodeId("n1");
    pin_v1_neg->setNodeId("gnd");
    pin_r1_left->setNodeId("n1");
    pin_r1_right->setNodeId("n2");
    pin_c1_left->setNodeId("n2");
    pin_c1_right->setNodeId("gnd");
    
    std::cout << "Created pins:" << std::endl;
    std::cout << "  " << pin_v1_pos->getFullId() << " -> " << pin_v1_pos->getNodeId() << std::endl;
    std::cout << "  " << pin_v1_neg->getFullId() << " -> " << pin_v1_neg->getNodeId() << std::endl;
    std::cout << "  " << pin_r1_left->getFullId() << " -> " << pin_r1_left->getNodeId() << std::endl;
    std::cout << "  " << pin_r1_right->getFullId() << " -> " << pin_r1_right->getNodeId() << std::endl;
    std::cout << "  " << pin_c1_left->getFullId() << " -> " << pin_c1_left->getNodeId() << std::endl;
    std::cout << "  " << pin_c1_right->getFullId() << " -> " << pin_c1_right->getNodeId() << std::endl;
    
    // Test wire creation
    std::cout << "\n=== Testing Wire Creation ===" << std::endl;
    
    // Create wires between connected pins
    auto wire1 = std::make_shared<Wire>("wire1", pin_v1_pos, pin_r1_left);
    auto wire2 = std::make_shared<Wire>("wire2", pin_r1_right, pin_c1_left);
    auto wire3 = std::make_shared<Wire>("wire3", pin_v1_neg, pin_c1_right);
    
    std::cout << "Created wires:" << std::endl;
    std::cout << "  " << wire1->getDescription() << std::endl;
    std::cout << "  " << wire2->getDescription() << std::endl;
    std::cout << "  " << wire3->getDescription() << std::endl;
    
    // Test pin connection status
    std::cout << "\n=== Testing Pin Connections ===" << std::endl;
    
    pin_v1_pos->updateConnectionStatus();
    pin_v1_neg->updateConnectionStatus();
    pin_r1_left->updateConnectionStatus();
    pin_r1_right->updateConnectionStatus();
    pin_c1_left->updateConnectionStatus();
    pin_c1_right->updateConnectionStatus();
    
    std::cout << "Pin connection status:" << std::endl;
    std::cout << "  " << pin_v1_pos->getFullId() << ": " << (pin_v1_pos->isConnected() ? "Connected" : "Disconnected") << std::endl;
    std::cout << "  " << pin_v1_neg->getFullId() << ": " << (pin_v1_neg->isConnected() ? "Connected" : "Disconnected") << std::endl;
    std::cout << "  " << pin_r1_left->getFullId() << ": " << (pin_r1_left->isConnected() ? "Connected" : "Disconnected") << std::endl;
    std::cout << "  " << pin_r1_right->getFullId() << ": " << (pin_r1_right->isConnected() ? "Connected" : "Disconnected") << std::endl;
    std::cout << "  " << pin_c1_left->getFullId() << ": " << (pin_c1_left->isConnected() ? "Connected" : "Disconnected") << std::endl;
    std::cout << "  " << pin_c1_right->getFullId() << ": " << (pin_c1_right->isConnected() ? "Connected" : "Disconnected") << std::endl;
    
    // Test graph extraction
    std::cout << "\n=== Testing Graph Extraction ===" << std::endl;
    
    GraphExtractor extractor(*circuit);
    
    // Add pins and wires to extractor
    extractor.addPin(pin_v1_pos);
    extractor.addPin(pin_v1_neg);
    extractor.addPin(pin_r1_left);
    extractor.addPin(pin_r1_right);
    extractor.addPin(pin_c1_left);
    extractor.addPin(pin_c1_right);
    
    extractor.addWire(wire1);
    extractor.addWire(wire2);
    extractor.addWire(wire3);
    
    // Extract the circuit graph
    CircuitGraph graph = extractor.extractGraph();
    
    // Print the extracted graph
    extractor.printGraph();
    
    // Test graph analysis
    std::cout << "\n=== Graph Analysis ===" << std::endl;
    
    std::cout << "Graph connected: " << (extractor.isGraphConnected() ? "Yes" : "No") << std::endl;
    
    auto isolated_nodes = extractor.findIsolatedNodes();
    if (!isolated_nodes.empty()) {
        std::cout << "Isolated nodes: ";
        for (const auto& node : isolated_nodes) {
            std::cout << node << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "No isolated nodes found." << std::endl;
    }
    
    auto short_circuits = extractor.findShortCircuits();
    if (!short_circuits.empty()) {
        std::cout << "Short circuits detected: ";
        for (const auto& element : short_circuits) {
            std::cout << element << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "No short circuits detected." << std::endl;
    }
    
    // Test MNA preparation
    std::cout << "\n=== MNA Preparation ===" << std::endl;
    
    auto node_map = extractor.createNodeIndexMap();
    std::cout << "Node index map:" << std::endl;
    for (const auto& [node_id, index] : node_map) {
        std::cout << "  " << node_id << " -> " << index << std::endl;
    }
    
    auto non_ground_nodes = extractor.getNonGroundNodes();
    std::cout << "Non-ground nodes: ";
    for (const auto& node : non_ground_nodes) {
        std::cout << node << " ";
    }
    std::cout << std::endl;
    
    // Export graph to file
    std::cout << "\n=== Exporting Graph ===" << std::endl;
    extractor.exportGraphToFile("test_circuit_graph.txt");
    
    std::cout << "\n=== Test Completed Successfully! ===" << std::endl;
    
    return 0;
}
