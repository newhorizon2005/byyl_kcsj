#include "TargetCode.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::string quadLabel(int index) {
    return "L" + std::to_string(index);
}

std::string targetLabel(const std::string& quadTarget) {
    if (quadTarget.size() >= 2 && quadTarget[0] == '$') {
        return "L" + quadTarget.substr(1);
    }
    return quadTarget;
}

bool isIntegerLiteral(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    std::size_t pos = 0;
    if (text[0] == '-') {
        pos = 1;
    }
    if (pos == text.size()) {
        return false;
    }
    for (; pos < text.size(); ++pos) {
        if (text[pos] < '0' || text[pos] > '9') {
            return false;
        }
    }
    return true;
}

void addInstruction(TargetProgram& program, const std::string& label, const std::string& op, std::vector<std::string> args = {}) {
    program.instructions.push_back(TargetInstruction{label, op, std::move(args)});
}

bool isGlobalDeclarationQuad(const Quad& quad) {
    return quad.op == "syss" || quad.op == "const" || quad.op == "var" || quad.op == "=";
}

std::vector<bool> findProcedureBodyQuads(const std::vector<Quad>& quads) {
    std::vector<bool> inProcedure(quads.size(), false);
    for (std::size_t i = 0; i < quads.size(); ++i) {
        if (quads[i].op != "procedure") {
            continue;
        }
        for (std::size_t j = i; j < quads.size(); ++j) {
            inProcedure[j] = true;
            if (quads[j].op == "ret") {
                i = j;
                break;
            }
        }
    }
    return inProcedure;
}

void translateQuad(TargetProgram& program, int index, const Quad& quad) {
    const std::string label = quadLabel(index);

    if (quad.op == "syss") {
        addInstruction(program, label, "START");
    } else if (quad.op == "syse") {
        addInstruction(program, label, "HALT");
    } else if (quad.op == "const") {
        addInstruction(program, label, "DECL_CONST", {quad.arg1});
    } else if (quad.op == "var") {
        addInstruction(program, label, "DECL_VAR", {quad.arg1});
    } else if (quad.op == "procedure") {
        addInstruction(program, label, "PROC", {quad.arg1});
    } else if (quad.op == "=" || quad.op == ":=") {
        addInstruction(program, label, "MOV", {quad.arg1, quad.result});
    } else if (quad.op == "+") {
        addInstruction(program, label, "ADD", {quad.arg1, quad.arg2, quad.result});
    } else if (quad.op == "-") {
        addInstruction(program, label, "SUB", {quad.arg1, quad.arg2, quad.result});
    } else if (quad.op == "*") {
        addInstruction(program, label, "MUL", {quad.arg1, quad.arg2, quad.result});
    } else if (quad.op == "/") {
        addInstruction(program, label, "DIV", {quad.arg1, quad.arg2, quad.result});
    } else if (quad.op == "j") {
        addInstruction(program, label, "JMP", {targetLabel(quad.result)});
    } else if (quad.op == "j=") {
        addInstruction(program, label, "JE", {quad.arg1, quad.arg2, targetLabel(quad.result)});
    } else if (quad.op == "j#") {
        addInstruction(program, label, "JNE", {quad.arg1, quad.arg2, targetLabel(quad.result)});
    } else if (quad.op == "j<") {
        addInstruction(program, label, "JLT", {quad.arg1, quad.arg2, targetLabel(quad.result)});
    } else if (quad.op == "j<=") {
        addInstruction(program, label, "JLE", {quad.arg1, quad.arg2, targetLabel(quad.result)});
    } else if (quad.op == "j>") {
        addInstruction(program, label, "JGT", {quad.arg1, quad.arg2, targetLabel(quad.result)});
    } else if (quad.op == "j>=") {
        addInstruction(program, label, "JGE", {quad.arg1, quad.arg2, targetLabel(quad.result)});
    } else if (quad.op == "jnotodd") {
        addInstruction(program, label, "JEVEN", {quad.arg1, targetLabel(quad.result)});
    } else if (quad.op == "read") {
        addInstruction(program, label, "READ", {quad.arg1});
    } else if (quad.op == "write") {
        addInstruction(program, label, "WRITE", {quad.arg1});
    } else if (quad.op == "call") {
        addInstruction(program, label, "CALL", {quad.arg1});
    } else if (quad.op == "ret") {
        addInstruction(program, label, "RET");
    } else {
        addInstruction(program, label, "NOP");
    }
}

