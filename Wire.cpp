#include "Wire.h"
#include "Pin.h"
#include <algorithm>
#include <sstream>
#include <iostream>

GuiWire::GuiWire(const std::string& wire_id, std::shared_ptr<Pin> start, std::shared_ptr<Pin> end)
    : Element(wire_id, "", ""), id(wire_id), start_pin(start), end_pin(end), is_selected(false), node_id("") {
    // Generate a unique node ID for this wire
    if (start && end) {
        node_id = "wire_" + start->getElementName() + "_" + std::to_string(start->getPinNumber()) + 
                  "_to_" + end->getElementName() + "_" + std::to_string(end->getPinNumber());
    }
}

void GuiWire::addWaypoint(const SDL_Point& point) {
    waypoints.push_back(point);
}

void GuiWire::removeWaypoint(size_t index) {
    if (index < waypoints.size()) {
        waypoints.erase(waypoints.begin() + index);
    }
}

void GuiWire::clearWaypoints() {
    waypoints.clear();
}

bool GuiWire::isNearPoint(int x, int y) const {
    const int CLICK_THRESHOLD = 10;
    
    // Check if point is near the main wire segment
    if (start_pin && end_pin) {
        SDL_Point start = start_pin->getPosition();
        SDL_Point end = end_pin->getPosition();
        
        // Calculate distance from point to line segment
        int A = x - start.x;
        int B = y - start.y;
        int C = end.x - start.x;
        int D = end.y - start.y;
        
        int dot = A * C + B * D;
        int len_sq = C * C + D * D;
        
        if (len_sq != 0) {
            double param = static_cast<double>(dot) / len_sq;
            if (param >= 0 && param <= 1) {
                int proj_x = start.x + static_cast<int>(param * C);
                int proj_y = start.y + static_cast<int>(param * D);
                
                int dx = x - proj_x;
                int dy = y - proj_y;
                if (dx * dx + dy * dy <= CLICK_THRESHOLD * CLICK_THRESHOLD) {
                    return true;
                }
            }
        }
    }
    
    // Check waypoints
    for (const auto& waypoint : waypoints) {
        int dx = x - waypoint.x;
        int dy = y - waypoint.y;
        if (dx * dx + dy * dy <= CLICK_THRESHOLD * CLICK_THRESHOLD) {
            return true;
        }
    }
    
    return false;
}

void GuiWire::updatePinConnections() {
    if (start_pin) start_pin->updateConnectionStatus();
    if (end_pin) end_pin->updateConnectionStatus();
}

std::string GuiWire::getDescription() const {
    std::ostringstream oss;
    oss << "Wire " << id;
    if (start_pin && end_pin) {
        oss << " from " << start_pin->getFullId() << " to " << end_pin->getFullId();
    }
    return oss.str();
}

void GuiWire::serialize(std::ostream& os) const {
    os << "WIRE " << id;
    if (start_pin) os << " " << start_pin->getFullId();
    if (end_pin) os << " " << end_pin->getFullId();
    os << " " << node_id;
    os << std::endl;
}

std::shared_ptr<GuiWire> GuiWire::deserialize(std::istream& is) {
    std::string id, start_pin_str, end_pin_str, node_id;
    is >> id >> start_pin_str >> end_pin_str >> node_id;
    
    // This is a simplified deserialization - in practice you'd need to look up the actual pins
    auto wire = std::make_shared<GuiWire>(id, nullptr, nullptr);
    wire->setNodeId(node_id);
    return wire;
}
