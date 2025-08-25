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
CircuitWire::CircuitWire() : Element() {}

CircuitWire::CircuitWire(const std::string& name, const std::string& node1, const std::string& node2)
    : Element(name, node1, node2) {}

std::string CircuitWire::getType() const { return "Wire"; }
double CircuitWire::getValue() const { return 0.0; }
void CircuitWire::setValue(double value) { /* Wires don't have values */ }
std::string CircuitWire::getAddCommandString() const {
    return "* wire " + getName() + " " + getNode1Id() + " " + getNode2Id();
}
void CircuitWire::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map,
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
void Resistor::setValue(double value) { resistance = value; }
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
void Capacitor::setValue(double value) { capacitance = value; }
std::string Capacitor::getAddCommandString() const {
    return "C " + name + " " + node1_id + " " + node2_id + " " + std::to_string(capacitance);
}

void Capacitor::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>& prev_voltages, bool is_transient, double timestep) {
    if (!is_transient || timestep <= 0) {
        // For DC analysis, capacitor acts as open circuit (no contribution)
        return;
    }

    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        // For transient analysis, use backward Euler: C*dV/dt = I
        // Discretized: C*(V[n] - V[n-1])/dt = I[n]
        // Rearranged: C/dt * V[n] = I[n] + C/dt * V[n-1]
        double conductance = capacitance / timestep;
        
        // Add conductance matrix contributions
        G[n1][n1] += conductance;
        G[n2][n2] += conductance;
        G[n1][n2] -= conductance;
        G[n2][n1] -= conductance;

        // Add current source from previous voltage step
        double v1_prev = prev_voltages.count(node1_id) ? prev_voltages.at(node1_id) : 0.0;
        double v2_prev = prev_voltages.count(node2_id) ? prev_voltages.at(node2_id) : 0.0;
        double i_history = conductance * (v1_prev - v2_prev);
        
        if (n1 != -1) J[n1] += i_history;
        if (n2 != -1) J[n2] -= i_history;
        
        // Debug logging for capacitor behavior
        static int log_count = 0;
        if (log_count < 5) { // Log only first few steps
            std::cout << "[CAP " << name << "] timestep=" << timestep << ", C=" << capacitance 
                      << ", G=" << conductance << ", V_prev=(" << v1_prev << "," << v2_prev 
                      << "), I_hist=" << i_history << std::endl;
            log_count++;
        }
    }
}

// --- Inductor Implementation ---
Inductor::Inductor() : Element(), inductance(0.0) {}

Inductor::Inductor(const std::string& name, const std::string& node1, const std::string& node2, double value)
    : Element(name, node1, node2), inductance(value) {}

std::string Inductor::getType() const { return "Inductor"; }
double Inductor::getValue() const { return inductance; }
void Inductor::setValue(double value) { inductance = value; }
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
PulseVoltageSource::PulseVoltageSource() : Element(), v1_val(0.0), v2_val(5.0), td_val(1e-3), tr_val(1e-4), tf_val(1e-4), pw_val(3e-3), per_val(8e-3) {}

PulseVoltageSource::PulseVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double V1_val, double V2_val, double TD_val, double TR_val, double TF_val, double PW_val, double PER_val)
    : Element(name, node1, node2), v1_val(V1_val), v2_val(V2_val), td_val(TD_val), tr_val(TR_val), tf_val(TF_val), pw_val(PW_val), per_val(PER_val) {}
std::string PulseVoltageSource::getType() const { return "PulseVoltageSource"; }
double PulseVoltageSource::getValue() const { return v1_val; }
void PulseVoltageSource::setValue(double value) { v1_val = value; }
void PulseVoltageSource::setV1(double new_v1) { v1_val = new_v1; }
std::string PulseVoltageSource::getAddCommandString() const {
    return "Vpulse " + name + " " + node1_id + " " + node2_id + " " + std::to_string(v1_val) + " " + std::to_string(v2_val) + " " + std::to_string(td_val) + " " + std::to_string(tr_val) + " " + std::to_string(tf_val) + " " + std::to_string(pw_val) + " " + std::to_string(per_val);
}