std::string instructionText(const TargetInstruction& instruction) {
    std::ostringstream out;
    out << instruction.op;
    for (const std::string& arg : instruction.args) {
        out << ' ' << arg;
    }
    return out.str();
}

class TargetMachine {
public:
    explicit TargetMachine(const TargetProgram& program)
        : program_(program) {
        for (std::size_t i = 0; i < program_.instructions.size(); ++i) {
            const TargetInstruction& instruction = program_.instructions[i];
            if (!instruction.label.empty()) {
                labels_[instruction.label] = static_cast<int>(i);
            }
            if (instruction.op == "PROC" && !instruction.args.empty()) {
                procedures_[instruction.args[0]] = static_cast<int>(i);
            }
        }
    }

    bool run(std::istream& input, std::ostream& output, std::ostream& error) {
        int pc = 0;
        int steps = 0;
        while (pc >= 0 && pc < static_cast<int>(program_.instructions.size())) {
            if (++steps > 100000) {
                error << "运行错误: 执行步数超过上限，可能存在死循环\n";
                return false;
            }

            const TargetInstruction& instruction = program_.instructions[pc];
            const std::string& op = instruction.op;

            if (op == "START" || op == "DECL_CONST" || op == "DECL_VAR" || op == "NOP" || op == "PROC") {
                if ((op == "DECL_CONST" || op == "DECL_VAR") && !instruction.args.empty()) {
                    values_[instruction.args[0]] = valueOf(instruction.args[0]);
                }
                ++pc;
            } else if (op == "HALT") {
                return true;
            } else if (op == "MOV") {
                values_[instruction.args[1]] = valueOf(instruction.args[0]);
                ++pc;
            } else if (op == "ADD") {
                values_[instruction.args[2]] = valueOf(instruction.args[0]) + valueOf(instruction.args[1]);
                ++pc;
            } else if (op == "SUB") {
                values_[instruction.args[2]] = valueOf(instruction.args[0]) - valueOf(instruction.args[1]);
                ++pc;
            } else if (op == "MUL") {
                values_[instruction.args[2]] = valueOf(instruction.args[0]) * valueOf(instruction.args[1]);
                ++pc;
            } else if (op == "DIV") {
                int divisor = valueOf(instruction.args[1]);
                if (divisor == 0) {
                    error << "运行错误: 除数为 0\n";
                    return false;
                }
                values_[instruction.args[2]] = valueOf(instruction.args[0]) / divisor;
                ++pc;
            } else if (op == "JMP") {
                pc = jumpTarget(instruction.args[0], error);
                if (pc < 0) {
                    return false;
                }
            } else if (op == "JE" || op == "JNE" || op == "JLT" || op == "JLE" || op == "JGT" || op == "JGE") {
                bool shouldJump = compare(op, valueOf(instruction.args[0]), valueOf(instruction.args[1]));
                if (shouldJump) {
                    pc = jumpTarget(instruction.args[2], error);
                    if (pc < 0) {
                        return false;
                    }
                } else {
                    ++pc;
                }
            } else if (op == "JEVEN") {
                if (valueOf(instruction.args[0]) % 2 == 0) {
                    pc = jumpTarget(instruction.args[1], error);
                    if (pc < 0) {
                        return false;
                    }
                } else {
                    ++pc;
                }
            } else if (op == "READ") {
                int value = 0;
                if (!(input >> value)) {
                    error << "运行错误: read 缺少输入值: " << instruction.args[0] << "\n";
                    return false;
                }
                values_[instruction.args[0]] = value;
                ++pc;
            } else if (op == "WRITE") {
                output << valueOf(instruction.args[0]) << '\n';
                ++pc;
            } else if (op == "CALL") {
                const auto found = procedures_.find(instruction.args[0]);
                if (found == procedures_.end()) {
                    error << "运行错误: 未找到过程入口: " << instruction.args[0] << "\n";
                    return false;
                }
                returnStack_.push_back(pc + 1);
                pc = found->second + 1;
            } else if (op == "RET") {
                if (returnStack_.empty()) {
                    return true;
                }
                pc = returnStack_.back();
                returnStack_.pop_back();
            } else {
                error << "运行错误: 未知目标指令: " << op << "\n";
                return false;
            }
        }

        error << "运行错误: 程序计数器越界\n";
        return false;
    }

private:
    int valueOf(const std::string& operand) {
        if (operand == "_" || operand.empty()) {
            return 0;
        }
        if (isIntegerLiteral(operand)) {
            return std::stoi(operand);
        }
        return values_[operand];
    }

