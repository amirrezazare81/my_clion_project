#pragma once
#include <vector>
#include <complex>
#include <map>
#include <string>
#include "Circuit.h"

// --- Type Aliases ---
using ComplexMatrix = std::vector<std::vector<std::complex<double>>>;
using ComplexVector = std::vector<std::complex<double>>;

// --- 6. Analysis and Solving Engine ---

class MNAMatrix {
private:
    // FIX: Renamed variables to match the .cpp file
    Matrix A_matrix;
    Vector b_vector;

public:
    MNAMatrix();
    void build(const Circuit& circuit, bool is_transient, double currentTime, double timeStepIncrement);
    const Matrix& getA() const;
    const Vector& getRHS() const;
    void reset();
};

class ComplexMNAMatrix {
private:
    ComplexMatrix A_matrix;
    ComplexVector b_vector;
public:
    void build(const Circuit& circuit, double omega, NodeIndexMap& node_map, std::map<std::string, int>& ac_source_map);
    const ComplexMatrix& getA() const;
    ComplexVector& getRHS();
};

class LinearSolver {
public:
    virtual ~LinearSolver() = default;
    virtual Vector solve(const Matrix& A, const Vector& b) const = 0;
};

class GaussianEliminationSolver : public LinearSolver {
public:
    Vector solve(const Matrix& A, const Vector& b) const override;
};

class LUDecompositionSolver : public LinearSolver {
public:
    Vector solve(const Matrix& A, const Vector& b) const override;
};

class ComplexLinearSolver {
public:
    ComplexVector solve(ComplexMatrix A, ComplexVector b) const;
};
