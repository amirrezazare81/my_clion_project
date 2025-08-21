#include "InputParser.h"
#include "Element.h"
#include "Circuit.h"
#include "Analyzers.h" // <-- FIX: Changed from TransientAnalysis.h
#include "ErrorManager.h"
#include <sstream>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>

std::vector<std::string> InputParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token.front() == '*' || token.front() == ';') break; // Comment line
        tokens.push_back(token);
    }
    return tokens;
}

double InputParser::parseValue(const std::string& value_str) {
    if (value_str.empty()) {
        throw std::runtime_error("Invalid numeric value format: empty string.");
    }
    std::string num_part = value_str;
    double multiplier = 1.0;
    char last_char = tolower(value_str.back());

    if (isalpha(last_char)) {
        num_part = value_str.substr(0, value_str.length() - 1);
        switch (last_char) {
            case 't': multiplier = 1e12; break;
            case 'g': multiplier = 1e9; break;
            case 'k': multiplier = 1e3; break;
            case 'm': multiplier = 1e-3; break;
            case 'u': multiplier = 1e-6; break;
            case 'n': multiplier = 1e-9; break;
            case 'p': multiplier = 1e-12; break;
            case 'f': multiplier = 1e-15; break;
            default: break;
        }
    }

    try {
        return std::stod(num_part) * multiplier;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid numeric value format: '" + value_str + "'.");
    }
}

void InputParser::parseAddCommand(const std::vector<std::string>& tokens, Circuit& circuit) {
    if (tokens.size() < 2) throw std::runtime_error("Insufficient parameters for 'add' command.");

    std::string full_id = tokens[1];
    char type_char = toupper(full_id[0]);
    std::string name = full_id;

    if (type_char == 'G' && (full_id == "GND" || full_id == "gnd")) {
        if (tokens.size() != 3) throw std::runtime_error("Invalid GND syntax. Expected: add GND <node>");
        circuit.addElement(std::make_unique<Ground>("GND", tokens[2]));
    } else if (type_char == 'R' || type_char == 'C' || type_char == 'L' || type_char == 'I') {
        if (tokens.size() != 5) throw std::runtime_error("Invalid syntax. Expected: add <id> <n1> <n2> <value>");
        std::string n1 = tokens[2], n2 = tokens[3];
        double val = parseValue(tokens[4]);
        if (type_char == 'R') circuit.addElement(std::make_unique<Resistor>(name, n1, n2, val));
        else if (type_char == 'C') circuit.addElement(std::make_unique<Capacitor>(name, n1, n2, val));
        else if (type_char == 'L') circuit.addElement(std::make_unique<Inductor>(name, n1, n2, val));
        else if (type_char == 'I') circuit.addElement(std::make_unique<IndependentCurrentSource>(name, n1, n2, val));
    } else if (type_char == 'D') {
        if (tokens.size() != 5) throw std::runtime_error("Invalid syntax for Diode. Expected: add <id> <n1> <n2> <model>");
        circuit.addElement(std::make_unique<Diode>(name, tokens[2], tokens[3], tokens[4]));
    } else if (type_char == 'V') { // Voltage Sources
        if (tokens.size() < 5) throw std::runtime_error("Invalid syntax for V source.");
        std::string n1 = tokens[2], n2 = tokens[3];
        if (tokens.size() > 5 && (tokens[4] == "PULSE" || tokens[4] == "pulse")) {
            if (tokens.size() != 14 || tokens[5] != "(" || tokens[13] != ")") throw std::runtime_error("Invalid PULSE syntax. Expected: add V<name> <n+> <n-> PULSE ( V1 V2 TD TR TF PW PER )");
            circuit.addElement(std::make_unique<PulseVoltageSource>(name, n1, n2, parseValue(tokens[6]), parseValue(tokens[7]), parseValue(tokens[8]), parseValue(tokens[9]), parseValue(tokens[10]), parseValue(tokens[11]), parseValue(tokens[12])));
        } else if (tokens.size() > 5 && (tokens[4] == "SIN" || tokens[4] == "sin")) {
            if (tokens.size() != 10 || tokens[5] != "(" || tokens[9] != ")") {
                throw std::runtime_error("Invalid SIN syntax. Expected: add V<name> <n+> <n-> SIN ( Voffset Vamplitude Frequency )");
            }
            circuit.addElement(std::make_unique<SinusoidalVoltageSource>(name, n1, n2, parseValue(tokens[6]), parseValue(tokens[7]), parseValue(tokens[8])));
        } else {
             if (tokens.size() != 5) throw std::runtime_error("Invalid syntax for DC Voltage Source.");
             circuit.addElement(std::make_unique<IndependentVoltageSource>(name, n1, n2, parseValue(tokens[4])));
        }
    } else if (type_char == 'E') { // VCVS
        if (tokens.size() != 7) throw std::runtime_error("Invalid syntax for VCVS. Expected: add E<name> <n+> <n-> <ctrl_n+> <ctrl_n-> <gain>");
        circuit.addElement(std::make_unique<VoltageControlledVoltageSource>(name, tokens[2], tokens[3], tokens[4], tokens[5], parseValue(tokens[6])));
    }
    else {
        throw std::runtime_error("Unknown element type: '" + full_id + "'.");
    }
    std::cout << "Added element: " << name << std::endl;
}


