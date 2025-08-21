#include "Element.h"
#include "Circuit.h"
#include "TcpSocket.h"
#include "ErrorManager.h"
#include <stdexcept>
#include <cmath>
#include <limits>
#include <utility>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Element Base Class Implementation ---
Element::Element() : name(""), node1_id(""), node2_id("") {}

Element::Element(const std::string& name, const std::string& node1, const std::string& node2)
    : name(name), node1_id(node1), node2_id(node2) {}

std::string Element::getName() const { return name; }
std::string Element::getNode1Id() const { return node1_id; }
std::string Element::getNode2Id() const { return node2_id; }
void Element::setNode1Id(const std::string& new_id) { node1_id = new_id; }
void Element::setNode2Id(const std::string& new_id) { node2_id = new_id; }

// --- Wire Implementation ---
Wire::Wire() : Element() {}

Wire::Wire(const std::string& name, const std::string& node1, const std::string& node2)
    : Element(name, node1, node2) {}

std::string Wire::getType() const { return "Wire"; }
double Wire::getValue() const { return 0.0; }
std::string Wire::getAddCommandString() const {
    return "* wire " + getName() + " " + getNode1Id() + " " + getNode2Id();
}
void Wire::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map,
                           const std::map<std::string, double>& prev_node_voltages,
                           bool is_transient, double timestep) {
    // Wires are handled by node connections and do not contribute directly to the matrix.
}

// --- Resistor Implementation ---
Resistor::Resistor() : Element(), resistance(0.0) {}

Resistor::Resistor(const std::string& name, const std::string& node1, const std::string& node2, double value)
    : Element(name, node1, node2), resistance(value) {}

std::string Resistor::getType() const { return "Resistor"; }
double Resistor::getValue() const { return resistance; }
std::string Resistor::getAddCommandString() const {
    return "R " + name + " " + node1_id + " " + node2_id + " " + std::to_string(resistance);
}

void Resistor::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        double conductance = 1.0 / resistance;
        G[n1][n1] += conductance;
        G[n2][n2] += conductance;
        G[n1][n2] -= conductance;
        G[n2][n1] -= conductance;
    }
}

// --- Capacitor Implementation ---
Capacitor::Capacitor() : Element(), capacitance(0.0) {}

Capacitor::Capacitor(const std::string& name, const std::string& node1, const std::string& node2, double value)
    : Element(name, node1, node2), capacitance(value) {}

std::string Capacitor::getType() const { return "Capacitor"; }
double Capacitor::getValue() const { return capacitance; }
std::string Capacitor::getAddCommandString() const {
    return "C " + name + " " + node1_id + " " + node2_id + " " + std::to_string(capacitance);
}

void Capacitor::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>& prev_voltages, bool is_transient, double timestep) {
    if (!is_transient || timestep <= 0) return;

    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        if (is_transient && timestep > 0) {
            // For transient analysis, use backward Euler
            double conductance = capacitance / timestep;
            G[n1][n1] += conductance;
            G[n2][n2] += conductance;
            G[n1][n2] -= conductance;
            G[n2][n1] -= conductance;

            // Add current source term from previous timestep
            double v1_prev = prev_voltages.count(node1_id) ? prev_voltages.at(node1_id) : 0.0;
            double v2_prev = prev_voltages.count(node2_id) ? prev_voltages.at(node2_id) : 0.0;
            double i_prev = conductance * (v1_prev - v2_prev);
            if (n1 != -1) J[n1] += i_prev;
            if (n2 != -1) J[n2] -= i_prev;
        }
        // For DC analysis, capacitor acts as an open circuit (no contribution)
    }
}

// --- Inductor Implementation ---
Inductor::Inductor() : Element(), inductance(0.0) {}

Inductor::Inductor(const std::string& name, const std::string& node1, const std::string& node2, double value)
    : Element(name, node1, node2), inductance(value) {}

std::string Inductor::getType() const { return "Inductor"; }
double Inductor::getValue() const { return inductance; }
std::string Inductor::getAddCommandString() const {
    return "L " + name + " " + node1_id + " " + node2_id + " " + std::to_string(inductance);
}

