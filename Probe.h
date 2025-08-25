#pragma once

#include <string>
#include <memory>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/memory.hpp>

/**
 * @brief Abstract base class for all probe types
 * 
 * Probes are used to measure electrical quantities at specific points
 * or across components in the circuit for plotting and analysis.
 */
class Probe {
protected:
    std::string name;           // Unique probe identifier
    std::string signal_name;    // Name of the signal being measured
    bool enabled;               // Whether the probe is active
    
    friend class cereal::access;
    Probe(); // Default constructor for Cereal
    
public:
    Probe(const std::string& probe_name, const std::string& signal);
    virtual ~Probe() = default;
    
    // Core interface
    virtual std::string getType() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getUnits() const = 0;
    
    // Accessors
    std::string getName() const { return name; }
    std::string getSignalName() const { return signal_name; }
    bool isEnabled() const { return enabled; }
    void setEnabled(bool state) { enabled = state; }
    
    // Serialization
    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(name), CEREAL_NVP(signal_name), CEREAL_NVP(enabled));
    }
};

/**
 * @brief Voltage probe for measuring node voltages
 */
class VoltageProbe : public Probe {
private:
    std::string node_id;        // Node to measure voltage at
    std::string reference_node; // Reference node (usually ground)
    
    friend class cereal::access;
    VoltageProbe(); // Default constructor for Cereal
    
public:
    VoltageProbe(const std::string& probe_name, const std::string& target_node, 
                 const std::string& ref_node = "0");
    
    std::string getType() const override { return "Voltage"; }
    std::string getDescription() const override;
    std::string getUnits() const override { return "V"; }
    
    std::string getNodeId() const { return node_id; }
    std::string getReferenceNode() const { return reference_node; }
    void setNodeId(const std::string& node) { node_id = node; }
    void setReferenceNode(const std::string& ref) { reference_node = ref; }
    
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Probe>(this), 
                CEREAL_NVP(node_id), CEREAL_NVP(reference_node));
    }
};

/**
 * @brief Current probe for measuring currents through components
 */
class CurrentProbe : public Probe {
private:
    std::string component_name; // Component to measure current through
    std::string element_type;   // Type of element (for validation)
    
    friend class cereal::access;
    CurrentProbe(); // Default constructor for Cereal
    
public:
    CurrentProbe(const std::string& probe_name, const std::string& component, 
                 const std::string& elem_type = "");
    
    std::string getType() const override { return "Current"; }
    std::string getDescription() const override;
    std::string getUnits() const override { return "A"; }
    
    std::string getComponentName() const { return component_name; }
    std::string getElementType() const { return element_type; }
    void setComponentName(const std::string& comp) { component_name = comp; }
    void setElementType(const std::string& type) { element_type = type; }
    
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Probe>(this), 
                CEREAL_NVP(component_name), CEREAL_NVP(element_type));
    }
};

/**
 * @brief Power probe for measuring power dissipation/consumption
 */
class PowerProbe : public Probe {
private:
    std::string component_name; // Component to measure power of
    std::string element_type;   // Type of element
    bool measure_dissipation;   // True for dissipation, false for supply
    
    friend class cereal::access;
    PowerProbe(); // Default constructor for Cereal
    
public:
    PowerProbe(const std::string& probe_name, const std::string& component, 
               const std::string& elem_type = "", bool dissipation = true);
    
    std::string getType() const override { return "Power"; }
    std::string getDescription() const override;
    std::string getUnits() const override { return "W"; }
    
    std::string getComponentName() const { return component_name; }
    std::string getElementType() const { return element_type; }
    bool isMeasuringDissipation() const { return measure_dissipation; }
    
    void setComponentName(const std::string& comp) { component_name = comp; }
    void setElementType(const std::string& type) { element_type = type; }
    void setMeasureDissipation(bool dissipation) { measure_dissipation = dissipation; }
    
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Probe>(this), 
                CEREAL_NVP(component_name), CEREAL_NVP(element_type), 
                CEREAL_NVP(measure_dissipation));
    }
};

/**
 * @brief Differential voltage probe for measuring voltage differences
 */
class DifferentialProbe : public Probe {
private:
    std::string positive_node;  // Positive measurement node
    std::string negative_node;  // Negative measurement node
    
    friend class cereal::access;
    DifferentialProbe(); // Default constructor for Cereal
    
public:
    DifferentialProbe(const std::string& probe_name, 
                      const std::string& pos_node, const std::string& neg_node);
    
    std::string getType() const override { return "Differential"; }
    std::string getDescription() const override;
    std::string getUnits() const override { return "V"; }
    
    std::string getPositiveNode() const { return positive_node; }
    std::string getNegativeNode() const { return negative_node; }
    void setPositiveNode(const std::string& node) { positive_node = node; }
    void setNegativeNode(const std::string& node) { negative_node = node; }
    
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Probe>(this), 
                CEREAL_NVP(positive_node), CEREAL_NVP(negative_node));
    }
};