void InputParser::parseCommand(const std::vector<std::string>& tokens, Circuit& circuit, MNAMatrix& mna, const LinearSolver& solver) {
    if (tokens.empty()) return;
    std::string cmd = tokens[0];
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "add") {
        parseAddCommand(tokens, circuit);
    } else if (cmd == "delete") {
        if (tokens.size() != 2) throw std::runtime_error("Usage: delete <element_name>");
        circuit.deleteElement(tokens[1]);
        std::cout << "Deleted element: " << tokens[1] << std::endl;
    } else if (cmd == "list") {
        circuit.listElements(tokens.size() > 1 ? tokens[1] : "");
    } else if (cmd == ".nodes") {
        circuit.listNodes();
    } else if (cmd == "rename") {
        if (tokens.size() != 4 || tokens[1] != "node") throw std::runtime_error("Usage: rename node <old> <new>");
        circuit.renameNode(tokens[2], tokens[3]);
    } else if (cmd == "tran") {
        if (tokens.size() < 3 || tokens.size() > 4) {
             throw std::runtime_error("Usage: tran <Tstep> <Tstop> [UIC]");
        }
        bool use_uic = false;
        if (tokens.size() == 4) {
            std::string uic_token = tokens[3];
            transform(uic_token.begin(), uic_token.end(), uic_token.begin(), ::toupper);
            if (uic_token == "UIC") {
                use_uic = true;
            } else {
                throw std::runtime_error("Invalid option '" + tokens[3] + "'. Did you mean 'UIC'?");
            }
        }

        if (!circuit.checkGroundNodeExists()) throw std::runtime_error("No ground node detected in the circuit.");
        if (!circuit.checkConnectivity()) throw std::runtime_error("Circuit is not fully connected.");

        TransientAnalysis tran(parseValue(tokens[1]), parseValue(tokens[2]), use_uic);
        tran.analyze(circuit, mna, solver);
        tran.displayResults();
    } else if (cmd == "dc") {
        if (tokens.size() != 5) throw std::runtime_error("Usage: dc <src_name> <start> <end> <inc>");
        if (!circuit.checkGroundNodeExists()) throw std::runtime_error("No ground node detected in the circuit.");
        if (!circuit.checkConnectivity()) throw std::runtime_error("Circuit is not fully connected.");
        DCSweepAnalysis dc(tokens[1], parseValue(tokens[2]), parseValue(tokens[3]), parseValue(tokens[4]));
        dc.analyze(circuit, mna, solver);
        dc.displayResults();
    } else if (cmd == "save") {
        if (tokens.size() != 2) throw std::runtime_error("Usage: save <filename.txt>");
        circuit.saveToFile(tokens[1]);
    } else {
        throw std::runtime_error("Unknown command: '" + cmd + "'.");
    }
}

void InputParser::parseFile(const std::string& file_path, Circuit& circuit, MNAMatrix& mna, const LinearSolver& solver) {
    std::ifstream infile(file_path);
    if (!infile.is_open()) throw std::runtime_error("Could not open file: " + file_path);

    std::string line;
    std::cout << "Loading circuit from: " << file_path << std::endl;
    while (getline(infile, line)) {
        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) continue;

        std::string cmd = tokens[0];
        transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "add" || cmd == "delete" || cmd == "rename") {
            try {
                parseCommand(tokens, circuit, mna, solver);
            } catch (const std::runtime_error& e) {
                ErrorManager::displayError("In file '" + file_path + "': " + e.what());
            }
        }
    }
    std::cout << "File parsing complete." << std::endl;
}