void Inductor::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>& prev_inductor_currents, bool is_transient, double timestep) {
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        if (is_transient && timestep > 0) {
            // For transient analysis, use backward Euler
            double conductance = timestep / inductance;
            G[n1][n1] += conductance;
            G[n2][n2] += conductance;
            G[n1][n2] -= conductance;
            G[n2][n1] -= conductance;

            // Add current source term from previous timestep
            double prev_current = prev_inductor_currents.count(name) ? prev_inductor_currents.at(name) : 0.0;
            if (n1 != -1) J[n1] += conductance * prev_current;
            if (n2 != -1) J[n2] -= conductance * prev_current;
        }
        // For DC analysis, inductor acts as a short circuit (handled by MNA matrix building)
    }
}

// --- Independent Voltage Source Implementation ---
IndependentVoltageSource::IndependentVoltageSource() : Element(), voltage_value(0.0) {}

IndependentVoltageSource::IndependentVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double value)
    : Element(name, node1, node2), voltage_value(value) {}

std::string IndependentVoltageSource::getType() const { return "IndependentVoltageSource"; }
double IndependentVoltageSource::getValue() const { return voltage_value; }
void IndependentVoltageSource::setValue(double new_value) { voltage_value = new_value; }
std::string IndependentVoltageSource::getAddCommandString() const {
    return "V " + name + " " + node1_id + " " + node2_id + " " + std::to_string(voltage_value);
}

void IndependentVoltageSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    // For voltage sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        // The voltage difference constraint will be handled in MNAMatrix::build
    }
}

// --- Independent Current Source Implementation ---
IndependentCurrentSource::IndependentCurrentSource() : Element(), current_value(0.0) {}

IndependentCurrentSource::IndependentCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, double value)
    : Element(name, node1, node2), current_value(value) {}

std::string IndependentCurrentSource::getType() const { return "IndependentCurrentSource"; }
double IndependentCurrentSource::getValue() const { return current_value; }
void IndependentCurrentSource::setValue(double new_value) { current_value = new_value; }
std::string IndependentCurrentSource::getAddCommandString() const {
    return "I " + name + " " + node1_id + " " + node2_id + " " + std::to_string(current_value);
}

void IndependentCurrentSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1) J[n1] -= current_value;
    if (n2 != -1) J[n2] += current_value;
}

// --- Pulse Voltage Source Implementation ---
PulseVoltageSource::PulseVoltageSource() : Element(), v1_val(0.0), v2_val(0.0), td_val(0.0), tr_val(0.0), tf_val(0.0), pw_val(0.0), per_val(0.0) {}

PulseVoltageSource::PulseVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double V1_val, double V2_val, double TD_val, double TR_val, double TF_val, double PW_val, double PER_val)
    : Element(name, node1, node2), v1_val(V1_val), v2_val(V2_val), td_val(TD_val), tr_val(TR_val), tf_val(TF_val), pw_val(PW_val), per_val(PER_val) {}
std::string PulseVoltageSource::getType() const { return "PulseVoltageSource"; }
double PulseVoltageSource::getValue() const { return v1_val; }
void PulseVoltageSource::setV1(double new_v1) { v1_val = new_v1; }
std::string PulseVoltageSource::getAddCommandString() const {
    return "Vpulse " + name + " " + node1_id + " " + node2_id + " " + std::to_string(v1_val) + " " + std::to_string(v2_val) + " " + std::to_string(td_val) + " " + std::to_string(tr_val) + " " + std::to_string(tf_val) + " " + std::to_string(pw_val) + " " + std::to_string(per_val);
}

double PulseVoltageSource::getVoltageAtTime(double current_time) const {
    if (current_time < td_val) return v1_val;
    
    double t_cycle = fmod(current_time - td_val, per_val);
    if (t_cycle < tr_val) {
        // Rising edge
        return v1_val + (v2_val - v1_val) * (t_cycle / tr_val);
    } else if (t_cycle < tr_val + pw_val) {
        // High level
        return v2_val;
    } else if (t_cycle < tr_val + pw_val + tf_val) {
        // Falling edge
        double t_fall = t_cycle - tr_val - pw_val;
        return v2_val + (v1_val - v2_val) * (t_fall / tf_val);
    } else {
        // Low level
        return v1_val;
    }
}
void PulseVoltageSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    // For time-dependent voltage sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;
    
    if (n1 != -1 && n2 != -1) {
        // The voltage difference constraint will be handled in MNAMatrix::build
    }
}

