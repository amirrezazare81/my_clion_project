#pragma once
#include <vector>
#include <string>

// Forward declarations
class Circuit;
class MNAMatrix;
class LinearSolver;

class InputParser {
public:
    std::vector<std::string> tokenize(const std::string& line);
    void parseCommand(const std::vector<std::string>& tokens, Circuit& circuit, MNAMatrix& mna, const LinearSolver& solver);
    void parseFile(const std::string& file_path, Circuit& circuit, MNAMatrix& mna, const LinearSolver& solver);
private:
    void parseAddCommand(const std::vector<std::string>& tokens, Circuit& circuit);
    double parseValue(const std::string& value_str);
};
