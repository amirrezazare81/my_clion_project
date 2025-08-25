#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include "Circuit.h"
#include "Pin.h"
#include "Wire.h"

// Forward declarations
class Element;

// --- Circuit Graph Structure ---
struct CircuitNode {
    std::string id;
    std::vector<std::string> connected_elements;
    std::vector<std::string> connected_wires;
    bool is_ground;
    double voltage;
    
    CircuitNode() : id(""), is_ground(false), voltage(0.0) {}
    CircuitNode(const std::string& node_id) : id(node_id), is_ground(false), voltage(0.0) {}
};

struct CircuitEdge {
    std::string id;
    std::string element_name;
    std::string node1_id;
    std::string node2_id;
    std::string element_type;
    double value;
    
    CircuitEdge(const std::string& edge_id, const std::string& elem_name, 
                const std::string& n1, const std::string& n2, 
                const std::string& type, double val)
        : id(edge_id), element_name(elem_name), node1_id(n1), node2_id(n2), 
          element_type(type), value(val) {}
};

struct CircuitGraph {
    std::map<std::string, CircuitNode> nodes;
    std::vector<CircuitEdge> edges;
    std::map<std::string, std::string> node_mapping; // Maps pin IDs to node IDs
    
    void clear() {
        nodes.clear();
        edges.clear();
        node_mapping.clear();
    }
};

// --- Graph Extractor Class ---
class GraphExtractor {
private:
    Circuit& circuit;
    std::vector<std::shared_ptr<Pin>> pins;
    std::vector<std::shared_ptr<GuiWire>> wires;
    mutable CircuitGraph graph; // Make it mutable so const methods can modify it
    
    // Helper methods
    void extractPinsFromElements();
    void extractWiresFromPins();
    void buildNodeMapping();
    void mergeConnectedNodes();
    void validateGraph();
    std::string generateNodeId(const std::string& element_name, int pin_number);
    
public:
    explicit GraphExtractor(Circuit& circ);
    
    // Main extraction method
    CircuitGraph extractGraph();
    
    // Pin and wire management
    void addPin(std::shared_ptr<Pin> pin);
    void addWire(std::shared_ptr<GuiWire> wire);
    void removePin(const std::string& pin_id);
    void removeWire(const std::string& wire_id);
    
    // Graph analysis
    bool isGraphConnected() const;
    std::vector<std::string> findIsolatedNodes() const;
    std::vector<std::string> findShortCircuits() const;
    
    // MNA preparation
    std::map<std::string, int> createNodeIndexMap() const;
    std::vector<std::string> getNonGroundNodes() const;
    
    // Utility methods
    void printGraph() const;
    void exportGraphToFile(const std::string& filename) const;
};
