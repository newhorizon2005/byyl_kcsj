#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../common/Token.h"

/*
 * SymbolKind 表示符号表中名字的种类。
 *
 * PL/0 中同样是一个标识符名字，它可能代表：
 * - 常量 const
 * - 变量 var
 * - 过程 procedure
 *
 * 语义检查需要区分这些种类，例如：
 * - 常量不能被赋值。
 * - 过程不能出现在表达式里。
 * - call 后面必须是过程名。
 */
enum class SymbolKind {
    Const,
    Var,
    Procedure
};

/*
 * Symbol 是符号表的一项。
 *
 * name  : 标识符名字。
 * kind  : 常量、变量或过程。
 * value : 常量值或变量初始值；过程通常为空。
 * level : 作用域层级。0 表示全局层，进入过程后层级加深。
 */
struct Symbol {
    std::string name;
    SymbolKind kind;
    std::string value;
    int level;
};

/*
 * Quad 是四元式，也叫中间代码。
 *
 * 四元式格式：
 *
 *   (op, arg1, arg2, result)
 *
 * 例子：
 * - (var, x, _, _)       声明变量 x
 * - (:=, 5, _, x)        x := 5
 * - (+, a, b, T1)        T1 = a + b
 * - (j>=, a, b, $10)     如果 a >= b，跳转到第 10 条四元式
 *
 * "_" 表示该位置不用。
 */
struct Quad {
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
};

// 语义错误，例如未声明、重复声明、常量被赋值等。
struct SemanticError {
    int line;
    std::string message;
};

/*
 * SemanticAnalyzer 负责语义检查和四元式生成。
 *
 * 它不主动遍历 Token，也不主动解析文法；
 * Parser 在识别到某个语法结构时调用它。
 *
 * 例如 Parser 读到：
 *   var x;
 * 就调用 declare(x, Var) 并 emit("var", "x", "_", "_")。
 */
class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    // 进入/退出作用域。过程声明会创建新的局部作用域。
    void enterScope();
    void exitScope();

    // 声明一个符号；如果当前作用域已经有同名符号，返回 false 并记录错误。
    bool declare(const Token& token, SymbolKind kind, const std::string& value = "");

    // 从当前作用域向外查找名字。
    const Symbol* lookup(const std::string& name) const;

    // 只在当前作用域查找名字，用于重复声明检查。
    const Symbol* lookupCurrent(const std::string& name) const;

    // 下面这些 require 函数是语义约束检查。
    void requireDeclared(const Token& token);
    void requireAssignable(const Token& token);
    void requireReadable(const Token& token);
    void requireCallable(const Token& token);
    void requireValue(const Token& token);

    // 追加一条四元式，返回它的 1 基编号。
    int emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result);

    // 回填四元式 result 字段，主要用于 if/while 的跳转目标。
    void patchResult(int quadIndex, const std::string& result);

    // 返回下一条四元式的 1 基编号。
    int nextQuadIndex() const;

    // 生成临时变量名 T1、T2、...
    std::string newTemp();

    // 记录语义错误。
    void addError(int line, const std::string& message);

    // 只读访问结果，供 main.cpp 打印。
    const std::vector<SemanticError>& errors() const;
    const std::vector<Quad>& quads() const;
    const std::vector<Symbol>& symbolsInOrder() const;

private:
    /*
     * 作用域栈。
     *
     * scopes_.back() 是当前作用域。
     * 每个作用域用 unordered_map 保存“名字 -> 符号”。
     */
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;

    // 按声明顺序保存所有符号，便于输出符号表时保持稳定顺序。
    std::vector<Symbol> symbolsInOrder_;

    // 语义错误列表。
    std::vector<SemanticError> errors_;

    // 生成出来的四元式序列。
    std::vector<Quad> quads_;

    // 临时变量计数器，newTemp() 每调用一次加一。
    int tempCounter_ = 0;
};

// 把符号种类转成字符串，供输出符号表使用。
std::string symbolKindName(SymbolKind kind);