double PulseVoltageSource::getVoltageAtTime(double current_time) const {
    if (current_time < td_val) {
        return v1_val;
    }
    
    double t_cycle = fmod(current_time - td_val, per_val);
    double voltage;
    
    if (t_cycle < tr_val) {
        // Rising edge
        voltage = v1_val + (v2_val - v1_val) * (t_cycle / tr_val);
    } else if (t_cycle < tr_val + pw_val) {
        // High level
        voltage = v2_val;
    } else if (t_cycle < tr_val + pw_val + tf_val) {
        // Falling edge
        double t_fall = t_cycle - tr_val - pw_val;
        voltage = v2_val + (v1_val - v2_val) * (t_fall / tf_val);
    } else {
        // Low level
        voltage = v1_val;
    }
    
    // Debug output every 100 time steps to avoid spam
    static int debug_counter = 0;
    if (debug_counter % 100 == 0) {
        std::cout << "[PULSE] t=" << current_time << "s, t_cycle=" << t_cycle << "s, V=" << voltage << "V (V1=" << v1_val << ", V2=" << v2_val << ", td=" << td_val << ", per=" << per_val << ")" << std::endl;
    }
    debug_counter++;
    
    return voltage;
}

double PulseVoltageSource::getV1() const { return v1_val; }
double PulseVoltageSource::getV2() const { return v2_val; }
double PulseVoltageSource::getTd() const { return td_val; }
double PulseVoltageSource::getTr() const { return tr_val; }
double PulseVoltageSource::getTf() const { return tf_val; }
double PulseVoltageSource::getPw() const { return pw_val; }
double PulseVoltageSource::getPer() const { return per_val; }

void PulseVoltageSource::setV2(double new_v2) { v2_val = new_v2; }
void PulseVoltageSource::setTd(double new_td) { td_val = new_td; }
void PulseVoltageSource::setTr(double new_tr) { tr_val = new_tr; }
void PulseVoltageSource::setTf(double new_tf) { tf_val = new_tf; }
void PulseVoltageSource::setPw(double new_pw) { pw_val = new_pw; }
void PulseVoltageSource::setPer(double new_per) { per_val = new_per; }

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
void SinusoidalVoltageSource::setValue(double value) { dc_offset = value; }
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

// --- AC Voltage Source Implementation ---
ACVoltageSource::ACVoltageSource() : Element(), magnitude(1.0), phase(0.0), frequency(1000.0) {}

ACVoltageSource::ACVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double mag, double ph, double freq)
    : Element(name, node1, node2), magnitude(mag), phase(ph), frequency(freq) {}

std::string ACVoltageSource::getType() const { return "ACVoltageSource"; }
double ACVoltageSource::getValue() const { return magnitude; }
void ACVoltageSource::setValue(double value) { magnitude = value; }
void ACVoltageSource::setMagnitude(double new_magnitude) { magnitude = new_magnitude; }
void ACVoltageSource::setPhase(double new_phase) { phase = new_phase; }
void ACVoltageSource::setFrequency(double new_frequency) { frequency = new_frequency; }
double ACVoltageSource::getMagnitude() const { return magnitude; }
double ACVoltageSource::getPhase() const { return phase; }
double ACVoltageSource::getFrequency() const { return frequency; }

std::string ACVoltageSource::getAddCommandString() const {
    return "VAC " + name + " " + node1_id + " " + node2_id + " AC " + std::to_string(magnitude) + " " + std::to_string(phase);
}

std::complex<double> ACVoltageSource::getComplexVoltage() const {
    double phase_rad = phase * M_PI / 180.0; // Convert degrees to radians
    return std::complex<double>(magnitude * cos(phase_rad), magnitude * sin(phase_rad));
}

void ACVoltageSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) {
    // For AC voltage sources, the contribution is handled in ComplexMNAMatrix::build
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    if (n1 != -1 && n2 != -1) {
        // The voltage difference constraint will be handled in ComplexMNAMatrix::build
    }
}



// --- Pulse Current Source Implementation ---
PulseCurrentSource::PulseCurrentSource() : Element(), i1(0.0), i2(1e-3), td(1e-3), tr(1e-4), tf(1e-4), pw(2e-3), per(5e-3) {}

PulseCurrentSource::PulseCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, 
                                       double I1, double I2, double TD, double TR, double TF, double PW, double PER)
    : Element(name, node1, node2), i1(I1), i2(I2), td(TD), tr(TR), tf(TF), pw(PW), per(PER) {}

std::string PulseCurrentSource::getType() const { return "PulseCurrentSource"; }
double PulseCurrentSource::getValue() const { return i2; } // Return pulse amplitude
void PulseCurrentSource::setValue(double value) { i2 = value; }

std::string PulseCurrentSource::getAddCommandString() const {
    return "IPULSE " + name + " " + node1_id + " " + node2_id + " " + 
           std::to_string(i1) + " " + std::to_string(i2) + " " + std::to_string(td) + " " + 
           std::to_string(tr) + " " + std::to_string(tf) + " " + std::to_string(pw) + " " + std::to_string(per);
}

