#include "Node.h"

Node::Node(const std::string& id) : id(id), voltage(0.0), isGround(false) {}

std::string Node::getId() const { return id; }

double Node::getVoltage() const { return voltage; }

bool Node::getIsGround() const { return isGround; }

void Node::setVoltage(double vol) { voltage = vol; }

void Node::setAsGround() {
    isGround = true;
    voltage = 0.0;
}

void Node::setAsGround(bool ground) {
    isGround = ground;
    if (ground) {
        voltage = 0.0;
    }
}