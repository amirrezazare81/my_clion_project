#pragma once
#include <string>
#include "Circuit.h"

// Class for saving and loading circuit projects using Cereal
class ProjectSerializer {
public:
    static void save(const Circuit& circuit, const std::string& filepath);
    static void load(Circuit& circuit, const std::string& filepath);
};
