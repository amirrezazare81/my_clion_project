#include "GraphExtractor.h"
#include "Element.h"
#include <iostream>
#include <fstream>
#include <queue>
#include <algorithm>
#include <sstream>

GraphExtractor::GraphExtractor(Circuit& circ) : circuit(circ) {}

void GraphExtractor::extractPinsFromElements() {
    pins.clear();
    
    for (const auto& element : circuit.getElements()) {
        if (!element) continue;
        
        // Create pins for each element
        std::string elem_name = element->getName();
        std::string node1_id = element->getNode1Id();
        std::string node2_id = element->getNode2Id();
        
        // Calculate pin positions based on element position
        // For now, we'll use a simple approach - in practice, this would come from GUI
        SDL_Point pin1_pos = {0, 0}; // These would be calculated from GUI
        SDL_Point pin2_pos = {100, 0};
        
        auto pin1 = std::make_shared<Pin>(node1_id, elem_name, 1, pin1_pos);
        auto pin2 = std::make_shared<Pin>(node2_id, elem_name, 2, pin2_pos);
        
        pin1->setNodeId(node1_id);
        pin2->setNodeId(node2_id);
        
        pins.push_back(pin1);
        pins.push_back(pin2);
    }
}

void GraphExtractor::extractWiresFromPins() {
    wires.clear();
    
    // For now, we'll create wires based on shared node IDs
    // In practice, this would come from the GUI wire drawing
    std::map<std::string, std::vector<std::shared_ptr<Pin>>> node_to_pins;
    
    for (const auto& pin : pins) {
        std::string node_id = pin->getNodeId();
        if (!node_id.empty()) {
            node_to_pins[node_id].push_back(pin);
        }
    }
    
    // Create wires between pins that share the same node
    int wire_counter = 0;
    for (const auto& [node_id, pin_list] : node_to_pins) {
        if (pin_list.size() > 1) {
            // Create wires between all pins in this node
            for (size_t i = 0; i < pin_list.size() - 1; ++i) {
                std::string wire_id = "wire_" + std::to_string(wire_counter++);
                auto wire = std::make_shared<GuiWire>(wire_id, pin_list[i], pin_list[i + 1]);
                wire->setNodeId(node_id);
                wires.push_back(wire);
            }
        }
    }
}

void GraphExtractor::buildNodeMapping() {
    // Build mapping from pin IDs to node IDs
    for (const auto& pin : pins) {
        std::string pin_id = pin->getFullId();
        std::string node_id = pin->getNodeId();
        if (!node_id.empty()) {
            graph.node_mapping[pin_id] = node_id;
        }
    }
}

void GraphExtractor::mergeConnectedNodes() {
    // This method would handle merging nodes that are connected by wires
    // For now, we'll use the existing node structure from the circuit
}

void GraphExtractor::validateGraph() {
    // Check for basic graph validity
    for (const auto& edge : graph.edges) {
        if (edge.node1_id == edge.node2_id) {
            std::cout << "Warning: Self-loop detected in element " << edge.element_name << std::endl;
        }
    }
}

std::string GraphExtractor::generateNodeId(const std::string& element_name, int pin_number) {
    return element_name + "_pin" + std::to_string(pin_number);
}

CircuitGraph GraphExtractor::extractGraph() {
    CircuitGraph graph;
    graph.clear();
    
    // Extract pins and wires
    extractPinsFromElements();
    extractWiresFromPins();
    buildNodeMapping();
    
    // Build nodes from circuit
    for (const auto& [node_id, node] : circuit.getNodes()) {
        CircuitNode circuit_node(node_id);
        circuit_node.is_ground = node->getIsGround();
        circuit_node.voltage = node->getVoltage();
        
        // Find elements connected to this node
        for (const auto& element : circuit.getElements()) {
            if (element->getNode1Id() == node_id || element->getNode2Id() == node_id) {
                circuit_node.connected_elements.push_back(element->getName());
            }
        }
        
        graph.nodes[node_id] = circuit_node;
    }
    
    // Build edges from elements
    for (const auto& element : circuit.getElements()) {
        if (!element) continue;
        
        std::string edge_id = element->getName();
        std::string node1_id = element->getNode1Id();
        std::string node2_id = element->getNode2Id();
        std::string element_type = element->getType();
        double value = element->getValue();
        
        CircuitEdge edge(edge_id, edge_id, node1_id, node2_id, element_type, value);
        graph.edges.push_back(edge);
    }
    
    // Merge connected nodes and validate
    mergeConnectedNodes();
    validateGraph();
    
    return graph;
}

void GraphExtractor::addPin(std::shared_ptr<Pin> pin) {
    if (pin) {
        pins.push_back(pin);
    }
}

void GraphExtractor::addWire(std::shared_ptr<GuiWire> wire) {
    if (wire) {
        wires.push_back(wire);
    }
}

