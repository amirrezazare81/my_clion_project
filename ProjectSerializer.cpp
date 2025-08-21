#include "ProjectSerializer.h"
#include <fstream>
#include <stdexcept>
#include <cereal/archives/json.hpp>

void ProjectSerializer::save(const Circuit& circuit, const std::string& filepath) {
    std::ofstream os(filepath);
    if (!os.is_open()) throw std::runtime_error("Failed to open file for saving: " + filepath);
    cereal::JSONOutputArchive archive(os);
    archive(cereal::make_nvp("circuit_elements", circuit.getElements()),
            cereal::make_nvp("ground_node", circuit.getGroundNodeId()));
}

void ProjectSerializer::load(Circuit& circuit, const std::string& filepath) {
    std::ifstream is(filepath);
    if (!is.is_open()) throw std::runtime_error("Failed to open file for loading: " + filepath);
    cereal::JSONInputArchive archive(is);
    std::vector<std::unique_ptr<Element>> loaded_elements;
    std::string loaded_ground_node_id;
    archive(loaded_elements, loaded_ground_node_id);
    circuit.clear();
    for (auto& elem : loaded_elements) {
        circuit.addElement(std::move(elem));
    }
    circuit.setGroundNode(loaded_ground_node_id);
}
