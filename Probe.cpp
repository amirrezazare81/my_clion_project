#include "Probe.h"

// --- Base Probe Implementation ---
Probe::Probe() : name(""), signal_name(""), enabled(true) {}

Probe::Probe(const std::string& probe_name, const std::string& signal)
    : name(probe_name), signal_name(signal), enabled(true) {}

// --- Voltage Probe Implementation ---
VoltageProbe::VoltageProbe() : Probe(), node_id(""), reference_node("0") {}

VoltageProbe::VoltageProbe(const std::string& probe_name, const std::string& target_node, 
                           const std::string& ref_node)
    : Probe(probe_name, "V(" + target_node + ")"), node_id(target_node), reference_node(ref_node) {}

std::string VoltageProbe::getDescription() const {
    if (reference_node == "0") {
        return "Voltage at node " + node_id + " (referenced to ground)";
    } else {
        return "Voltage between " + node_id + " and " + reference_node;
    }
}

// --- Current Probe Implementation ---
CurrentProbe::CurrentProbe() : Probe(), component_name(""), element_type("") {}

CurrentProbe::CurrentProbe(const std::string& probe_name, const std::string& component, 
                           const std::string& elem_type)
    : Probe(probe_name, "I(" + component + ")"), component_name(component), element_type(elem_type) {}

std::string CurrentProbe::getDescription() const {
    if (element_type.empty()) {
        return "Current through " + component_name;
    } else {
        return "Current through " + element_type + " " + component_name;
    }
}

// --- Power Probe Implementation ---
PowerProbe::PowerProbe() : Probe(), component_name(""), element_type(""), measure_dissipation(true) {}

PowerProbe::PowerProbe(const std::string& probe_name, const std::string& component, 
                       const std::string& elem_type, bool dissipation)
    : Probe(probe_name, "P(" + component + ")"), component_name(component), 
      element_type(elem_type), measure_dissipation(dissipation) {}

std::string PowerProbe::getDescription() const {
    std::string action = measure_dissipation ? "dissipated by" : "supplied by";
    if (element_type.empty()) {
        return "Power " + action + " " + component_name;
    } else {
        return "Power " + action + " " + element_type + " " + component_name;
    }
}

// --- Differential Probe Implementation ---
DifferentialProbe::DifferentialProbe() : Probe(), positive_node(""), negative_node("") {}

DifferentialProbe::DifferentialProbe(const std::string& probe_name, 
                                     const std::string& pos_node, const std::string& neg_node)
    : Probe(probe_name, "V(" + pos_node + "," + neg_node + ")"), 
      positive_node(pos_node), negative_node(neg_node) {}

std::string DifferentialProbe::getDescription() const {
    return "Differential voltage: V(" + positive_node + ") - V(" + negative_node + ")";
}
