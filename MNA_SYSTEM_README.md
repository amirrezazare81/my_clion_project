# Modified Nodal Analysis (MNA) System

This document describes the complete implementation of a Modified Nodal Analysis (MNA) system for circuit analysis, including matrix construction, solving, and visualization.

## Overview

The MNA system provides a comprehensive framework for analyzing electrical circuits using the Modified Nodal Analysis method. It supports:

- **DC Analysis**: Steady-state analysis of circuits
- **Transient Analysis**: Time-domain simulation
- **AC Analysis**: Frequency-domain analysis
- **DC Sweep**: Parameter variation analysis
- **Graph Extraction**: Circuit topology analysis
- **Visualization**: Plotting and result display

## System Architecture

### Core Components

1. **Circuit Class** (`Circuit.h/cpp`)
   - Manages circuit elements and nodes
   - Handles ground node assignment
   - Provides connectivity validation

2. **Element Classes** (`Element.h/cpp`)
   - Base class for all circuit components
   - Implements `contributeToMNA` method for matrix construction
   - Supports resistors, capacitors, inductors, voltage/current sources, etc.

3. **MNA Matrix Classes** (`Solvers.h/cpp`)
   - `MNAMatrix`: DC and transient analysis matrices
   - `ComplexMNAMatrix`: AC analysis matrices
   - Automatic matrix construction from circuit description

4. **Linear Solvers** (`Solvers.h/cpp`)
   - `GaussianEliminationSolver`: Basic matrix solver
   - `LUDecompositionSolver`: Optimized solver for large matrices
   - `ComplexLinearSolver`: Complex number matrix solver

5. **Main MNA Solver** (`Solvers.h/cpp`)
   - `MNASolver`: High-level interface for all analysis types
   - Configuration management
   - Result extraction and formatting

6. **Graph Extraction** (`GraphExtractor.h/cpp`)
   - Converts circuit to graph representation
   - Validates connectivity
   - Prepares data structures for MNA

7. **Plotting System** (`Plotter.h/cpp`)
   - `CircuitPlotter`: SDL2-based visualization
   - Support for time series, frequency response, and bar charts
   - Export functionality

## MNA Matrix Construction

### Matrix Structure

The MNA system constructs matrices of the form:

```
[G  B] [v]   [J]
[B' 0] [i] = [E]
```

Where:
- **G**: Conductance matrix (modified by elements)
- **B**: Incidence matrix (voltage sources and inductors)
- **B'**: Transpose of B
- **v**: Node voltage vector
- **i**: Branch current vector
- **J**: Current source vector
- **E**: Voltage source vector

### Element Contributions

Each circuit element contributes to the MNA matrices through its `contributeToMNA` method:

#### Resistor
```cpp
void Resistor::contributeToMNA(Matrix& G, Vector& J, int num_nodes, 
                              const NodeIndexMap& node_map, ...) {
    int n1_idx = node_map.at(node1_id);
    int n2_idx = node_map.at(node2_id);
    double conductance = 1.0 / resistance;
    
    G[n1_idx][n1_idx] += conductance;
    G[n2_idx][n2_idx] += conductance;
    G[n1_idx][n2_idx] -= conductance;
    G[n2_idx][n1_idx] -= conductance;
}
```

#### Capacitor (Transient)
```cpp
void Capacitor::contributeToMNA(Matrix& G, Vector& J, int num_nodes, 
                               const NodeIndexMap& node_map, 
                               const std::map<std::string, double>& prev_voltages, 
                               bool is_transient, double timestep) {
    if (is_transient) {
        double conductance = capacitance / timestep;
        double current = conductance * (prev_voltages.at(node1_id) - prev_voltages.at(node2_id));
        
        // Add to conductance matrix
        G[n1_idx][n1_idx] += conductance;
        G[n2_idx][n2_idx] += conductance;
        G[n1_idx][n2_idx] -= conductance;
        G[n2_idx][n1_idx] -= conductance;
        
        // Add to current vector
        J[n1_idx] += current;
        J[n2_idx] -= current;
    }
}
```

#### Inductor
```cpp
void Inductor::contributeToMNA(Matrix& G, Vector& J, int num_nodes, 
                               const NodeIndexMap& node_map, 
                               const std::map<std::string, double>& prev_inductor_currents, 
                               bool is_transient, double timestep) {
    // Add to incidence matrix B
    int l_curr_idx = num_nodes + num_voltage_sources + inductor_index;
    
    B[n1_idx][l_curr_idx] += 1.0;
    B[n2_idx][l_curr_idx] -= 1.0;
    B[l_curr_idx][n1_idx] += 1.0;
    B[l_curr_idx][n2_idx] -= 1.0;
    
    if (is_transient) {
        // Add transient terms
        G[l_curr_idx][l_curr_idx] -= inductance / timestep;
        J[l_curr_idx] = -(inductance / timestep) * prev_current;
    }
}
```

