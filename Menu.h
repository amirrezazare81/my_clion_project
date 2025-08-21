#pragma once
#include "Circuit.h"
#include "Solvers.h" // <-- FIX: Changed from MNAMatrix.h
#include "InputParser.h"
#include <memory>

// Forward declaration
class LinearSolver;

class Menu {
private:
    Circuit circuit;
    MNAMatrix mna_matrix;
    std::unique_ptr<LinearSolver> solver;
    InputParser parser;

public:
    Menu();
    void run();
private:
    void displayMainMenu() const;
    void chooseSolver();
};
