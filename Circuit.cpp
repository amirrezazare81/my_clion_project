#include "Circuit.h"
#include "ErrorManager.h"
#include "Element.h"
#include "Node.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <queue>
#include <stdexcept>
#include <fstream>

Circuit::Circuit() : ground_node_id("") {}
Circuit::~Circuit() = default;

Node* Circuit::getOrCreateNode(const std::string& node_id) {
    if (nodes.find(node_id) == nodes.end()) {
        nodes[node_id] = std::make_unique<Node>(node_id);
    }
    return nodes[node_id].get();
}

void Circuit::addElement(std::unique_ptr<Element> element) {
    if (!element) return;
    if (hasElement(element->getName())) {
        throw std::runtime_error("Element with name '" + element->getName() + "' already exists.");
    }
    getOrCreateNode(element->getNode1Id());

    if (element->getType() == "VoltageControlledVoltageSource" || element->getType() == "VoltageControlledCurrentSource") {
        auto* vcvs = static_cast<VoltageControlledVoltageSource*>(element.get());
        getOrCreateNode(vcvs->getControlNode1Id());
        getOrCreateNode(vcvs->getControlNode2Id());
    }

    if (!element->getNode2Id().empty()) {
        getOrCreateNode(element->getNode2Id());
    }
    if (element->getType() == "Ground") {
        setGroundNode(element->getNode1Id());
    }
    elements.push_back(std::move(element));
}

void Circuit::deleteElement(const std::string& name) {
    auto it = std::remove_if(elements.begin(), elements.end(),
        [&](const std::unique_ptr<Element>& elem) { return elem->getName() == name; });
    if (it != elements.end()) {
        elements.erase(it, elements.end());
    } else {
        throw std::runtime_error("Element not found.");
    }
}

void Circuit::clear() {
    elements.clear();
    nodes.clear();
    node_labels.clear();
    ground_node_id = "";
}

void Circuit::setGroundNode(const std::string& node_id) {
    ground_node_id = node_id;
    getOrCreateNode(node_id)->setAsGround();
}

std::string Circuit::getGroundNodeId() const { return ground_node_id; }
const std::vector<std::unique_ptr<Element>>& Circuit::getElements() const { return elements; }
const std::map<std::string, std::unique_ptr<Node>>& Circuit::getNodes() const { return nodes; }

Element* Circuit::getElement(const std::string& name) const {
    for (const auto& elem : elements) {
        if (elem->getName() == name) return elem.get();
    }
    return nullptr;
}

bool Circuit::hasElement(const std::string& name) const {
    return getElement(name) != nullptr;
}

Node* Circuit::getNode(const std::string& node_id) const {
    auto it = nodes.find(node_id);
    return (it != nodes.end()) ? it->second.get() : nullptr;
}

bool Circuit::hasNode(const std::string& node_id) const {
    return nodes.count(node_id) > 0;
}

void Circuit::listNodes() const {
    for (const auto& pair : nodes) {
        std::cout << "- " << pair.first << (pair.second->getIsGround() ? " (Ground)" : "") << std::endl;
    }
}

void Circuit::listElements(const std::string& type_filter) const {
    for (const auto& elem : elements) {
        if (type_filter.empty() || elem->getType() == type_filter) {
            std::cout << "- " << elem->getAddCommandString() << std::endl;
        }
    }
}

void Circuit::renameNode(const std::string& old_name, const std::string& new_name) {
    if (!hasNode(old_name)) throw std::runtime_error("Node <" + old_name + "> does not exist.");
    if (hasNode(new_name)) throw std::runtime_error("Node name <" + new_name + "> already exists.");

    auto node_handle = nodes.extract(old_name);
    node_handle.key() = new_name;
    nodes.insert(std::move(node_handle));

    if (ground_node_id == old_name) ground_node_id = new_name;

    for (const auto& elem : elements) {
        if (elem->getNode1Id() == old_name) elem->setNode1Id(new_name);
        if (elem->getNode2Id() == old_name) elem->setNode2Id(new_name);
    }
}

bool Circuit::checkGroundNodeExists() const {
    return !ground_node_id.empty();
}

bool Circuit::checkConnectivity() const {
    if (nodes.empty()) return true;
    std::set<std::string> visited;
    std::queue<std::string> q;
    q.push(nodes.begin()->first);
    visited.insert(nodes.begin()->first);

    while (!q.empty()) {
        std::string u = q.front();
        q.pop();
        for (const auto& elem : elements) {
            std::string v = "";
            if (elem->getNode1Id() == u) v = elem->getNode2Id();
            else if (elem->getNode2Id() == u) v = elem->getNode1Id();
            if (!v.empty() && visited.find(v) == visited.end()) {
                visited.insert(v);
                q.push(v);
            }
        }
    }
    return visited.size() == nodes.size();
}

void Circuit::getNonGroundNodes(std::vector<Node*>& non_ground_nodes, NodeIndexMap& node_map) const {
    non_ground_nodes.clear();
    node_map.clear();
    int index = 0;
    for (const auto& pair : nodes) {
        if (!pair.second->getIsGround()) {
            non_ground_nodes.push_back(pair.second.get());
            node_map[pair.first] = index++;
        }
    }
}

int Circuit::getNumNonGroundNodes() const {
    int count = 0;
    for (const auto& pair : nodes) {
        if (!pair.second->getIsGround()) count++;
    }
    return count;
}

void Circuit::updatePreviousNodeVoltages(const std::map<std::string, double>& current_voltages) {
    previous_node_voltages = current_voltages;
}

void Circuit::updatePreviousInductorCurrents(const std::map<std::string, double>& current_inductor_currents) {
    previous_inductor_currents = current_inductor_currents;
}

const std::map<std::string, double>& Circuit::getPreviousInductorCurrents() const {
    return previous_inductor_currents;
}

void Circuit::saveToFile(const std::string& filename) const {
    std::ofstream outfile(filename);
    if (!outfile.is_open()) throw std::runtime_error("Could not save circuit to file '" + filename + "'.");
    for (const auto& elem : elements) {
        outfile << elem->getAddCommandString() << std::endl;
    }
}

// --- NEW: Implementation for node labels ---
void Circuit::addNodeLabel(const std::string& node_id, const std::string& label) {
    node_labels[node_id] = label;
}

const std::map<std::string, std::string>& Circuit::getNodeLabels() const {
    return node_labels;
}
