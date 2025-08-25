#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <complex>
#include <cereal/archives/json.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/map.hpp>

// --- Type Aliases ---
using Matrix = std::vector<std::vector<double>>;
using Vector = std::vector<double>;
using NodeIndexMap = std::map<std::string, int>;
using Complex = std::complex<double>;

// Forward declarations
class Circuit;
class TcpSocket;

// --- Element (Abstract Base) ---
class Element {
protected:
    std::string name;
    std::string node1_id;
    std::string node2_id;
    Element();

public:
    Element(const std::string& name, const std::string& node1, const std::string& node2);
    virtual ~Element() = default;
    virtual std::string getType() const = 0;
    virtual double getValue() const = 0;
    virtual void setValue(double value) = 0;
    virtual std::string getAddCommandString() const = 0;
    virtual void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map,
                                 const std::map<std::string, double>& prev_node_voltages,
                                 bool is_transient, double timestep) = 0;
    std::string getName() const;
    std::string getNode1Id() const;
    std::string getNode2Id() const;
    void setNode1Id(const std::string& new_id);
    void setNode2Id(const std::string& new_id);

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(name), CEREAL_NVP(node1_id), CEREAL_NVP(node2_id));
    }
};

// --- CircuitWire Class ---
class CircuitWire : public Element {
private:
    friend class cereal::access;
    CircuitWire(); // Default constructor for Cereal
public:
    CircuitWire(const std::string& name, const std::string& node1, const std::string& node2);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map,
                         const std::map<std::string, double>& prev_node_voltages,
                         bool is_transient, double timestep) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this));
    }
};


// --- Concrete Element Class Definitions ---
class Resistor : public Element {
private:
    double resistance;
    friend class cereal::access;
    Resistor(); // Default constructor for Cereal
public:
    Resistor(const std::string& name, const std::string& node1, const std::string& node2, double value);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(resistance));
    }
};
class Capacitor : public Element {
private:
    double capacitance;
    friend class cereal::access;
    Capacitor(); // Default constructor for Cereal
public:
    Capacitor(const std::string& name, const std::string& node1, const std::string& node2, double value);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(capacitance));
    }
};

class Inductor : public Element {
private:
    double inductance;
    friend class cereal::access;
    Inductor(); // Default constructor for Cereal
public:
    Inductor(const std::string& name, const std::string& node1, const std::string& node2, double value);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(inductance));
    }
};

class IndependentVoltageSource : public Element {
private:
    double voltage_value;
    friend class cereal::access;
    IndependentVoltageSource(); // Default constructor for Cereal
public:
    IndependentVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double value);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double new_value);
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(voltage_value));
    }
};

class IndependentCurrentSource : public Element {
private:
    double current_value;
    friend class cereal::access;
    IndependentCurrentSource(); // Default constructor for Cereal
public:
    IndependentCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, double value);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double new_value);
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(current_value));
    }
};

class PulseVoltageSource : public Element {
private:
    double v1_val, v2_val, td_val, tr_val, tf_val, pw_val, per_val;
    friend class cereal::access;
    PulseVoltageSource(); // Default constructor for Cereal
public:
    PulseVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double V1_val, double V2_val, double TD_val, double TR_val, double TF_val, double PW_val, double PER_val);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    void setV1(double new_v1);
    void setV2(double new_v2);
    void setTd(double new_td);
    void setTr(double new_tr);
    void setTf(double new_tf);
    void setPw(double new_pw);
    void setPer(double new_per);
    std::string getAddCommandString() const override;
    double getVoltageAtTime(double current_time) const;
    double getV1() const;
    double getV2() const;
    double getTd() const;
    double getTr() const;
    double getTf() const;
    double getPw() const;
    double getPer() const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(v1_val), CEREAL_NVP(v2_val), CEREAL_NVP(td_val), CEREAL_NVP(tr_val), CEREAL_NVP(tf_val), CEREAL_NVP(pw_val), CEREAL_NVP(per_val));
    }
};

