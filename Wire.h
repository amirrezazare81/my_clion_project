#pragma once
#include <string>
#include <SDL.h>
#include <memory>
#include <vector>
#include "Element.h"

// Forward declarations
class Pin;

// --- GuiWire Class ---
// Represents a visual and electrical connection between pins
class GuiWire : public Element {
private:
    std::string id;
    std::shared_ptr<Pin> start_pin;
    std::shared_ptr<Pin> end_pin;
    std::vector<SDL_Point> waypoints;  // For curved or multi-segment wires
    bool is_selected;
    std::string node_id;  // The electrical node this wire represents

public:
    GuiWire(const std::string& wire_id, std::shared_ptr<Pin> start, std::shared_ptr<Pin> end);
    
    // Getters
    std::string getId() const { return id; }
    std::shared_ptr<Pin> getStartPin() const { return start_pin; }
    std::shared_ptr<Pin> getEndPin() const { return end_pin; }
    bool isSelected() const { return is_selected; }
    std::string getNodeId() const { return node_id; }
    const std::vector<SDL_Point>& getWaypoints() const { return waypoints; }
    
    // Setters
    void setSelected(bool selected) { is_selected = selected; }
    void setNodeId(const std::string& node) { node_id = node; }
    
    // Wire manipulation
    void addWaypoint(const SDL_Point& point);
    void removeWaypoint(size_t index);
    void clearWaypoints();
    
    // Utility
    bool isNearPoint(int x, int y) const;
    void updatePinConnections();
    std::string getDescription() const;
    
    // Serialization
    void serialize(std::ostream& os) const;
    static std::shared_ptr<GuiWire> deserialize(std::istream& is);
    
    // Element virtual method implementations
    std::string getType() const override { return "GuiWire"; }
    double getValue() const override { return 0.0; }
    void setValue(double value) override { /* GUI wires don't have values */ }
    std::string getAddCommandString() const override { return "* guiwire " + id; }
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map,
                         const std::map<std::string, double>& prev_node_voltages,
                         bool is_transient, double timestep) override {
        // GUI wires don't contribute to MNA - they're just visual connections
    }
};
