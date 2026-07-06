#include "Semantic.h"

#include <utility>

/*
 * 构造语义分析器。
 *
 * 一开始就创建全局作用域：
 * - 全局 const/var/procedure 都放在第 0 层。
 * - 后续进入过程时再 push 新作用域。
 */
SemanticAnalyzer::SemanticAnalyzer() {
    enterScope();
}

// 新建一个空作用域，压入作用域栈顶。
void SemanticAnalyzer::enterScope() {
    scopes_.emplace_back();
}

/*
 * 退出当前作用域。
 *
 * 这里保护全局作用域不被弹掉：
 * 如果 scopes_.size() == 1，说明只剩全局层，不执行 pop_back()。
 */
void SemanticAnalyzer::exitScope() {
    if (scopes_.size() > 1) {
        scopes_.pop_back();
    }
}

/*
 * 声明一个符号。
 *
 * 执行流程：
 * 1. 只查当前作用域 lookupCurrent()，判断是否重复声明。
 *    注意：允许内层作用域声明与外层同名的符号，这叫遮蔽。
 * 2. 如果重复，记录错误并返回 false。
 * 3. 构造 Symbol，其中 level = 当前作用域下标。
 * 4. 写入当前作用域的 map，便于后续查找。
 * 5. 同时追加到 symbolsInOrder_，便于最终按声明顺序打印。
 */
bool SemanticAnalyzer::declare(const Token& token, SymbolKind kind, const std::string& value) {
    if (lookupCurrent(token.lexeme) != nullptr) {
        addError(token.line, "重复声明: " + token.lexeme);
        return false;
    }

    Symbol symbol{token.lexeme, kind, value, static_cast<int>(scopes_.size()) - 1};
    scopes_.back()[token.lexeme] = symbol;
    symbolsInOrder_.push_back(symbol);
    return true;
}

/*
 * lookup 从内层作用域向外层作用域查找名字。
 *
 * 例如过程内部访问 x：
 * - 先查过程自己的局部作用域。
 * - 找不到再查全局作用域。
 *
 * 返回指针：
 * - 找到：返回 Symbol 地址。
 * - 找不到：返回 nullptr。
 */
const Symbol* SemanticAnalyzer::lookup(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto it = scope->find(name);
        if (it != scope->end()) {
            return &it->second;
        }
    }
    return nullptr;
}

// 只查当前作用域。主要用于“同一层不能重复声明”的检查。
const Symbol* SemanticAnalyzer::lookupCurrent(const std::string& name) const {
    if (scopes_.empty()) {
        return nullptr;
    }
    const auto it = scopes_.back().find(name);
    if (it == scopes_.back().end()) {
        return nullptr;
    }
    return &it->second;
}

// 要求某个标识符已经声明过；否则记录“未声明标识符”。
void SemanticAnalyzer::requireDeclared(const Token& token) {
    if (lookup(token.lexeme) == nullptr) {
        addError(token.line, "未声明标识符: " + token.lexeme);
    }
}

/*
 * 要求某个标识符可以被赋值。
 *
 * 合法情况：它必须存在，并且 kind == Var。
 *
 * 不合法例子：
 * - x 没声明：x := 1
 * - c 是常量：const c=1; c := 2
 * - p 是过程：procedure p; ...; p := 1
 */
void SemanticAnalyzer::requireAssignable(const Token& token) {
    const Symbol* symbol = lookup(token.lexeme);
    if (symbol == nullptr) {
        addError(token.line, "未声明标识符: " + token.lexeme);
        return;
    }
    if (symbol->kind != SymbolKind::Var) {
        addError(token.line, "不可赋值标识符: " + token.lexeme);
    }
}

// read(x) 会把输入写入 x，所以 read 参数和赋值左侧一样，必须是变量。
void SemanticAnalyzer::requireReadable(const Token& token) {
    requireAssignable(token);
}

// call 后面必须是已经声明的过程名。
void SemanticAnalyzer::requireCallable(const Token& token) {
    const Symbol* symbol = lookup(token.lexeme);
    if (symbol == nullptr) {
        addError(token.line, "未声明过程: " + token.lexeme);
        return;
    }
    if (symbol->kind != SymbolKind::Procedure) {
        addError(token.line, "不可调用标识符: " + token.lexeme);
    }
}

/*
 * 要求某个标识符能作为表达式的值。
 *
 * 常量和变量都可以作为值：
 *   x + 1
 *   c + 1
 *
 * 过程名不能作为值：
 *   p + 1   // 错
 */
void SemanticAnalyzer::requireValue(const Token& token) {
    const Symbol* symbol = lookup(token.lexeme);
    if (symbol == nullptr) {
        addError(token.line, "未声明标识符: " + token.lexeme);
        return;
    }
    if (symbol->kind == SymbolKind::Procedure) {
        addError(token.line, "过程名不能作为表达式值: " + token.lexeme);
    }
}

/*
 * 追加一条四元式。
 *
 * 返回值是四元式编号，采用 1 基编号：
 * - vector 下标是 0 基。
 * - 但报告/输出中更习惯从 1 开始编号。
 *
 * 这个返回值常用于 if/while：
 * 先生成 result 为 "$?" 的跳转四元式，
 * 之后拿编号调用 patchResult() 回填真实目标。
 */
int SemanticAnalyzer::emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result) {
    quads_.push_back(Quad{op, arg1, arg2, result});
    return static_cast<int>(quads_.size());
}

// 回填某条四元式的 result 字段。quadIndex 是 1 基编号，所以访问 vector 时要减 1。
void SemanticAnalyzer::patchResult(int quadIndex, const std::string& result) {
    if (quadIndex >= 1 && quadIndex <= static_cast<int>(quads_.size())) {
        quads_[quadIndex - 1].result = result;
    }
}

// 下一条四元式编号 = 当前已有数量 + 1。
int SemanticAnalyzer::nextQuadIndex() const {
    return static_cast<int>(quads_.size()) + 1;
}

// 生成临时变量名。表达式中间结果会存到 T1、T2 这类名字里。
std::string SemanticAnalyzer::newTemp() {
    ++tempCounter_;
    return "T" + std::to_string(tempCounter_);
}

// 记录语义错误。
void SemanticAnalyzer::addError(int line, const std::string& message) {
    errors_.push_back(SemanticError{line, message});
}

// 以下三个函数返回只读引用，避免复制内部数组。
const std::vector<SemanticError>& SemanticAnalyzer::errors() const {
    return errors_;
}

const std::vector<Quad>& SemanticAnalyzer::quads() const {
    return quads_;
}

const std::vector<Symbol>& SemanticAnalyzer::symbolsInOrder() const {
    return symbolsInOrder_;
}

// 把 SymbolKind 转成输出用字符串。
std::string symbolKindName(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Const: return "const";
        case SymbolKind::Var: return "var";
        case SymbolKind::Procedure: return "procedure";
    }
    return "unknown";
}