double PulseCurrentSource::getCurrentAtTime(double current_time) const {
    if (per <= 0) return i1; // If no period set, stay at initial value
    
    // Calculate time within the current period
    double t_period = fmod(current_time, per);
    
    if (t_period < td) {
        // Before delay time - initial value
        return i1;
    } else if (t_period < td + tr) {
        // Rise time - linear interpolation
        double t_rise = (t_period - td) / tr;
        return i1 + (i2 - i1) * t_rise;
    } else if (t_period < td + tr + pw) {
        // Pulse width - constant pulse value
        return i2;
    } else if (t_period < td + tr + pw + tf) {
        // Fall time - linear interpolation
        double t_fall = (t_period - td - tr - pw) / tf;
        return i2 + (i1 - i2) * t_fall;
    } else {
        // After pulse - back to initial value
        return i1;
    }
}

void PulseCurrentSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool is_transient, double currentTime) {
    int n1 = node_map.count(node1_id) ? node_map.at(node1_id) : -1;
    int n2 = node_map.count(node2_id) ? node_map.at(node2_id) : -1;

    double current_value = is_transient ? getCurrentAtTime(currentTime) : i1; // Use initial value for DC
    
    if (n1 != -1) J[n1] -= current_value;
    if (n2 != -1) J[n2] += current_value;
}

// --- Waveform Voltage Source Implementation ---

// --- Waveform Voltage Source Implementation ---
WaveformVoltageSource::WaveformVoltageSource() : Element(), sampling_rate(1000.0), signal_duration(1.0), start_time(0.0), repeat(false) {}

WaveformVoltageSource::WaveformVoltageSource(const std::string& name, const std::string& node1, const std::string& node2,
                                             const std::vector<double>& values, double fs, double duration, double start_time, bool repeat)
    : Element(name, node1, node2), voltage_values(values), sampling_rate(fs), signal_duration(duration), start_time(start_time), repeat(repeat) {}

std::string WaveformVoltageSource::getType() const { return "WaveformVoltageSource"; }
double WaveformVoltageSource::getValue() const { 
    return voltage_values.empty() ? 0.0 : voltage_values[0]; 
}
void WaveformVoltageSource::setValue(double value) { 
    if (!voltage_values.empty()) voltage_values[0] = value; 
}

std::string WaveformVoltageSource::getAddCommandString() const {
    std::stringstream ss;
    ss << "VWAVEFORM " << name << " " << node1_id << " " << node2_id << " " 
       << sampling_rate << " " << signal_duration << " " << start_time << " " << (repeat ? 1 : 0) << " [";
    for (size_t i = 0; i < voltage_values.size(); ++i) {
        if (i > 0) ss << ",";
        ss << voltage_values[i];
    }
    ss << "]";
    return ss.str();
}

double WaveformVoltageSource::getVoltageAtTime(double time) const {
    if (voltage_values.empty()) return 0.0;
    
    // Check if we're before the start time
    if (time < start_time) return 0.0;
    
    double relative_time = time - start_time;
    
    // Handle repetition
    if (repeat && signal_duration > 0.0) {
        relative_time = fmod(relative_time, signal_duration);
    } else if (relative_time >= signal_duration) {
        // Past the end of non-repeating signal
        return voltage_values.back();
    }
    
    // Calculate the index in the voltage array
    double sample_index = relative_time * sampling_rate;
    
    // Handle bounds
    if (sample_index < 0.0) return voltage_values.front();
    if (sample_index >= voltage_values.size() - 1) return voltage_values.back();
    
    // Linear interpolation between samples
    int index_floor = static_cast<int>(sample_index);
    int index_ceil = index_floor + 1;
    double fraction = sample_index - index_floor;
    
    if (index_ceil >= voltage_values.size()) {
        return voltage_values.back();
    }
    
    return voltage_values[index_floor] * (1.0 - fraction) + voltage_values[index_ceil] * fraction;
}

const std::vector<double>& WaveformVoltageSource::getVoltageValues() const { return voltage_values; }
double WaveformVoltageSource::getSamplingRate() const { return sampling_rate; }
double WaveformVoltageSource::getSignalDuration() const { return signal_duration; }
double WaveformVoltageSource::getStartTime() const { return start_time; }
bool WaveformVoltageSource::getRepeat() const { return repeat; }

void WaveformVoltageSource::setVoltageValues(const std::vector<double>& values) { voltage_values = values; }
void WaveformVoltageSource::setSamplingRate(double fs) { sampling_rate = fs; }
void WaveformVoltageSource::setSignalDuration(double duration) { signal_duration = duration; }
void WaveformVoltageSource::setStartTime(double start_time) { this->start_time = start_time; }
void WaveformVoltageSource::setRepeat(bool repeat) { this->repeat = repeat; }

void WaveformVoltageSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool is_transient, double currentTime) {
    // Get the voltage at current time
    double voltage = getVoltageAtTime(currentTime);
    
    // Waveform voltage source is handled like any voltage source in MNA
    // The actual voltage constraint is handled in the solver
    
    // For debugging
    static int debug_count = 0;
    if (debug_count < 10) {
        ErrorManager::info("[WAVEFORM] " + name + " at t=" + std::to_string(currentTime) + "s: V=" + std::to_string(voltage) + "V");
        debug_count++;
    }
}

// --- Phase Voltage Source Implementation ---
PhaseVoltageSource::PhaseVoltageSource() : Element(), magnitude(1.0), base_frequency(1.0), phase(0.0) {}

PhaseVoltageSource::PhaseVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, 
                                       double magnitude, double base_frequency, double phase)
    : Element(name, node1, node2), magnitude(magnitude), base_frequency(base_frequency), phase(phase) {}

std::string PhaseVoltageSource::getType() const { return "PhaseVoltageSource"; }
double PhaseVoltageSource::getValue() const { return magnitude; }
void PhaseVoltageSource::setValue(double value) { magnitude = value; }

std::string PhaseVoltageSource::getAddCommandString() const {
    return "VPHASE " + name + " " + node1_id + " " + node2_id + " " + 
           std::to_string(magnitude) + " " + std::to_string(base_frequency) + " " + std::to_string(phase);
}

double PhaseVoltageSource::getVoltageAtTime(double time) const {
    // V(t) = magnitude * cos(base_frequency * t + phase)
    return magnitude * cos(base_frequency * time + phase);
}

double PhaseVoltageSource::getMagnitude() const { return magnitude; }
double PhaseVoltageSource::getBaseFrequency() const { return base_frequency; }
double PhaseVoltageSource::getPhase() const { return phase; }

void PhaseVoltageSource::setMagnitude(double mag) { magnitude = mag; }
void PhaseVoltageSource::setBaseFrequency(double freq) { base_frequency = freq; }
void PhaseVoltageSource::setPhase(double ph) { phase = ph; }

std::complex<double> PhaseVoltageSource::getComplexVoltage() const {
    // Complex representation: V = magnitude * e^(j*phase)
    return std::complex<double>(magnitude * cos(phase), magnitude * sin(phase));
}

void PhaseVoltageSource::contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool is_transient, double currentTime) {
    // Get the voltage at current time
    double voltage = getVoltageAtTime(currentTime);
    
    // Phase voltage source is handled like any voltage source in MNA
    // The actual voltage constraint is handled in the solver
    
    // For debugging
    static int debug_count = 0;
    if (debug_count < 10) {
        ErrorManager::info("[PHASE] " + name + " at t=" + std::to_string(currentTime) + "s: V=" + std::to_string(voltage) + "V (φ=" + std::to_string(phase) + " rad)");
        debug_count++;
    }
}

// --- Voltage Controlled Voltage Source (VCVS) Implementation ---
VoltageControlledVoltageSource::VoltageControlledVoltageSource() : Element(), gain(0.0) {}

VoltageControlledVoltageSource::VoltageControlledVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_node1, const std::string& ctrl_node2, double g)
    : Element(name, node1, node2), control_node1_id(ctrl_node1), control_node2_id(ctrl_node2), gain(g) {}

std::string VoltageControlledVoltageSource::getType() const { return "VoltageControlledVoltageSource"; }
double VoltageControlledVoltageSource::getValue() const { return gain; }
void VoltageControlledVoltageSource::setValue(double value) { gain = value; }
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
void VoltageControlledCurrentSource::setValue(double value) { transconductance = value; }
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
void CurrentControlledCurrentSource::setValue(double value) { gain = value; }
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
void CurrentControlledVoltageSource::setValue(double value) { transresistance = value; }
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
void Diode::setValue(double value) { /* Diodes don't have simple values */ }
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
void Ground::setValue(double value) { /* Ground voltage is fixed at 0V */ }
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
void Subcircuit::setValue(double value) { /* Subcircuits don't have simple values */ }
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
void WirelessVoltageSource::setValue(double value) { last_known_voltage = value; }
std::string WirelessVoltageSource::getAddCommandString() const {
    return "Vwireless " + name + " " + node1_id + " " + node2_id + " " + (is_server ? "SERVER" : "CLIENT") + " " + ip_address + " " + std::to_string(port);
}

void WirelessVoltageSource::contributeToMNA(Matrix&, Vector&, int, const NodeIndexMap&, const std::map<std::string, double>&, bool, double) {
    // For wireless voltage sources, we need to add a current variable to the MNA matrix
    // This is handled in MNAMatrix::build by adding extra rows/columns
    // The actual voltage value is obtained from the network connection
}