class SinusoidalVoltageSource : public Element {
private:
    double dc_offset;
    double amplitude;
    double frequency;
    friend class cereal::access;
    SinusoidalVoltageSource(); // Default constructor for Cereal
public:
    SinusoidalVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double offset, double amp, double freq);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    void setDCOffset(double new_offset);
    std::string getAddCommandString() const override;
    double getVoltageAtTime(double current_time) const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(dc_offset), CEREAL_NVP(amplitude), CEREAL_NVP(frequency));
    }
};

class ACVoltageSource : public Element {
private:
    double magnitude;
    double phase;
    double frequency;
    friend class cereal::access;
    ACVoltageSource(); // Default constructor for Cereal
public:
    ACVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, double mag, double ph, double freq);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    void setMagnitude(double new_magnitude);
    void setPhase(double new_phase);
    void setFrequency(double new_frequency);
    double getMagnitude() const;
    double getPhase() const;
    double getFrequency() const;
    std::string getAddCommandString() const override;
    std::complex<double> getComplexVoltage() const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(magnitude), CEREAL_NVP(phase), CEREAL_NVP(frequency));
    }
};



class PulseCurrentSource : public Element {
private:
    double i1, i2;    // Initial and pulse current values  
    double td, tr, tf, pw, per;  // Delay, rise time, fall time, pulse width, period
    friend class cereal::access;
    PulseCurrentSource(); // Default constructor for Cereal
public:
    PulseCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, 
                       double I1, double I2, double TD, double TR, double TF, double PW, double PER);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    double getCurrentAtTime(double time) const;
    
    // Getters for pulse parameters
    double getI1() const { return i1; }
    double getI2() const { return i2; }
    double getTd() const { return td; }
    double getTr() const { return tr; }
    double getTf() const { return tf; }
    double getPw() const { return pw; }
    double getPer() const { return per; }
    
    // Setters for pulse parameters
    void setI1(double val) { i1 = val; }
    void setI2(double val) { i2 = val; }
    void setTd(double val) { td = val; }
    void setTr(double val) { tr = val; }
    void setTf(double val) { tf = val; }
    void setPw(double val) { pw = val; }
    void setPer(double val) { per = val; }
    
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(i1), CEREAL_NVP(i2), CEREAL_NVP(td), CEREAL_NVP(tr), CEREAL_NVP(tf), CEREAL_NVP(pw), CEREAL_NVP(per));
    }
};

class WaveformVoltageSource : public Element {
private:
    std::vector<double> voltage_values;  // Voltage values at each sample
    double sampling_rate;                // Samples per second (Hz)
    double signal_duration;              // Total duration of signal (s)
    double start_time;                   // When the waveform starts (s)
    bool repeat;                         // Whether to repeat the waveform
    friend class cereal::access;
    WaveformVoltageSource(); // Default constructor for Cereal
public:
    WaveformVoltageSource(const std::string& name, const std::string& node1, const std::string& node2,
                          const std::vector<double>& values, double fs, double duration, double start_time = 0.0, bool repeat = false);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    double getVoltageAtTime(double time) const;
    const std::vector<double>& getVoltageValues() const;
    double getSamplingRate() const;
    double getSignalDuration() const;
    double getStartTime() const;
    bool getRepeat() const;
    void setVoltageValues(const std::vector<double>& values);
    void setSamplingRate(double fs);
    void setSignalDuration(double duration);
    void setStartTime(double start_time);
    void setRepeat(bool repeat);
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double currentTime) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(voltage_values), CEREAL_NVP(sampling_rate), 
                CEREAL_NVP(signal_duration), CEREAL_NVP(start_time), CEREAL_NVP(repeat));
    }
};

class PhaseVoltageSource : public Element {
private:
    double magnitude;      // Voltage magnitude (V)
    double base_frequency; // Base frequency omega_base (rad/s)
    double phase;          // Phase phi (radians)
    friend class cereal::access;
    PhaseVoltageSource(); // Default constructor for Cereal
public:
    PhaseVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, 
                       double magnitude, double base_frequency, double phase);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    double getVoltageAtTime(double time) const;
    double getMagnitude() const;
    double getBaseFrequency() const;
    double getPhase() const;
    void setMagnitude(double mag);
    void setBaseFrequency(double freq);
    void setPhase(double ph);
    std::complex<double> getComplexVoltage() const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double currentTime) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(magnitude), CEREAL_NVP(base_frequency), CEREAL_NVP(phase));
    }
};

