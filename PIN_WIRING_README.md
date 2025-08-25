# Pin-Based Circuit Wiring System

This document describes the new pin-based wiring system implemented in the circuit simulator, which provides a more intuitive and accurate way to connect circuit components.

## Overview

The pin-based wiring system replaces the old node-based connection system with a visual pin interface where:
- Each circuit component has **pins** (connection points)
- **Wires** connect pins together to create electrical connections
- The system automatically generates the circuit graph for MNA analysis

## Key Components

### 1. Pin Class (`Pin.h`, `Pin.cpp`)
Represents a connection point on a circuit element.

**Features:**
- Unique identifier and element association
- Position tracking (x, y coordinates)
- Connection status (connected/disconnected)
- Node ID mapping for electrical analysis
- Wire management (add/remove connected wires)

**Usage:**
```cpp
auto pin = std::make_shared<Pin>("pin_id", "element_name", pin_number, SDL_Point{x, y});
pin->setNodeId("node_id");
```

### 2. Wire Class (`Wire.h`, `Wire.cpp`)
Represents an electrical connection between two pins.

**Features:**
- Start and end pin connections
- Waypoint support for complex routing
- Selection state for GUI interaction
- Automatic node ID generation
- Serialization support

**Usage:**
```cpp
auto wire = std::make_shared<Wire>("wire_id", start_pin, end_pin);
wire->addWaypoint(SDL_Point{x, y});
```

### 3. GraphExtractor Class (`GraphExtractor.h`, `GraphExtractor.cpp`)
Extracts the circuit graph structure from pins and wires for MNA analysis.

**Features:**
- Automatic pin and wire extraction from circuit elements
- Circuit graph construction (nodes and edges)
- Connectivity analysis
- MNA matrix preparation
- Graph validation and export

**Usage:**
```cpp
GraphExtractor extractor(circuit);
CircuitGraph graph = extractor.extractGraph();
auto node_map = extractor.createNodeIndexMap();
```

## Circuit Graph Structure

The system creates a mathematical graph representation where:

- **Nodes (Vertices)**: Electrical connection points
  - Properties: ID, voltage, ground status, connected elements
  - Automatically merged when connected by wires

- **Edges**: Circuit elements (resistors, capacitors, etc.)
  - Properties: element type, value, node connections
  - Contribute to MNA matrix construction

## GUI Integration

### Pin Visualization
- Pins are displayed as small circles on components
- Green: Connected pins
- Yellow: Unconnected pins
- Clickable for wire creation

### Wire Creation
1. Click "Pin Wire" button in component selector
2. Click on first pin (starts wire creation)
3. Click on second pin (completes wire)
4. Wire is automatically created and connected

### Wire Management
- Wires are drawn as blue lines between pins
- Selected wires appear in yellow
- Waypoints can be added for complex routing
- Wires can be deleted by selecting and pressing Delete

## MNA Integration

The pin-based system seamlessly integrates with the existing Modified Nodal Analysis (MNA) system:

1. **Graph Extraction**: Pins and wires automatically generate the circuit graph
2. **Matrix Construction**: Graph structure is used to build the MNA matrix
3. **Analysis**: Standard DC, AC, and transient analysis work unchanged
4. **Results**: Node voltages and element currents are calculated normally

## Example Usage

### Creating a Simple RC Circuit

```cpp
// Create components
auto v1 = std::make_unique<IndependentVoltageSource>("V1", "n1", "gnd", 5.0);
auto r1 = std::make_unique<Resistor>("R1", "n1", "n2", 1000.0);
auto c1 = std::make_unique<Capacitor>("C1", "n2", "gnd", 1e-6);

// Add to circuit
circuit->addElement(std::move(v1));
circuit->addElement(std::move(r1));
circuit->addElement(std::move(c1));

// Create pins (automatically done by GUI)
// Create wires between pins (done by user clicking)
// Extract graph for analysis
GraphExtractor extractor(*circuit);
CircuitGraph graph = extractor.extractGraph();
```

## Benefits

1. **Visual Clarity**: Pins make connections obvious and intuitive
2. **Error Prevention**: Prevents invalid connections
3. **Automatic Graph Generation**: No manual node mapping required
4. **Scalability**: Works with complex circuits and subcircuits
5. **MNA Compatibility**: Integrates with existing analysis engine

## Testing

A comprehensive test program is provided in `test_pin_wiring.cpp` that demonstrates:

- Pin creation and management
- Wire creation and routing
- Graph extraction and analysis
- MNA matrix preparation
- Graph export functionality

To run the test:
```bash
cmake --build . --target test_pin_wiring
./test_pin_wiring
```

## Future Enhancements

1. **Pin Types**: Support for different pin types (input, output, bidirectional)
2. **Wire Styles**: Different wire styles for different connection types
3. **Subcircuit Support**: Pin mapping for hierarchical circuits
4. **Validation**: Enhanced connection validation and error reporting
5. **Undo/Redo**: Wire creation and deletion history

## Technical Notes

- Pins use SDL coordinates for GUI positioning
- Wires automatically generate unique node IDs for MNA
- The system maintains backward compatibility with existing circuits
- Graph extraction is performed on-demand for analysis
- All pin and wire data is serializable for project saving/loading