// --- Sinusoidal Voltage Source Implementation ---
SinusoidalVoltageSource::SinusoidalVoltageSource() : Element(), dc_offset(0.0), amplitude(0.0), frequency(0.0) {}

SinusoidalVoltageSource::SinusoidalVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double offset, double amp, double freq)
    : Element(name, node1, node2), dc_offset(offset), amplitude(amp), frequency(freq) {}

std::string SinusoidalVoltageSource::getType() const { return "SinusoidalVoltageSource"; }
double SinusoidalVoltageSource::getValue() const { return dc_offset; }
void SinusoidalVoltageSource::setDCOffset(double new_offset) { dc_offset = new_offset; }
std::string SinusoidalVoltageSource::getAddCommandString() const {
    return "Vsin " + name + " " + node1_id + " " + node2_id + " " + std::to_string(dc_offset) + " " + std::to_string(amplitude) + " " + std::to_string(frequency);
}

double SinusoidalVoltageSource::getVoltageAtTime(double current_time) const {
    if (frequency <= 0) return dc_offset;
    return dc_offset + amplitude * std::sin(2.0 * M_PI * frequency * current_time);
}

void SinusoidalVoltageSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    // For time-dependent voltage sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        // The voltage difference constraint will be handled in MNAMatrix::build
    }
}

// --- Voltage Controlled Voltage Source (VCVS) Implementation ---
VoltageControlledVoltageSource::VoltageControlledVoltageSource() : Element(), gain(0.0) {}

VoltageControlledVoltageSource::VoltageControlledVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_node1, const std::string& ctrl_node2, double g)
    : Element(name, node1, node2), control_node1_id(ctrl_node1), control_node2_id(ctrl_node2), gain(g) {}

std::string VoltageControlledVoltageSource::getType() const { return "VoltageControlledVoltageSource"; }
double VoltageControlledVoltageSource::getValue() const { return gain; }
std::string VoltageControlledVoltageSource::getAddCommandString() const {
    return "VCVS " + name + " " + node1_id + " " + node2_id + " " + control_node1_id + " " + control_node2_id + " " + std::to_string(gain);
}

std::string VoltageControlledVoltageSource::getControlNode1Id() const { return control_node1_id; }
std::string VoltageControlledVoltageSource::getControlNode2Id() const { return control_node2_id; }
double VoltageControlledVoltageSource::getGain() const { return gain; }

void VoltageControlledVoltageSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    // For dependent voltage sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;
    int cn1 = node_map.count(control_node1_id) ? node_map.at(control_node1_id) : -1;
    int cn2 = node_map.count(control_node2_id) ? node_map.at(control_node2_id) : -1;
    
    if (n1 != -1 && n2 != -1 && cn1 != -1 && cn2 != -1) {
        // The voltage constraint will be handled in MNAMatrix::build
    }
}

// --- Voltage Controlled Current Source (VCCS) Implementation ---
VoltageControlledCurrentSource::VoltageControlledCurrentSource() : Element(), transconductance(0.0) {}

VoltageControlledCurrentSource::VoltageControlledCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_node1, const std::string& ctrl_node2, double g)
    : Element(name, node1, node2), control_node1_id(ctrl_node1), control_node2_id(ctrl_node2), transconductance(g) {}

std::string VoltageControlledCurrentSource::getType() const { return "VoltageControlledCurrentSource"; }
double VoltageControlledCurrentSource::getValue() const { return transconductance; }
std::string VoltageControlledCurrentSource::getAddCommandString() const {
    return "VCCS " + name + " " + node1_id + " " + node2_id + " " + control_node1_id + " " + control_node2_id + " " + std::to_string(transconductance);
}

