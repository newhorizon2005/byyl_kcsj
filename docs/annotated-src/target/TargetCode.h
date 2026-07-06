#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "../semantic/Semantic.h"

/*
 * TargetInstruction 是“目标代码”的一条指令。
 *
 * 这里的目标代码不是 x86/ARM 真实机器码，而是本项目自定义的一套
 * 便于展示和解释的伪汇编指令。
 *
 * label : 指令标签，例如 L12、MAIN。跳转指令会跳到标签。
 * op    : 操作码，例如 MOV、ADD、JMP、READ、WRITE。
 * args  : 参数列表，不同 op 的参数个数不同。
 *
 * 例子：
 *   label = "L5", op = "ADD", args = {"a", "b", "T1"}
 * 表示：
 *   L5: ADD a b T1
 * 含义：
 *   T1 = a + b
 */
struct TargetInstruction {
    std::string label;
    std::string op;
    std::vector<std::string> args;
};

/*
 * TargetProgram 是目标指令序列。
 *
 * 四元式是中间代码；
 * TargetProgram 是更接近“能执行”的代码。
 */
struct TargetProgram {
    std::vector<TargetInstruction> instructions;
};

// 把四元式序列翻译成目标指令序列。
TargetProgram generateTargetProgram(const std::vector<Quad>& quads);

// 按带行号的形式打印目标指令。
void printTargetProgram(const TargetProgram& program, std::ostream& output);

// 用内置解释器执行目标指令，支持 read/write/call/while 等效果。
bool runTargetProgram(const TargetProgram& program, std::istream& input, std::ostream& output, std::ostream& error);
