#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "../common/Token.h"
#include "Parser.h"

/*
 * SLRTraceRow 保存 SLR 分析过程中的一行追踪信息。
 *
 * SLR 分析是“栈驱动”的：
 * - 状态栈保存自动机状态编号。
 * - 符号栈保存已经移进/归约出来的语法符号。
 * - input 保存还没处理的输入。
 * - action 显示本步执行移进、归约、接受或错误。
 *
 * printTrace() 会把这些行打印成表格，便于课程演示。
 */
struct SLRTraceRow {
    int step;
    std::string stateStack;
    std::string symbolStack;
    std::string input;
    std::string action;
};

/*
 * SLRParser 是自动机构造版语法分析器。
 *
 * 它和 Parser.cpp 中的递归下降 Parser 对比：
 * - Parser：每个非终结符一个函数，按文法手写递归调用。
 * - SLRParser：先构造 LR(0) 项目集、ACTION 表、GOTO 表，再用状态栈分析。
 *
 * 当前项目中 SLRParser 主要用于：
 * - parse-lr：输出 SLR 分析过程。
 * - sem-lr：先用 SLR 检查语法，通过后再调用递归下降流程生成语义结果。
 *
 * 注意：SLRParser 本身不生成四元式，四元式仍由 Parser + SemanticAnalyzer 生成。
 */
class SLRParser {
public:
    // 接收 Lexer 输出的 Token 序列。
    explicit SLRParser(std::vector<Token> tokens);

    /*
     * 执行 SLR 分析，成功返回 true。
     *
     * parse() 内部会：
     * - 把 Token 转成 ident、number、if、+ 这类文法终结符。
     * - 使用缓存的 ACTION/GOTO 表驱动状态栈。
     * - 把每一步查表结果保存到 trace_，供 printTrace() 打印。
     */
    bool parse();

    // 打印分析过程表；必须先调用 parse()，否则 trace_ 为空。
    void printTrace(std::ostream& output) const;

    // 返回语法错误列表。
    const std::vector<SyntaxError>& syntaxErrors() const;

    /*
     * 导出 LR(0) 项目集状态图的 Graphviz DOT 文本。
     *
     * 输出的是“项目集自动机”，不是语法分析树：
     * - 节点 Ii 表示一个 LR(0) 项目集。
     * - 边标签 symbol 表示 GOTO(Ii, symbol)。
     */
    static void writeDot(std::ostream& output);

private:
    // 预留的实现隐藏结构；当前主要数据直接放在 cpp 内部匿名命名空间中。
    struct Impl;

    // 输入 Token。
    std::vector<Token> tokens_;

    // SLR 分析错误。
    std::vector<SyntaxError> syntaxErrors_;

    // SLR 分析过程追踪。
    std::vector<SLRTraceRow> trace_;
};
