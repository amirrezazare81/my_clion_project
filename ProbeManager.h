#pragma once

#include "Probe.h"
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>

/**
 * @brief Manages all probes in the circuit simulation
 * 
 * The ProbeManager handles creation, deletion, and organization of probes.
 * It also provides methods to extract probe data from simulation results.
 */
class ProbeManager {
private:
    std::vector<std::unique_ptr<Probe>> probes;
    std::map<std::string, int> probe_name_map; // Fast lookup by name
    
    friend class cereal::access;
    ProbeManager(); // Default constructor for Cereal
    
public:
    ProbeManager(bool init) : ProbeManager() {} // Public constructor
    
    // Probe management
    void addProbe(std::unique_ptr<Probe> probe);
    bool removeProbe(const std::string& probe_name);
    void clearProbes();
    
    // Probe access
    Probe* getProbe(const std::string& probe_name);
    const Probe* getProbe(const std::string& probe_name) const;
    std::vector<Probe*> getEnabledProbes();
    std::vector<const Probe*> getEnabledProbes() const;
    std::vector<Probe*> getProbesByType(const std::string& type);
    
    // Information
    size_t getProbeCount() const { return probes.size(); }
    size_t getEnabledProbeCount() const;
    std::vector<std::string> getProbeNames() const;
    std::vector<std::string> getEnabledProbeNames() const;
    
    // Data extraction from simulation results
    std::map<std::string, std::vector<double>> extractProbeData(
        const std::map<std::string, std::vector<double>>& simulation_results) const;
    
    // Signal selection helpers
    std::vector<std::string> getAvailableSignals(
        const std::map<std::string, std::vector<double>>& simulation_results) const;
    std::vector<std::string> getSelectedSignals() const;
    
    // Auto-probe creation
    void createVoltageProbesForAllNodes(const std::vector<std::string>& node_names);
    void createCurrentProbesForAllComponents(const std::vector<std::pair<std::string, std::string>>& components);
    
    // Serialization
    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(probes));
        // Rebuild the name map after deserialization
        probe_name_map.clear();
        for (size_t i = 0; i < probes.size(); ++i) {
            if (probes[i]) {
                probe_name_map[probes[i]->getName()] = static_cast<int>(i);
            }
        }
    }
    
private:
    void updateNameMap();
    bool isValidSignalName(const std::string& signal_name) const;
};
