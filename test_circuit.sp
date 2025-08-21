* Simple test circuit for SPICE simulator
* DC voltage source with resistor and capacitor

* Voltage source
V1 N1 0 5.0

* Resistor
R1 N1 N2 1000.0

* Capacitor
C1 N2 0 1e-6

* Ground
GND 0

* Analysis commands
.tran 1e-6 5e-3
.ac V1 1 100k 100
.dc V1 0 10 0.1
.end
