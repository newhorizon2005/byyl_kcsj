#include "TargetCode.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

/*
 * 匿名命名空间里的函数和类只在本 cpp 文件内部可见。
 * 这类似 Java/C# 中的 private 工具函数，避免污染外部接口。
 */

// 把四元式编号转成标签。第 8 条四元式对应目标代码标签 L8。
std::string quadLabel(int index) {
    return "L" + std::to_string(index);
}

/*
 * 把四元式跳转目标转成目标代码标签。
 *
 * Parser 生成四元式时用 "$8" 表示“跳到第 8 条四元式”。
 * 目标代码打印时使用更像汇编的 "L8"。
 */
std::string targetLabel(const std::string& quadTarget) {
    if (quadTarget.size() >= 2 && quadTarget[0] == '$') {
        return "L" + quadTarget.substr(1);
    }
    return quadTarget;
}

/*
 * 判断一个字符串是不是整数字面量。
 *
 * 目标机执行时，操作数可能是：
 * - "123"：立即数，直接转 int。
 * - "x"：变量名，需要去 values_ 表中取值。
 * - "T1"：临时变量名，也去 values_ 表中取值。
 */
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

// 向 TargetProgram 末尾追加一条指令。
void addInstruction(TargetProgram& program, const std::string& label, const std::string& op, std::vector<std::string> args = {}) {
    program.instructions.push_back(TargetInstruction{label, op, std::move(args)});
}

/*
 * 判断某条四元式是否属于“全局声明/初始化”。
 *
 * 生成目标程序时，需要先处理全局声明，再跳过过程体进入 MAIN。
 * 这些全局四元式不属于过程体，也不属于主程序普通语句。
 */
bool isGlobalDeclarationQuad(const Quad& quad) {
    return quad.op == "syss" || quad.op == "const" || quad.op == "var" || quad.op == "=";
}

/*
 * 标记哪些四元式属于过程体。
 *
 * 四元式中过程大致长这样：
 *
 *   procedure p
 *   ...过程体...
 *   ret
 *
 * 这个函数返回一个 bool 数组：
 * - true  表示对应四元式在某个 procedure 到 ret 之间。
 * - false 表示它是全局声明或主程序四元式。
 */
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

/*
 * translateQuad 把“一条四元式”翻译成“一条目标指令”。
 *
 * index 是四元式编号，作为标签来源：
 *   第 index 条四元式 -> 标签 Lindex
 *
 * 典型映射：
 *   (:=, a, _, x)     -> Lk: MOV a x
 *   (+, a, b, T1)     -> Lk: ADD a b T1
 *   (j>=, a, b, $10)  -> Lk: JGE a b L10
 */
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
        // 遇到未识别四元式时生成 NOP，保证目标代码仍有对应标签位置。
        addInstruction(program, label, "NOP");
    }
}

// 把一条目标指令拼成可打印文本，例如 "ADD a b T1"。
std::string instructionText(const TargetInstruction& instruction) {
    std::ostringstream out;
    out << instruction.op;
    for (const std::string& arg : instruction.args) {
        out << ' ' << arg;
    }
    return out.str();
}

/*
 * TargetMachine 是目标代码解释器。
 *
 * 它模拟一台很小的机器：
 * - pc            : 程序计数器，指向当前要执行的指令下标。
 * - labels_       : 标签表，L8 -> 指令下标。
 * - procedures_   : 过程入口表，p -> PROC p 所在指令下标。
 * - values_       : 变量/常量/临时变量的值表。
 * - returnStack_  : 过程调用返回地址栈。
 *
 * 所以 run 模式不是生成真实机器码后运行，
 * 而是用这个解释器逐条执行伪汇编指令。
 */
class TargetMachine {
public:
    /*
     * 构造目标机时先扫描整段目标程序，建立标签表和过程表。
     *
     * 为什么提前建表？
     * - JMP L10 执行时需要快速知道 L10 在 instructions 的哪个下标。
     * - CALL p 执行时需要快速知道过程 p 的入口。
     */
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

