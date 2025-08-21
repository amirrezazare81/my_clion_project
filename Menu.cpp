#include "Menu.h"
#include "Solvers.h"
#include "ErrorManager.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

Menu::Menu() : solver(std::make_unique<LUDecompositionSolver>()) {}

void Menu::run() {
    std::string line;
    while (true) {
        std::cout << "\n> ";
        if (!getline(std::cin, line)) break;

        std::vector<std::string> tokens = parser.tokenize(line);
        if (tokens.empty()) continue;

        std::string cmd = tokens[0];
        transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting simulator. Goodbye!" << std::endl;
            break;
        } else if (cmd == "help") {
            displayMainMenu();
        } else if (cmd == "newfile") {
            if (tokens.size() != 2) { ErrorManager::displayError("Usage: newfile <path/to/file.txt>"); continue; }
            try {
                circuit.clear();
                parser.parseFile(tokens[1], circuit, mna_matrix, *solver);
            } catch (const std::runtime_error& e) { ErrorManager::displayError(e.what()); }
        } else if (cmd == "solver") {
            chooseSolver();
        }
        else {
            try {
                parser.parseCommand(tokens, circuit, mna_matrix, *solver);
            } catch (const std::runtime_error& e) {
                ErrorManager::displayError(e.what());
            }
        }
    }
}

void Menu::displayMainMenu() const {
    std::cout << "--- Circuit Simulator Help ---" << std::endl;
    std::cout << "  - add <element> ...      : Add an element (R, C, L, I, V, D, E, GND)." << std::endl;
    std::cout << "  - delete <name>          : Delete an element by its name." << std::endl;
    std::cout << "  - list [type]            : List all elements or filter by type." << std::endl;
    std::cout << "  - .nodes                 : List all nodes in the circuit." << std::endl;
    std::cout << "  - rename node <old> <new>: Rename a node." << std::endl;
    std::cout << "  - tran <Tstep> <Tstop> [UIC] : Run transient analysis. UIC is optional." << std::endl;
    std::cout << "  - dc <src> <start> <end> <inc>: Run DC sweep analysis." << std::endl;
    std::cout << "  - newfile <path>         : Load a circuit from a file." << std::endl;
    std::cout << "  - save <path>            : Save the current circuit to a file." << std::endl;
    std::cout << "  - solver                 : Choose the linear equation solver." << std::endl;
    std::cout << "  - help                   : Display this help menu." << std::endl;
    std::cout << "  - exit / quit            : Exit the simulator." << std::endl;
    std::cout << "------------------------------" << std::endl;
}

void Menu::chooseSolver() {
    std::string choice;
    std::cout << "Choose solver (1: LU, 2: Gaussian): ";
    getline(std::cin, choice);
    if (choice == "1") {
        solver = std::make_unique<LUDecompositionSolver>();
        std::cout << "Solver set to LU Decomposition." << std::endl;
    } else if (choice == "2") {
        solver = std::make_unique<GaussianEliminationSolver>();
        std::cout << "Solver set to Gaussian Elimination." << std::endl;
    } else {
        ErrorManager::displayError("Invalid choice. No change made.");
    }
}
