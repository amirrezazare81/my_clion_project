# SPICE Circuit Simulator

A comprehensive SPICE circuit simulator implementation with SDL2-based GUI, supporting both Phase 1 and Phase 2 requirements.

## Features

### Phase 1 Features
- **Basic Circuit Elements**: Resistor, Capacitor, Inductor, Diode, Ground, Voltage/Current Sources
- **Circuit Analysis**: Modified Nodal Analysis (MNA) implementation
- **Basic GUI**: SDL2-based graphical user interface
- **Circuit File Format**: Support for .sp files
- **Basic Solvers**: Gaussian elimination and LU decomposition

### Phase 2 Features
- **Advanced Analysis Types**: DC, AC, Transient analysis
- **Parameter Sweep**: DC sweep analysis with customizable ranges
- **Advanced Sources**: Sinusoidal, Pulse, Dependent sources (VCVS, VCCS, CCVS, CCCS)
- **Enhanced GUI**: Component placement, schematic editing, plotting
- **Project Serialization**: Save/load circuits in JSON format
- **Subcircuit Support**: Basic subcircuit functionality

## Circuit Elements Supported

### Passive Elements
- **Resistor (R)**: Ohmic resistance
- **Capacitor (C)**: Capacitive reactance with transient analysis
- **Inductor (L)**: Inductive reactance with transient analysis
- **Diode (D)**: Semiconductor diode with exponential model

### Independent Sources
- **Voltage Source (V)**: DC, Pulse, and Sinusoidal waveforms
- **Current Source (I)**: DC current injection

### Dependent Sources
- **VCVS (E)**: Voltage-controlled voltage source
- **VCCS (G)**: Voltage-controlled current source
- **CCVS (H)**: Current-controlled voltage source
- **CCCS (F)**: Current-controlled current source

### Special Elements
- **Ground (GND)**: Reference node (0V)
- **Wire**: Connection element
- **Subcircuit**: Hierarchical circuit blocks

## Analysis Types

### DC Analysis
- Operating point calculation
- DC sweep with parameter variation
- Support for nonlinear elements (diodes)

### AC Analysis
- Small-signal analysis
- Frequency sweep (linear and decade)
- Complex impedance calculations

### Transient Analysis
- Time-domain simulation
- Initial conditions (UIC option)
- Variable time step support

## GUI Features

### Schematic Editor
- Grid-based component placement
- Drag-and-drop wire drawing
- Component selection panel
- Node labeling system

### Simulation Control
- Analysis type selection
- Parameter configuration
- Real-time results display

### Plotting
- Multi-signal plotting
- Zoom and pan controls
- Cursor measurements
- Export capabilities

## Building the Project

### Prerequisites
- CMake 3.10 or higher
- C++17 compatible compiler
- SDL2, SDL2_image, SDL2_ttf libraries
- Cereal serialization library (included)

### Build Commands
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Dependencies
The project includes the following external libraries:
- **SDL2**: Graphics and input handling
- **SDL2_image**: Image loading support
- **SDL2_ttf**: Font rendering
- **Cereal**: JSON serialization

## Usage

### Running the Simulator
```bash
./my_clion_project
```

### GUI Operation
1. **Add Components**: Use the component selector panel
2. **Place Elements**: Click on grid positions to place components
3. **Connect Components**: Draw wires between nodes
4. **Configure Analysis**: Set simulation parameters
5. **Run Simulation**: Execute the desired analysis type
6. **View Results**: Analyze plots and numerical data

### Circuit File Format
The simulator supports SPICE-compatible netlist format:
```
* Component definitions
V1 N1 0 5.0
R1 N1 N2 1000.0
C1 N2 0 1e-6

* Analysis commands
.tran 1e-6 5e-3
.ac V1 1 100k 100
.dc V1 0 10 0.1
```

## Project Structure

```
my_clion_project/
├── src/
│   ├── Element.h/cpp          # Circuit element implementations
│   ├── Circuit.h/cpp          # Circuit management
│   ├── Solvers.h/cpp          # MNA matrix and solvers
│   ├── Analyzers.h/cpp        # Analysis engines
│   ├── GUI.h/cpp              # SDL2-based user interface
│   ├── ProjectSerializer.h/cpp # Circuit serialization
│   ├── ErrorManager.h/cpp     # Error handling
│   ├── TcpSocket.h/cpp        # Network communication
│   └── main.cpp               # Application entry point
├── assets/                    # Component textures and icons
├── CMakeLists.txt            # Build configuration
└── README.md                 # This file
```

## Implementation Details

### Modified Nodal Analysis (MNA)
- Automatic matrix construction
- Support for voltage sources and inductors
- Current variable handling
- Ground node elimination

### Solvers
- **Gaussian Elimination**: Basic linear solver
- **LU Decomposition**: Efficient matrix factorization
- **Complex Solver**: AC analysis support

### GUI Architecture
- Component-based design
- Event-driven architecture
- Real-time rendering
- Responsive interface

## Testing

The project includes several test circuits:
- Basic RC circuit
- Diode rectifier
- Dependent source examples
- Multi-node networks

## Future Enhancements

- Advanced device models (MOSFET, BJT)
- Monte Carlo analysis
- Temperature effects
- Advanced plotting features
- Scripting interface
- Multi-threaded simulation

## License

This project is part of an educational assignment for Object-Oriented Programming.

## Contributing

This is an academic project. For educational purposes, feel free to study and modify the code.

## Support

For technical issues or questions about the implementation, refer to the code comments and documentation.