    /*
     * 执行目标程序。
     *
     * input  : read 指令从这里读整数，通常是 std::cin。
     * output : write 指令写到这里，通常是 std::cout。
     * error  : 运行错误写到这里，通常是 std::cerr。
     *
     * 主循环每轮执行 instructions[pc]：
     * - 普通指令执行后 pc++。
     * - 跳转指令会直接修改 pc。
     * - HALT 正常结束。
     */
    bool run(std::istream& input, std::ostream& output, std::ostream& error) {
        int pc = 0;
        int steps = 0;
        while (pc >= 0 && pc < static_cast<int>(program_.instructions.size())) {
            // 防止 while true 这类程序让解释器无限跑下去。
            if (++steps > 100000) {
                error << "运行错误: 执行步数超过上限，可能存在死循环\n";
                return false;
            }

            const TargetInstruction& instruction = program_.instructions[pc];
            const std::string& op = instruction.op;

            /*
             * START/DECL/PROC/NOP 本身不做复杂动作。
             *
             * DECL_CONST/DECL_VAR 会保证名字在 values_ 表中出现。
             * valueOf(name) 如果 name 还不存在，会通过 values_[name] 默认生成 0。
             */
            if (op == "START" || op == "DECL_CONST" || op == "DECL_VAR" || op == "NOP" || op == "PROC") {
                if ((op == "DECL_CONST" || op == "DECL_VAR") && !instruction.args.empty()) {
                    values_[instruction.args[0]] = valueOf(instruction.args[0]);
                }
                ++pc;
            } else if (op == "HALT") {
                // HALT 表示程序正常结束。
                return true;
            } else if (op == "MOV") {
                // MOV src dst：把 src 的值写入 dst。
                values_[instruction.args[1]] = valueOf(instruction.args[0]);
                ++pc;
            } else if (op == "ADD") {
                // ADD a b result：result = a + b。
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
                // 无条件跳转：pc 直接变成目标标签对应的指令下标。
                pc = jumpTarget(instruction.args[0], error);
                if (pc < 0) {
                    return false;
                }
            } else if (op == "JE" || op == "JNE" || op == "JLT" || op == "JLE" || op == "JGT" || op == "JGE") {
                // 条件跳转：先比较两个操作数，条件满足才跳，否则顺序执行下一条。
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
                // jnotodd 被翻译成 JEVEN：表达式值为偶数时跳转。
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
                // READ x：从输入流读一个整数，写到变量 x。
                if (!(input >> value)) {
                    error << "运行错误: read 缺少输入值: " << instruction.args[0] << "\n";
                    return false;
                }
                values_[instruction.args[0]] = value;
                ++pc;
            } else if (op == "WRITE") {
                // WRITE x：取出 x 的值并输出一行。
                output << valueOf(instruction.args[0]) << '\n';
                ++pc;
            } else if (op == "CALL") {
                /*
                 * CALL p：
                 * 1. 在 procedures_ 中找到过程 p 的 PROC 指令位置。
                 * 2. 把下一条指令 pc + 1 压入返回栈。
                 * 3. pc 跳到过程体第一条指令，也就是 PROC 后一条。
                 */
                const auto found = procedures_.find(instruction.args[0]);
                if (found == procedures_.end()) {
                    error << "运行错误: 未找到过程入口: " << instruction.args[0] << "\n";
                    return false;
                }
                returnStack_.push_back(pc + 1);
                pc = found->second + 1;
            } else if (op == "RET") {
                /*
                 * RET：
                 * - 如果返回栈为空，说明没有调用者，直接结束。
                 * - 否则弹出返回地址，让 pc 回到 call 后面的指令。
                 */
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
    /*
     * 取操作数的整数值。
     *
     * operand 可能是：
     * - "_" 或空字符串：占位，无实际值，返回 0。
     * - 整数字面量："123"、"-5"，直接 stoi。
     * - 变量/临时变量名："x"、"T1"，从 values_ 表中取。
     */
    int valueOf(const std::string& operand) {
        if (operand == "_" || operand.empty()) {
            return 0;
        }
        if (isIntegerLiteral(operand)) {
            return std::stoi(operand);
        }
        return values_[operand];
    }

    // 根据标签查找跳转目标指令下标。找不到就报告运行错误。
    int jumpTarget(const std::string& label, std::ostream& error) const {
        const auto found = labels_.find(label);
        if (found == labels_.end()) {
            error << "运行错误: 未找到跳转标签: " << label << "\n";
            return -1;
        }
        return found->second;
    }

    // 执行条件跳转中的关系比较。
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

    // 保存目标程序引用，不复制整段指令。
    const TargetProgram& program_;

    // 标签名 -> 指令下标。
    std::unordered_map<std::string, int> labels_;

    // 过程名 -> PROC 指令下标。
    std::unordered_map<std::string, int> procedures_;

    // 所有变量、常量、临时变量的当前值。
    std::unordered_map<std::string, int> values_;

    // 过程调用返回地址栈。
    std::vector<int> returnStack_;
};

}  // namespace

/*
 * generateTargetProgram 把四元式序列变成目标指令序列。
 *
 * 为什么不能简单按四元式顺序一条条翻译？
 *
 * 因为四元式里过程声明通常出现在主程序前面：
 *
 *   syss
 *   procedure p
 *   ...过程体...
 *   ret
 *   ...主程序...
 *   syse
 *
 * 如果目标机从第一条顺序执行，就会直接进入过程体，这是不对的。
 * 正确做法：
 * 1. 先执行全局声明。
 * 2. JMP MAIN 跳过所有过程体。
 * 3. 保留过程体，供 CALL 使用。
 * 4. MAIN 标签后放主程序指令。
 */
TargetProgram generateTargetProgram(const std::vector<Quad>& quads) {
    TargetProgram program;

    // 标记每条四元式是否属于过程体。
    std::vector<bool> inProcedure = findProcedureBodyQuads(quads);

    std::vector<int> globalDeclarations;
    std::vector<int> procedureQuads;
    std::vector<int> mainQuads;

    // 把四元式编号分到三类数组里。quadIndex 使用 1 基编号。
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

    // 1. 先翻译全局声明。
    for (int quadIndex : globalDeclarations) {
        translateQuad(program, quadIndex, quads[quadIndex - 1]);
    }

    // 2. 程序启动时跳过过程体，直接进入主程序。
    addInstruction(program, "", "JMP", {"MAIN"});

    // 3. 翻译过程体，CALL 时会跳到这里。
    for (int quadIndex : procedureQuads) {
        translateQuad(program, quadIndex, quads[quadIndex - 1]);
    }

    // 4. 主程序入口。
    addInstruction(program, "MAIN", "NOP");
    for (int quadIndex : mainQuads) {
        translateQuad(program, quadIndex, quads[quadIndex - 1]);
    }

    return program;
}

// 打印目标代码，格式类似：0001  L1:    START
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

// 对外执行接口：创建 TargetMachine，然后调用它的 run()。
bool runTargetProgram(const TargetProgram& program, std::istream& input, std::ostream& output, std::ostream& error) {
    TargetMachine machine(program);
    return machine.run(input, output, error);
}
