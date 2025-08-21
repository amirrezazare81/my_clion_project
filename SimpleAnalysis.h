#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>
#include <complex>
#include <iostream>
#include <array>
#include "Element.h"

using namespace std;
using cd = complex<double>;

// Forward declarations
class Circuit;

// PlotColor for plotting (SDL-independent definition)
struct PlotColor {
    unsigned char r, g, b, a;
};

struct SignalView {
    const vector<double>* x;
    const vector<double>* y;
    PlotColor color;
};

// 10-color palette (RGBA)
const array<PlotColor, 10> kPalette = {{
    PlotColor{230, 25,  75, 255},  // red
    PlotColor{60,  180, 75, 255},  // green
    PlotColor{0,   130, 200,255},  // blue
    PlotColor{245, 130, 48, 255},  // orange
    PlotColor{145, 30,  180,255},  // purple
    PlotColor{70,  240, 240,255},  // cyan
    PlotColor{240, 50,  230,255},  // magenta
    PlotColor{210, 245, 60, 255},  // lime
    PlotColor{250, 190, 190,255},  // pink
    PlotColor{0,   128, 128,255},  // teal
}};

// Type definitions
struct TransientSeries {
    vector<double> t;
    vector<vector<double>> ys;
    vector<string> names;
    vector<SignalView> series;  // views for plotting
};

struct ACSweepVals {
    vector<double> freq;
    vector<vector<double>> ys;
    vector<string> names;
};

// Simplified analysis classes based on other.cpp logic

class MNASystem {
private:
    vector<vector<double>> mat;
    vector<double> rhs;
    vector<double> solution;
    unordered_map<string, int> nodeIndexMap;
    unordered_map<string, int> vsIndexMap;
    vector<string> nodeNames;
    vector<string> vsNames;
    unordered_map<string, string> nodeEquivalence; // Maps original nodes to representative nodes
    int n, m, size;

public:
    MNASystem(const Circuit& circuit);

    void reset();
    void stampResistor(const string& n1, const string& n2, double R);
    void stampCurrentSource(const string& n1, const string& n2, double I);
    void stampVoltageSource(const string& name, const string& n1, const string& n2, double V);
    void stampCapacitor(const string& n1, const string& n2, double C, double dt);
    void stampInductor(const string& n1, const string& n2, double L, double dt);
    bool solveGaussian();
    double getNodeVoltage(const string& node);
    double getCurrentThroughVS(const string& vsName);
    void printSolution();

    // Getters for debugging
    int getNumNodes() const { return n; }
    int getNumVoltageSources() const { return m; }
    int getSystemSize() const { return size; }
    const vector<vector<double>>& getMatrix() const { return mat; }
    const vector<double>& getRHS() const { return rhs; }
    const vector<double>& getSolution() const { return solution; }
    const unordered_map<string, int>& getNodeIndexMap() const { return nodeIndexMap; }
    const unordered_map<string, int>& getVSIndexMap() const { return vsIndexMap; }
};

class ComplexMNASystem {
private:
    vector<vector<cd>> mat;
    vector<cd> rhs;
    vector<cd> solution;
    unordered_map<string, int> nodeIndexMap;
    unordered_map<string, int> vsIndexMap;
    vector<string> nodeNames;
    vector<string> vsNames;
    int n, m, size;

public:
    ComplexMNASystem(const Circuit& circuit);

    void reset();
    void stampImpedance(const string& n1, const string& n2, cd Z);
    void stampCurrentSource(const string& n1, const string& n2, cd I);
    void stampVoltageSource(const string& name, const string& n1, const string& n2, cd V);
    bool solveGaussian();
    cd getNodeVoltageComplex(const string& node) const;
    cd getCurrentThroughVSComplex(const string& vsName) const;
};

class TransientSimulator {
private:
    unordered_map<string, double> capVoltages;
    unordered_map<string, double> indCurrents;

public:
    void init(const Circuit& circuit);
    void update(const Circuit& circuit, MNASystem& sys, double dt);
    double getCapCurrent(const string& name, double C, double dt);
    double getIndCurrent(const string& name);

    // Getters for debugging
    const unordered_map<string, double>& getCapVoltages() const { return capVoltages; }
    const unordered_map<string, double>& getIndCurrents() const { return indCurrents; }
};

#include "Circuit.h"

// Circuit analysis methods
class CircuitAnalyzer {
public:
    static bool analyzeDC(Circuit& circuit);
    static TransientSeries analyzeTransientMulti(Circuit& circuit, double t_step, double t_total, const vector<string>& desiredSignals);
    static ACSweepVals ACsweep(Circuit& circuit, double startOmega, double stopOmega, int stepCount, string desiredSignal);
    static vector<double> getElementCurrent(const Element* elem, MNASystem& sys, double t_step);
};