class VoltageControlledVoltageSource : public Element {
private:
    std::string control_node1_id;
    std::string control_node2_id;
    double gain;
    friend class cereal::access;
    VoltageControlledVoltageSource(); // Default constructor for Cereal
public:
    VoltageControlledVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_node1, const std::string& ctrl_node2, double g);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    std::string getControlNode1Id() const;
    std::string getControlNode2Id() const;
    double getGain() const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(control_node1_id), CEREAL_NVP(control_node2_id), CEREAL_NVP(gain));
    }
};

class VoltageControlledCurrentSource : public Element {
private:
    std::string control_node1_id;
    std::string control_node2_id;
    double transconductance;
    friend class cereal::access;
    VoltageControlledCurrentSource(); // Default constructor for Cereal
public:
    VoltageControlledCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_node1, const std::string& ctrl_node2, double g);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    std::string getControlNode1Id() const;
    std::string getControlNode2Id() const;
    double getTransconductance() const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(control_node1_id), CEREAL_NVP(control_node2_id), CEREAL_NVP(transconductance));
    }
};

class CurrentControlledCurrentSource : public Element {
private:
    std::string controlling_branch_name;
    double gain;
    friend class cereal::access;
    CurrentControlledCurrentSource(); // Default constructor for Cereal
public:
    CurrentControlledCurrentSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_branch, double g);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    std::string getControllingBranchName() const;
    double getGain() const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(controlling_branch_name), CEREAL_NVP(gain));
    }
};

class CurrentControlledVoltageSource : public Element {
private:
    std::string controlling_branch_name;
    double transresistance;
    friend class cereal::access;
    CurrentControlledVoltageSource(); // Default constructor for Cereal
public:
    CurrentControlledVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, const std::string& ctrl_branch, double r);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    std::string getControllingBranchName() const;
    double getTransresistance() const;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(controlling_branch_name), CEREAL_NVP(transresistance));
    }
};


class Diode : public Element {
private:
    std::string model_type;
    double saturation_current;
    double ideality_factor;
    double thermal_voltage;
    friend class cereal::access;
    Diode(); // Default constructor for Cereal
public:
    Diode(const std::string& name, const std::string& node1, const std::string& node2, const std::string& model);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(model_type), CEREAL_NVP(saturation_current), CEREAL_NVP(ideality_factor), CEREAL_NVP(thermal_voltage));
    }
};

class Ground : public Element {
private:
    friend class cereal::access;
    Ground(); // Default constructor for Cereal
public:
    Ground(const std::string& name, const std::string& node_id);
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this));
    }
};

class Subcircuit : public Element {
private:
    std::unique_ptr<Circuit> internal_circuit;
    std::string internal_port1_id;
    std::string internal_port2_id;
    friend class cereal::access;
    Subcircuit();
public:
    Subcircuit(const std::string& name, const std::string& node1, const std::string& node2, std::unique_ptr<Circuit> internal_c, std::string port1, std::string port2);
    ~Subcircuit();
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& main_node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(internal_circuit), CEREAL_NVP(internal_port1_id), CEREAL_NVP(internal_port2_id));
    }
};

class WirelessVoltageSource : public Element {
private:
    bool is_server;
    std::string ip_address;
    int port;
    mutable std::unique_ptr<TcpSocket> socket;
    double last_known_voltage;
    friend class cereal::access;
    WirelessVoltageSource();
public:
    WirelessVoltageSource(const std::string& name, const std::string& node1, const std::string& node2, bool isServer, std::string ip, int p);
    ~WirelessVoltageSource();
    std::string getType() const override;
    double getValue() const override;
    void setValue(double value) override;
    std::string getAddCommandString() const override;
    void contributeToMNA(Matrix& G, Vector& J, int num_nodes, const NodeIndexMap& node_map, const std::map<std::string, double>&, bool, double) override;
    template<class Archive>
    void serialize(Archive& archive) {
        archive(cereal::base_class<Element>(this), CEREAL_NVP(is_server), CEREAL_NVP(ip_address), CEREAL_NVP(port));
    }
};