## Analysis Types

### 1. DC Analysis

```cpp
MNASolver solver;
auto result = solver.solveDC(circuit);

if (result.success) {
    // Access node voltages
    for (const auto& [node_id, voltage] : result.node_voltages) {
        std::cout << node_id << ": " << voltage << " V" << std::endl;
    }
    
    // Access branch currents
    for (const auto& [branch_name, current] : result.branch_currents) {
        std::cout << branch_name << ": " << current << " A" << std::endl;
    }
}
```

### 2. Transient Analysis

```cpp
MNASolverConfig config;
config.transient_timestep = 1e-6;    // 1 microsecond
config.transient_end_time = 1e-3;    // 1 millisecond

MNASolver solver(config);
auto result = solver.solveTransient(circuit);

if (result.success) {
    // Access time series data
    for (const auto& [series_name, values] : result.time_series) {
        std::cout << series_name << ": " << values.size() << " time points" << std::endl;
    }
}
```

### 3. AC Analysis

```cpp
MNASolverConfig config;
config.ac_start_freq = 1.0;      // 1 Hz
config.ac_end_freq = 1e6;        // 1 MHz
config.ac_points = 100;          // 100 frequency points

MNASolver solver(config);
auto result = solver.solveAC(circuit);

if (result.success) {
    // Access frequency response data
    for (const auto& [series_name, values] : result.frequency_series) {
        std::cout << series_name << ": " << values.size() << " frequency points" << std::endl;
    }
}
```

### 4. DC Sweep

```cpp
MNASolver solver;
auto result = solver.solveDCSweep(circuit, "R1", 100.0, 10000.0, 50);

if (result.success) {
    // Access sweep data
    for (const auto& [series_name, values] : result.time_series) {
        std::cout << series_name << ": " << values.size() << " sweep points" << std::endl;
    }
}
```

## Graph Extraction and Validation

### Circuit Graph Structure

```cpp
struct CircuitNode {
    std::string id;
    std::string label;
    bool is_ground;
    std::vector<std::string> connected_elements;
};

struct CircuitEdge {
    std::string id;
    std::string element_name;
    std::string node1_id;
    std::string node2_id;
    std::string element_type;
    double value;
};

struct CircuitGraph {
    std::vector<CircuitNode> nodes;
    std::vector<CircuitEdge> edges;
};
```

### Graph Analysis

```cpp
GraphExtractor extractor(circuit);

// Extract graph
auto graph = extractor.extractGraph();

// Check connectivity
if (extractor.isGraphConnected()) {
    std::cout << "Circuit is fully connected" << std::endl;
}

// Find isolated nodes
auto isolated = extractor.findIsolatedNodes();
if (!isolated.empty()) {
    std::cout << "Isolated nodes found: ";
    for (const auto& node : isolated) {
        std::cout << node << " ";
    }
    std::cout << std::endl;
}

// Check for short circuits
auto short_circuits = extractor.findShortCircuits();
if (!short_circuits.empty()) {
    std::cout << "Short circuits detected" << std::endl;
}

// Create node index map for MNA
auto node_map = extractor.createNodeIndexMap();
```

## Visualization and Plotting

### Plot Types

1. **Time Series**: Transient analysis results
2. **Frequency Response**: AC analysis results
3. **DC Sweep**: Parameter variation results
4. **Bar Charts**: Node voltages and branch currents

### Basic Plotting

```cpp
PlotConfig config;
config.width = 800;
config.height = 600;
config.title = "Circuit Analysis Results";

CircuitPlotter plotter(config);

// Plot DC results
plotter.plotDCResults(result);

// Plot transient results
plotter.plotTransientResults(result);

// Plot AC results
plotter.plotACResults(result);

// Plot DC sweep results
plotter.plotDCSweepResults(result);
```

### Custom Plotting

```cpp
// Create time series plot
std::vector<PlotData> data;
PlotData series("Node V1", 255, 0, 0);  // Red color
series.addPoint(0.0, 0.0);
series.addPoint(1e-3, 5.0);
series.addPoint(2e-3, 5.0);
data.push_back(series);

plotter.plotTimeSeries(data, "Custom Time Series");

// Create bar chart
std::map<std::string, double> bar_data;
bar_data["V1"] = 5.0;
bar_data["V2"] = 3.0;
bar_data["V3"] = 1.0;

plotter.plotBarChart(bar_data, "Node Voltages");
```

