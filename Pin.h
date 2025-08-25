#pragma once
#include <string>
#include <SDL.h>
#include <memory>
#include <vector> // Added missing include for std::vector

// Forward declarations
class Element;
class GuiWire;

// --- Pin Class ---
// Represents a connection point on a circuit element
class Pin {
private:
    std::string id;
    std::string element_name;
    int pin_number;  // 1 or 2 for two-terminal elements
    SDL_Point position;  // Screen coordinates
    bool is_connected;
    std::vector<std::shared_ptr<GuiWire>> connected_wires;
    std::string node_id;  // The electrical node this pin connects to

public:
    Pin(const std::string& pin_id, const std::string& elem_name, int pin_num, const SDL_Point& pos);
    
    // Getters
    std::string getId() const { return id; }
    std::string getElementName() const { return element_name; }
    int getPinNumber() const { return pin_number; }
    SDL_Point getPosition() const { return position; }
    bool isConnected() const { return is_connected; }
    std::string getNodeId() const { return node_id; }
    const std::vector<std::shared_ptr<GuiWire>>& getConnectedWires() const { return connected_wires; }
    
    // Setters
    void setPosition(const SDL_Point& pos) { position = pos; }
    void setNodeId(const std::string& node) { node_id = node; }
    
    // Wire management
    void addWire(std::shared_ptr<GuiWire> wire);
    void removeWire(std::shared_ptr<GuiWire> wire);
    void updateConnectionStatus();
    
    // Utility
    bool isAtPosition(int x, int y) const;
    bool isNearPosition(int x, int y, int hover_radius = 15) const;  // For hover detection
    std::string getFullId() const;  // Returns "element_name.pin_number"

    // Hover state
    bool is_hovered = false;
};