std::string VoltageControlledCurrentSource::getControlNode1Id() const { return control_node1_id; }
std::string VoltageControlledCurrentSource::getControlNode2Id() const { return control_node2_id; }
double VoltageControlledCurrentSource::getTransconductance() const { return transconductance; }

void VoltageControlledCurrentSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;
    int ctrl_n1 = node_map.count(control_node1_id) ? node_map.at(control_node1_id) : -1;
    int ctrl_n2 = node_map.count(control_node2_id) ? node_map.at(control_node2_id) : -1;

    if (n1 != -1 && n2 != -1 && ctrl_n1 != -1 && ctrl_n2 != -1) {
        // Current from node1 to node2 = transconductance * (V_ctrl1 - V_ctrl2)
        if (n1 != -1) J[n1] += transconductance * (ctrl_n1 != -1 ? 1.0 : 0.0) - transconductance * (ctrl_n2 != -1 ? 1.0 : 0.0);
        if (n2 != -1) J[n2] -= transconductance * (ctrl_n1 != -1 ? 1.0 : 0.0) - transconductance * (ctrl_n2 != -1 ? 1.0 : 0.0);
    }
}

// --- Current Controlled Current Source (CCCS) Implementation ---
CurrentControlledCurrentSource::CurrentControlledCurrentSource() : Element(), gain(0.0) {}

CurrentControlledCurrentSource::CurrentControlledCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_branch, double g)
    : Element(name, node1, node2), controlling_branch_name(ctrl_branch), gain(g) {}

std::string CurrentControlledCurrentSource::getType() const { return "CurrentControlledCurrentSource"; }
double CurrentControlledCurrentSource::getValue() const { return gain; }
std::string CurrentControlledCurrentSource::getAddCommandString() const {
    return "CCCS " + name + " " + node1_id + " " + node2_id + " " + controlling_branch_name + " " + std::to_string(gain);
}

std::string CurrentControlledCurrentSource::getControllingBranchName() const { return controlling_branch_name; }
double CurrentControlledCurrentSource::getGain() const { return gain; }

void CurrentControlledCurrentSource::contributeToMNA(Matrix&, Vector&, int, const NodeIndexMap&, const std::map<std::string, double>&, bool, double) {
    // For current-controlled sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
}

// --- Current Controlled Voltage Source (CCVS) Implementation ---
CurrentControlledVoltageSource::CurrentControlledVoltageSource() : Element(), transresistance(0.0) {}

CurrentControlledVoltageSource::CurrentControlledVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_branch, double r)
    : Element(name, node1, node2), controlling_branch_name(ctrl_branch), transresistance(r) {}

std::string CurrentControlledVoltageSource::getType() const { return "CurrentControlledVoltageSource"; }
double CurrentControlledVoltageSource::getValue() const { return transresistance; }
std::string CurrentControlledVoltageSource::getAddCommandString() const {
    return "CCVS " + name + " " + node1_id + " " + node2_id + " " + controlling_branch_name + " " + std::to_string(transresistance);
}

std::string CurrentControlledVoltageSource::getControllingBranchName() const { return controlling_branch_name; }
double CurrentControlledVoltageSource::getTransresistance() const { return transresistance; }

void CurrentControlledVoltageSource::contributeToMNA(Matrix&, Vector&, int, const NodeIndexMap&, const std::map<std::string, double>&, bool, double) {
    // For current-controlled voltage sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
}

// --- Diode Implementation ---
Diode::Diode() : Element(), model_type(""), saturation_current(1e-12), ideality_factor(1.0), thermal_voltage(0.026) {}

Diode::Diode(const std::string& name, const std::string& node1, const std::string& node2, const std::string& model)
    : Element(name, node1, node2), model_type(model), saturation_current(1e-12), ideality_factor(1.0), thermal_voltage(0.026) {}
std::string Diode::getType() const { return "Diode"; }
double Diode::getValue() const { return std::numeric_limits<double>::quiet_NaN(); }
std::string Diode::getAddCommandString() const {
    return "D " + name + " " + node1_id + " " + node2_id + " " + model_type;
}

