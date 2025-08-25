#include "ProbeManager.h"
#include "ErrorManager.h"
#include <algorithm>
#include <sstream>

ProbeManager::ProbeManager() {}

void ProbeManager::addProbe(std::unique_ptr<Probe> probe) {
    if (!probe) {
        ErrorManager::warn("[ProbeManager] Attempted to add null probe");
        return;
    }
    
    std::string probe_name = probe->getName();
    
    // Check for duplicate names
    if (probe_name_map.count(probe_name)) {
        ErrorManager::warn("[ProbeManager] Probe with name '" + probe_name + "' already exists");
        return;
    }
    
    // Add probe and update name map
    probe_name_map[probe_name] = static_cast<int>(probes.size());
    probes.push_back(std::move(probe));
    
    ErrorManager::info("[ProbeManager] Added probe: " + probe_name);
}

bool ProbeManager::removeProbe(const std::string& probe_name) {
    if (!probe_name_map.count(probe_name)) {
        ErrorManager::warn("[ProbeManager] Probe '" + probe_name + "' not found");
        return false;
    }
    
    int index = probe_name_map[probe_name];
    
    // Remove from vector
    probes.erase(probes.begin() + index);
    
    // Update name map
    updateNameMap();
    
    ErrorManager::info("[ProbeManager] Removed probe: " + probe_name);
    return true;
}

void ProbeManager::clearProbes() {
    probes.clear();
    probe_name_map.clear();
    ErrorManager::info("[ProbeManager] Cleared all probes");
}

Probe* ProbeManager::getProbe(const std::string& probe_name) {
    if (!probe_name_map.count(probe_name)) {
        return nullptr;
    }
    int index = probe_name_map[probe_name];
    return (index >= 0 && index < static_cast<int>(probes.size())) ? probes[index].get() : nullptr;
}

const Probe* ProbeManager::getProbe(const std::string& probe_name) const {
    if (!probe_name_map.count(probe_name)) {
        return nullptr;
    }
    int index = probe_name_map.at(probe_name);
    return (index >= 0 && index < static_cast<int>(probes.size())) ? probes[index].get() : nullptr;
}

std::vector<Probe*> ProbeManager::getEnabledProbes() {
    std::vector<Probe*> enabled_probes;
    for (auto& probe : probes) {
        if (probe && probe->isEnabled()) {
            enabled_probes.push_back(probe.get());
        }
    }
    return enabled_probes;
}

std::vector<const Probe*> ProbeManager::getEnabledProbes() const {
    std::vector<const Probe*> enabled_probes;
    for (const auto& probe : probes) {
        if (probe && probe->isEnabled()) {
            enabled_probes.push_back(probe.get());
        }
    }
    return enabled_probes;
}

std::vector<Probe*> ProbeManager::getProbesByType(const std::string& type) {
    std::vector<Probe*> typed_probes;
    for (auto& probe : probes) {
        if (probe && probe->getType() == type) {
            typed_probes.push_back(probe.get());
        }
    }
    return typed_probes;
}

size_t ProbeManager::getEnabledProbeCount() const {
    size_t count = 0;
    for (const auto& probe : probes) {
        if (probe && probe->isEnabled()) {
            count++;
        }
    }
    return count;
}

std::vector<std::string> ProbeManager::getProbeNames() const {
    std::vector<std::string> names;
    for (const auto& probe : probes) {
        if (probe) {
            names.push_back(probe->getName());
        }
    }
    return names;
}

std::vector<std::string> ProbeManager::getEnabledProbeNames() const {
    std::vector<std::string> names;
    for (const auto& probe : probes) {
        if (probe && probe->isEnabled()) {
            names.push_back(probe->getName());
        }
    }
    return names;
}

std::map<std::string, std::vector<double>> ProbeManager::extractProbeData(
    const std::map<std::string, std::vector<double>>& simulation_results) const {
    
    std::map<std::string, std::vector<double>> probe_data;
    
    for (const auto& probe : probes) {
        if (!probe || !probe->isEnabled()) {
            continue;
        }
        
        std::string signal_name = probe->getSignalName();
        
        // Check if this signal exists in simulation results
        if (simulation_results.count(signal_name)) {
            probe_data[probe->getName()] = simulation_results.at(signal_name);
            ErrorManager::info("[ProbeManager] Extracted data for probe: " + probe->getName() + 
                             " (signal: " + signal_name + ")");
        } else {
            ErrorManager::warn("[ProbeManager] Signal '" + signal_name + 
                             "' not found in simulation results for probe: " + probe->getName());
            
            // Create empty vector to maintain consistency
            if (!simulation_results.empty()) {
                size_t result_size = simulation_results.begin()->second.size();
                probe_data[probe->getName()] = std::vector<double>(result_size, 0.0);
            }
        }
    }
    
    return probe_data;
}

std::vector<std::string> ProbeManager::getAvailableSignals(
    const std::map<std::string, std::vector<double>>& simulation_results) const {
    
    std::vector<std::string> signals;
    for (const auto& pair : simulation_results) {
        signals.push_back(pair.first);
    }
    
    // Sort for consistent ordering
    std::sort(signals.begin(), signals.end());
    return signals;
}

std::vector<std::string> ProbeManager::getSelectedSignals() const {
    std::vector<std::string> signals;
    for (const auto& probe : probes) {
        if (probe && probe->isEnabled()) {
            signals.push_back(probe->getSignalName());
        }
    }
    return signals;
}

void ProbeManager::createVoltageProbesForAllNodes(const std::vector<std::string>& node_names) {
    int probe_count = 0;
    for (const auto& node : node_names) {
        if (node == "0") continue; // Skip ground node
        
        std::string probe_name = "VP_" + node;
        
        // Check if probe already exists
        if (probe_name_map.count(probe_name)) {
            continue;
        }
        
        auto voltage_probe = std::make_unique<VoltageProbe>(probe_name, node);
        addProbe(std::move(voltage_probe));
        probe_count++;
    }
    
    if (probe_count > 0) {
        ErrorManager::info("[ProbeManager] Auto-created " + std::to_string(probe_count) + " voltage probes");
    }
}

void ProbeManager::createCurrentProbesForAllComponents(
    const std::vector<std::pair<std::string, std::string>>& components) {
    
    int probe_count = 0;
    for (const auto& component : components) {
        const std::string& name = component.first;
        const std::string& type = component.second;
        
        std::string probe_name = "IP_" + name;
        
        // Check if probe already exists
        if (probe_name_map.count(probe_name)) {
            continue;
        }
        
        auto current_probe = std::make_unique<CurrentProbe>(probe_name, name, type);
        addProbe(std::move(current_probe));
        probe_count++;
    }
    
    if (probe_count > 0) {
        ErrorManager::info("[ProbeManager] Auto-created " + std::to_string(probe_count) + " current probes");
    }
}

void ProbeManager::updateNameMap() {
    probe_name_map.clear();
    for (size_t i = 0; i < probes.size(); ++i) {
        if (probes[i]) {
            probe_name_map[probes[i]->getName()] = static_cast<int>(i);
        }
    }
}

bool ProbeManager::isValidSignalName(const std::string& signal_name) const {
    // Basic validation - signals should start with V( or I( or P(
    return (signal_name.substr(0, 2) == "V(" || 
            signal_name.substr(0, 2) == "I(" || 
            signal_name.substr(0, 2) == "P(") && 
           signal_name.back() == ')';
}
