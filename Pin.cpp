#include "Pin.h"
#include "Wire.h"
#include <algorithm>

Pin::Pin(const std::string& pin_id, const std::string& elem_name, int pin_num, const SDL_Point& pos)
    : id(pin_id), element_name(elem_name), pin_number(pin_num), position(pos), 
      is_connected(false), node_id("") {
}

void Pin::addWire(std::shared_ptr<GuiWire> wire) {
    if (wire && std::find(connected_wires.begin(), connected_wires.end(), wire) == connected_wires.end()) {
        connected_wires.push_back(wire);
        updateConnectionStatus();
    }
}

void Pin::removeWire(std::shared_ptr<GuiWire> wire) {
    auto it = std::find(connected_wires.begin(), connected_wires.end(), wire);
    if (it != connected_wires.end()) {
        connected_wires.erase(it);
        updateConnectionStatus();
    }
}

void Pin::updateConnectionStatus() {
    is_connected = !connected_wires.empty();
}

bool Pin::isAtPosition(int x, int y) const {
    const int PIN_RADIUS = 15;  // Larger clickable radius around pin for easier clicking
    int dx = x - position.x;
    int dy = y - position.y;
    return (dx * dx + dy * dy) <= (PIN_RADIUS * PIN_RADIUS);
}

bool Pin::isNearPosition(int x, int y, int hover_radius) const {
    int dx = x - position.x;
    int dy = y - position.y;
    return (dx * dx + dy * dy) <= (hover_radius * hover_radius);
}

std::string Pin::getFullId() const {
    return element_name + "." + std::to_string(pin_number);
}
