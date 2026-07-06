#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "../semantic/Semantic.h"

struct TargetInstruction {
    std::string label;
    std::string op;
    std::vector<std::string> args;
};

struct TargetProgram {
    std::vector<TargetInstruction> instructions;
};

TargetProgram generateTargetProgram(const std::vector<Quad>& quads);
void printTargetProgram(const TargetProgram& program, std::ostream& output);
bool runTargetProgram(const TargetProgram& program, std::istream& input, std::ostream& output, std::ostream& error);