void Diode::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>& prev_voltages, bool, double) {
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        // Get previous voltages for linearization
        double v1_prev = prev_voltages.count(node1_id) ? prev_voltages.at(node1_id) : 0.0;
        double v2_prev = prev_voltages.count(node2_id) ? prev_voltages.at(node2_id) : 0.0;
        double vd_prev = v1_prev - v2_prev;

        // Linearize around the previous operating point
        double id_prev = saturation_current * (std::exp(vd_prev / (ideality_factor * thermal_voltage)) - 1.0);
        double gd_prev = saturation_current * std::exp(vd_prev / (ideality_factor * thermal_voltage)) / (ideality_factor * thermal_voltage);

        // Add conductance and current source terms
        G[n1][n1] += gd_prev;
        G[n2][n2] += gd_prev;
        G[n1][n2] -= gd_prev;
        G[n2][n1] -= gd_prev;

        // Add current source term
        double is_equiv = id_prev - gd_prev * vd_prev;
        if (n1 != -1) J[n1] -= is_equiv;
        if (n2 != -1) J[n2] += is_equiv;
    }
}

// --- Ground Implementation ---
Ground::Ground() : Element() {}

Ground::Ground(const std::string& name, const std::string& node_id)
    : Element(name, node_id, "0") {}

std::string Ground::getType() const { return "Ground"; }
double Ground::getValue() const { return 0.0; }
std::string Ground::getAddCommandString() const {
    return "GND " + name + " " + node1_id;
}

void Ground::contributeToMNA(Matrix&, Vector&, int, const NodeIndexMap&, const std::map<std::string, double>&, bool, double) {
    // Ground nodes are handled specially in the MNA matrix building process
    // They are assigned to row/column 0 and their voltage is fixed at 0V
}

// --- Subcircuit Implementation ---
Subcircuit::Subcircuit() : Element(), internal_port1_id(""), internal_port2_id("") {}

Subcircuit::Subcircuit(const std::string& name, const std::string& node1, const std::string& node2, std::unique_ptr<Circuit> internal_c, std::string port1, std::string port2)
    : Element(name, node1, node2), internal_circuit(std::move(internal_c)), internal_port1_id(port1), internal_port2_id(port2) {}
Subcircuit::~Subcircuit() = default;
std::string Subcircuit::getType() const { return "Subcircuit"; }
double Subcircuit::getValue() const { return std::numeric_limits<double>::quiet_NaN(); }
std::string Subcircuit::getAddCommandString() const {
    return "SUBCKT " + name + " " + node1_id + " " + node2_id + " " + internal_port1_id + " " + internal_port2_id;
}

void Subcircuit::contributeToMNA(Matrix&, Vector&, int, const NodeIndexMap&, const std::map<std::string, double>&, bool, double) {
    // Subcircuit contribution is handled by expanding the internal circuit
    // This is a placeholder for now - full implementation would require
    // expanding the internal circuit and mapping its nodes to the main circuit
}

// --- Wireless Voltage Source Implementation ---
WirelessVoltageSource::WirelessVoltageSource() : Element(), is_server(false), ip_address(""), port(0), last_known_voltage(0.0) {}

WirelessVoltageSource::WirelessVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, bool isServer, std::string ip, int p)
    : Element(name, node1, node2), is_server(isServer), ip_address(std::move(ip)), port(p), last_known_voltage(0.0) {}
WirelessVoltageSource::~WirelessVoltageSource() = default;
std::string WirelessVoltageSource::getType() const { return "WirelessVoltageSource"; }
double WirelessVoltageSource::getValue() const { return last_known_voltage; }
std::string WirelessVoltageSource::getAddCommandString() const {
    return "Vwireless " + name + " " + node1_id + " " + node2_id + " " + (is_server ? "SERVER" : "CLIENT") + " " + ip_address + " " + std::to_string(port);
}

void WirelessVoltageSource::contributeToMNA(Matrix&, Vector&, int, const NodeIndexMap&, const std::map<std::string, double>&, bool, double) {
    // For wireless voltage sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
    // The actual voltage value is obtained from the network connection
}
