#pragma once
#include <map>
#include <vector>
#include <memory>
#include <string>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include "Node.h"
#include "Element.h"

// --- 5. Circuit (Manages elements and nodes) ---
class Circuit {
private:
    std::map<std::string, std::unique_ptr<Node>> nodes;
    std::vector<std::unique_ptr<Element>> elements;
    std::string ground_node_id;
    std::map<std::string, std::string> node_labels; // <-- NEW: To store node labels
    Node* getOrCreateNode(const std::string& node_id);

public:
    Circuit();
    ~Circuit();
    Circuit(const Circuit&) = delete;
    Circuit& operator=(const Circuit&) = delete;

    void addElement(std::unique_ptr<Element> element);
    void deleteElement(const std::string& name);
    void clear();
    void setGroundNode(const std::string& node_id);
    std::string getGroundNodeId() const;
    const std::vector<std::unique_ptr<Element>>& getElements() const;
    const std::map<std::string, std::unique_ptr<Node>>& getNodes() const;
    Element* getElement(const std::string& name) const;
    bool hasElement(const std::string& name) const;
    Node* getNode(const std::string& node_id) const;
    bool hasNode(const std::string& node_id) const;
    void listNodes() const;
    void listElements(const std::string& type_filter = "") const;
    void renameNode(const std::string& old_name, const std::string& new_name);
    bool checkGroundNodeExists() const;
    bool checkConnectivity() const;
    void getNonGroundNodes(std::vector<Node*>& non_ground_nodes, NodeIndexMap& node_map) const;
    int getNumNonGroundNodes() const;
    void updatePreviousNodeVoltages(const std::map<std::string, double>& current_voltages);
    void updatePreviousInductorCurrents(const std::map<std::string, double>& current_inductor_currents);
    const std::map<std::string, double>& getPreviousInductorCurrents() const;
    void saveToFile(const std::string& filename) const;

    // --- NEW: Methods for node labels ---
    void addNodeLabel(const std::string& node_id, const std::string& label);
    const std::map<std::string, std::string>& getNodeLabels() const;

    // State for transient analysis
    std::map<std::string, double> previous_node_voltages;
    std::map<std::string, double> previous_inductor_currents;

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(elements), CEREAL_NVP(ground_node_id), CEREAL_NVP(node_labels));
    }
};