void GraphExtractor::removePin(const std::string& pin_id) {
    pins.erase(std::remove_if(pins.begin(), pins.end(),
        [&](const std::shared_ptr<Pin>& pin) { return pin->getId() == pin_id; }), pins.end());
}

void GraphExtractor::removeWire(const std::string& wire_id) {
    wires.erase(std::remove_if(wires.begin(), wires.end(),
        [&](const std::shared_ptr<GuiWire>& wire) { return wire->getId() == wire_id; }), wires.end());
}

bool GraphExtractor::isGraphConnected() const {
    if (graph.nodes.empty()) return true;
    
    std::set<std::string> visited;
    std::queue<std::string> q;
    
    // Start with the first node
    std::string start_node = graph.nodes.begin()->first;
    q.push(start_node);
    visited.insert(start_node);
    
    while (!q.empty()) {
        std::string current = q.front();
        q.pop();
        
        // Find all edges connected to this node
        for (const auto& edge : graph.edges) {
            std::string neighbor;
            if (edge.node1_id == current) {
                neighbor = edge.node2_id;
            } else if (edge.node2_id == current) {
                neighbor = edge.node1_id;
            } else {
                continue;
            }
            
            if (visited.find(neighbor) == visited.end()) {
                visited.insert(neighbor);
                q.push(neighbor);
            }
        }
    }
    
    return visited.size() == graph.nodes.size();
}

std::vector<std::string> GraphExtractor::findIsolatedNodes() const {
    std::vector<std::string> isolated;
    
    for (const auto& [node_id, node] : graph.nodes) {
        bool has_connections = false;
        for (const auto& edge : graph.edges) {
            if (edge.node1_id == node_id || edge.node2_id == node_id) {
                has_connections = true;
                break;
            }
        }
        if (!has_connections) {
            isolated.push_back(node_id);
        }
    }
    
    return isolated;
}

std::vector<std::string> GraphExtractor::findShortCircuits() const {
    std::vector<std::string> short_circuits;
    
    // Check for elements with zero resistance/inductance or infinite capacitance
    for (const auto& edge : graph.edges) {
        if (edge.element_type == "Resistor" && edge.value == 0.0) {
            short_circuits.push_back(edge.element_name);
        } else if (edge.element_type == "Inductor" && edge.value == 0.0) {
            short_circuits.push_back(edge.element_name);
        } else if (edge.element_type == "Capacitor" && std::isinf(edge.value)) {
            short_circuits.push_back(edge.element_name);
        }
    }
    
    return short_circuits;
}

std::map<std::string, int> GraphExtractor::createNodeIndexMap() const {
    std::map<std::string, int> node_map;
    int index = 0;
    
    for (const auto& [node_id, node] : graph.nodes) {
        if (!node.is_ground) {
            node_map[node_id] = index++;
        }
    }
    
    return node_map;
}

std::vector<std::string> GraphExtractor::getNonGroundNodes() const {
    std::vector<std::string> non_ground_nodes;
    
    for (const auto& [node_id, node] : graph.nodes) {
        if (!node.is_ground) {
            non_ground_nodes.push_back(node_id);
        }
    }
    
    return non_ground_nodes;
}

void GraphExtractor::printGraph() const {
    std::cout << "=== Circuit Graph ===" << std::endl;
    std::cout << "Nodes (" << graph.nodes.size() << "):" << std::endl;
    for (const auto& [node_id, node] : graph.nodes) {
        std::cout << "  " << node_id << (node.is_ground ? " (Ground)" : "") << std::endl;
    }
    
    std::cout << "Edges (" << graph.edges.size() << "):" << std::endl;
    for (const auto& edge : graph.edges) {
        std::cout << "  " << edge.element_name << " (" << edge.element_type << ") "
                  << edge.node1_id << " -> " << edge.node2_id;
        if (edge.value != std::numeric_limits<double>::quiet_NaN()) {
            std::cout << " = " << edge.value;
        }
        std::cout << std::endl;
    }
    
    std::cout << "Graph connected: " << (isGraphConnected() ? "Yes" : "No") << std::endl;
}

void GraphExtractor::exportGraphToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }
    
    file << "# Circuit Graph Export" << std::endl;
    file << "# Nodes:" << std::endl;
    for (const auto& [node_id, node] : graph.nodes) {
        file << "NODE " << node_id << " " << (node.is_ground ? "GROUND" : "NORMAL") << std::endl;
    }
    
    file << "# Edges:" << std::endl;
    for (const auto& edge : graph.edges) {
        file << "EDGE " << edge.element_name << " " << edge.element_type << " "
             << edge.node1_id << " " << edge.node2_id << " " << edge.value << std::endl;
    }
    
    file.close();
    std::cout << "Graph exported to " << filename << std::endl;
}