## Configuration and Performance

### Solver Configuration

```cpp
MNASolverConfig config;
config.tolerance = 1e-12;                    // Matrix solution tolerance
config.max_iterations = 1000;                // Maximum iterations
config.use_lu_decomposition = true;          // Use LU decomposition
config.transient_timestep = 1e-6;           // Transient analysis timestep
config.transient_end_time = 1e-3;           // Transient analysis end time
config.ac_start_freq = 1.0;                 // AC analysis start frequency
config.ac_end_freq = 1e6;                   // AC analysis end frequency
config.ac_points = 100;                     // AC analysis frequency points

MNASolver solver(config);
```

### Performance Optimization

1. **LU Decomposition**: Faster than Gaussian elimination for large matrices
2. **Sparse Matrix**: Efficient storage for large circuits
3. **Parallel Processing**: Multi-threaded matrix operations (future enhancement)

## Example Circuits

### 1. Simple RC Circuit

```cpp
Circuit circuit;
circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "V1", "GND", 5.0));
circuit.addElement(std::make_unique<Resistor>("R1", "V1", "OUT", 1000.0));
circuit.addElement(std::make_unique<Capacitor>("C1", "OUT", "GND", 1e-6));
circuit.setGroundNode("GND");

MNASolver solver;
auto result = solver.solveDC(circuit);
```

### 2. Voltage Divider

```cpp
Circuit circuit;
circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "V1", "GND", 10.0));
circuit.addElement(std::make_unique<Resistor>("R1", "V1", "MID", 1000.0));
circuit.addElement(std::make_unique<Resistor>("R2", "MID", "GND", 2000.0));
circuit.setGroundNode("GND");

MNASolver solver;
auto result = solver.solveDC(circuit);
```

### 3. RLC Circuit

```cpp
Circuit circuit;
circuit.addElement(std::make_unique<IndependentVoltageSource>("V1", "V1", "GND", 12.0));
circuit.addElement(std::make_unique<Resistor>("R1", "V1", "OUT", 100.0));
circuit.addElement(std::make_unique<Inductor>("L1", "OUT", "MID", 0.001));
circuit.addElement(std::make_unique<Capacitor>("C1", "MID", "GND", 1e-6));
circuit.setGroundNode("GND");

MNASolver solver;
auto result = solver.solveTransient(circuit);
```

## Testing and Validation

### Running Tests

```bash
# Build the project
cmake --build cmake-build-debug --target test_mna_system

# Run the MNA system test
./cmake-build-debug/test_mna_system
```

### Test Coverage

The test program validates:

1. **DC Analysis**: Simple circuits with known solutions
2. **Transient Analysis**: Time-domain behavior
3. **AC Analysis**: Frequency response
4. **DC Sweep**: Parameter variation
5. **Graph Extraction**: Circuit topology analysis
6. **Plotting**: Visualization system
7. **Performance**: Large circuit handling

## Error Handling

### Common Errors

1. **Singular Matrix**: Circuit has floating nodes or short circuits
2. **Convergence Failure**: Transient analysis timestep too large
3. **Invalid Circuit**: Missing ground node or disconnected components

### Error Recovery

```cpp
try {
    auto result = solver.solve(circuit, AnalysisType::DC_ANALYSIS);
    if (!result.success) {
        std::cout << "Analysis failed: " << result.error_message << std::endl;
        // Handle error
    }
} catch (const std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl;
    // Handle exception
}
```

## Future Enhancements

### Planned Features

1. **Sparse Matrix Support**: Efficient storage for large circuits
2. **Parallel Processing**: Multi-threaded matrix operations
3. **Advanced Elements**: MOSFETs, BJTs, operational amplifiers
4. **Monte Carlo Analysis**: Statistical circuit analysis
5. **Sensitivity Analysis**: Parameter sensitivity calculations
6. **Optimization**: Automatic component value optimization

### Performance Improvements

1. **Matrix Reordering**: Optimize matrix structure for solving
2. **Adaptive Timestep**: Dynamic timestep adjustment in transient analysis
3. **Caching**: Cache matrix factorizations for repeated solves

## Conclusion

The MNA system provides a robust, efficient, and extensible framework for circuit analysis. It supports all major analysis types, includes comprehensive validation, and offers powerful visualization capabilities. The modular design makes it easy to extend with new element types and analysis methods.

For questions or contributions, please refer to the main project documentation or contact the development team.
