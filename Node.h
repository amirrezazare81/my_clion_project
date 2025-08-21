#pragma once
#include <string>

class Node {
private:
    std::string id;
    double voltage;
    bool isGround; // Standardized naming

public:
    explicit Node(const std::string& id);
    std::string getId() const;
    double getVoltage() const;
    bool getIsGround() const;
    void setVoltage(double vol);
    void setAsGround();
};
