#include "Element.h"
#include "Circuit.h" // <-- FIX: Include the full definition of Circuit

// --- Cereal Polymorphic Registrations ---
// This file exists to explicitly instantiate the serialization code for each
// element type.

CEREAL_REGISTER_TYPE(Resistor)
CEREAL_REGISTER_TYPE(Capacitor)
CEREAL_REGISTER_TYPE(Inductor)
CEREAL_REGISTER_TYPE(IndependentVoltageSource)
CEREAL_REGISTER_TYPE(IndependentCurrentSource)
CEREAL_REGISTER_TYPE(PulseVoltageSource)
CEREAL_REGISTER_TYPE(SinusoidalVoltageSource)
CEREAL_REGISTER_TYPE(VoltageControlledVoltageSource)
CEREAL_REGISTER_TYPE(VoltageControlledCurrentSource)
CEREAL_REGISTER_TYPE(CurrentControlledCurrentSource)
CEREAL_REGISTER_TYPE(CurrentControlledVoltageSource)
CEREAL_REGISTER_TYPE(Diode)
CEREAL_REGISTER_TYPE(Ground)
// CEREAL_REGISTER_TYPE(Subcircuit) // Commented out due to circular dependency with Circuit
CEREAL_REGISTER_TYPE(WirelessVoltageSource)
CEREAL_REGISTER_TYPE(Wire)

CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Resistor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Capacitor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Inductor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, IndependentVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, IndependentCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, PulseVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, SinusoidalVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, VoltageControlledVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, VoltageControlledCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, CurrentControlledCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, CurrentControlledVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Diode)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Ground)
// CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Subcircuit) // Commented out due to circular dependency with Circuit
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, WirelessVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Wire)
