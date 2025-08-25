#include <Wire.h>

#include "Element.h"
#include "Circuit.h" // <-- FIX: Include the full definition of Circuit
#include "Probe.h"

// --- Cereal Polymorphic Registrations ---
// This file exists to explicitly instantiate the serialization code for each
// element type.

CEREAL_REGISTER_TYPE(Resistor)
CEREAL_REGISTER_TYPE(Capacitor)
CEREAL_REGISTER_TYPE(Inductor)
CEREAL_REGISTER_TYPE(IndependentVoltageSource)
CEREAL_REGISTER_TYPE(IndependentCurrentSource)
CEREAL_REGISTER_TYPE(PulseVoltageSource)
CEREAL_REGISTER_TYPE(WaveformVoltageSource)
CEREAL_REGISTER_TYPE(PhaseVoltageSource)
CEREAL_REGISTER_TYPE(SinusoidalVoltageSource)
CEREAL_REGISTER_TYPE(ACVoltageSource)

CEREAL_REGISTER_TYPE(PulseCurrentSource)
CEREAL_REGISTER_TYPE(VoltageControlledVoltageSource)  // Temporarily disabled to fix linker issues
CEREAL_REGISTER_TYPE(VoltageControlledCurrentSource) // Temporarily disabled to fix linker issues
CEREAL_REGISTER_TYPE(CurrentControlledCurrentSource) // Temporarily disabled to fix linker issues
CEREAL_REGISTER_TYPE(CurrentControlledVoltageSource) // Temporarily disabled to fix linker issues
CEREAL_REGISTER_TYPE(Diode)                          // Temporarily disabled to fix linker issues
CEREAL_REGISTER_TYPE(Ground)                         // Temporarily disabled to fix linker issues
// CEREAL_REGISTER_TYPE(Subcircuit) // Commented out due to circular dependency with Circuit
CEREAL_REGISTER_TYPE(CircuitWire)
CEREAL_REGISTER_TYPE(WirelessVoltageSource)
CEREAL_REGISTER_TYPE(GuiWire)

// Probe types
CEREAL_REGISTER_TYPE(VoltageProbe)
CEREAL_REGISTER_TYPE(CurrentProbe)
CEREAL_REGISTER_TYPE(PowerProbe)
CEREAL_REGISTER_TYPE(DifferentialProbe)

CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Resistor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Capacitor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Inductor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, IndependentVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, IndependentCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, PulseVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, WaveformVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, PhaseVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, SinusoidalVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, ACVoltageSource)

CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, PulseCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, VoltageControlledVoltageSource)  // Temporarily disabled
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, VoltageControlledCurrentSource) // Temporarily disabled
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, CurrentControlledCurrentSource) // Temporarily disabled
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, CurrentControlledVoltageSource) // Temporarily disabled
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Diode)                          // Temporarily disabled
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Ground)                         // Temporarily disabled
// CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Subcircuit) // Commented out due to circular dependency with Circuit
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, CircuitWire)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, WirelessVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, GuiWire)

// Probe polymorphic relations
CEREAL_REGISTER_POLYMORPHIC_RELATION(Probe, VoltageProbe)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Probe, CurrentProbe)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Probe, PowerProbe)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Probe, DifferentialProbe)