    int jumpTarget(const std::string& label, std::ostream& error) const {
        const auto found = labels_.find(label);
        if (found == labels_.end()) {
            error << "运行错误: 未找到跳转标签: " << label << "\n";
            return -1;
        }
        return found->second;
    }

    bool compare(const std::string& op, int lhs, int rhs) const {
        if (op == "JE") {
            return lhs == rhs;
        }
        if (op == "JNE") {
            return lhs != rhs;
        }
        if (op == "JLT") {
            return lhs < rhs;
        }
        if (op == "JLE") {
            return lhs <= rhs;
        }
        if (op == "JGT") {
            return lhs > rhs;
        }
        if (op == "JGE") {
            return lhs >= rhs;
        }
        return false;
    }

    const TargetProgram& program_;
    std::unordered_map<std::string, int> labels_;
    std::unordered_map<std::string, int> procedures_;
    std::unordered_map<std::string, int> values_;
    std::vector<int> returnStack_;
};

}  // namespace

TargetProgram generateTargetProgram(const std::vector<Quad>& quads) {
    TargetProgram program;
    std::vector<bool> inProcedure = findProcedureBodyQuads(quads);

    std::vector<int> globalDeclarations;
    std::vector<int> procedureQuads;
    std::vector<int> mainQuads;

    for (std::size_t i = 0; i < quads.size(); ++i) {
        const int quadIndex = static_cast<int>(i) + 1;
        const Quad& quad = quads[i];
        if (inProcedure[i]) {
            procedureQuads.push_back(quadIndex);
        } else if (isGlobalDeclarationQuad(quad)) {
            globalDeclarations.push_back(quadIndex);
        } else {
            mainQuads.push_back(quadIndex);
        }
    }

    for (int quadIndex : globalDeclarations) {
        translateQuad(program, quadIndex, quads[quadIndex - 1]);
    }

    addInstruction(program, "", "JMP", {"MAIN"});

    for (int quadIndex : procedureQuads) {
        translateQuad(program, quadIndex, quads[quadIndex - 1]);
    }

    addInstruction(program, "MAIN", "NOP");
    for (int quadIndex : mainQuads) {
        translateQuad(program, quadIndex, quads[quadIndex - 1]);
    }

    return program;
}

void printTargetProgram(const TargetProgram& program, std::ostream& output) {
    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        const TargetInstruction& instruction = program.instructions[i];
        output << std::setw(4) << std::setfill('0') << (i + 1) << std::setfill(' ') << "  ";
        if (!instruction.label.empty()) {
            output << std::left << std::setw(7) << (instruction.label + ":") << std::right;
        } else {
            output << "       ";
        }
        output << instructionText(instruction) << '\n';
    }
}

bool runTargetProgram(const TargetProgram& program, std::istream& input, std::ostream& output, std::ostream& error) {
    TargetMachine machine(program);
    return machine.run(input, output, error);
}
