#include "SLRParser.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace {

/*
 * 下面几个常量是 SLR 文法构造中常用的特殊符号。
 *
 * kAugmentedStart:
 *   增广开始符号 S'。LR 分析通常会把原文法：
 *     Program -> ...
 *   增广成：
 *     S' -> Program
 *   当项目 S' -> Program · 遇到输入结束符 $ 时，表示接受。
 *
 * kStart:
 *   原文法开始符号。
 *
 * kEnd:
 *   输入结束符，相当于 EOF，在 LR 表里记作 "$"。
 *
 * kEpsilon:
 *   空串，用于打印 FIRST 集或产生式显示；产生式右部为空 vector 表示 epsilon。
 */
constexpr const char* kAugmentedStart = "S'";
constexpr const char* kStart = "Program";
constexpr const char* kEnd = "$";
constexpr const char* kEpsilon = "epsilon";

/*
 * Production 表示一条产生式。
 *
 * 例如：
 *   Expression -> SignOpt Term ExprTail
 *
 * lhs = "Expression"
 * rhs = {"SignOpt", "Term", "ExprTail"}
 */
struct Production {
    std::string lhs;
    std::vector<std::string> rhs;
};

/*
 * Item 表示 LR(0) 项目。
 *
 * LR 项目的“点”表示当前识别到产生式的哪个位置。
 *
 * 例如产生式：
 *   Expression -> SignOpt Term ExprTail
 *
 * dot = 0: Expression -> · SignOpt Term ExprTail
 * dot = 1: Expression -> SignOpt · Term ExprTail
 * dot = 3: Expression -> SignOpt Term ExprTail ·
 *
 * production 是 productions 数组中的下标。
 * dot 是点在 rhs 中的位置。
 */
struct Item {
    int production = 0;
    int dot = 0;

    // std::find 比较项目集时需要判断两个 Item 是否完全相同。
    bool operator==(const Item& other) const {
        return production == other.production && dot == other.dot;
    }

    // std::set<Item> 需要排序规则；先按产生式编号，再按点位置排序。
    bool operator<(const Item& other) const {
        if (production != other.production) {
            return production < other.production;
        }
        return dot < other.dot;
    }
};

/*
 * Action 表示 ACTION 表中的动作。
 *
 * SLR 分析表分两部分：
 * - ACTION[state, terminal]：遇到终结符时做什么。
 * - GOTO[state, nonterminal]：归约出非终结符后转到哪个状态。
 *
 * ACTION 的三种类型：
 * - Shift  : 移进，value 是目标状态。
 * - Reduce : 归约，value 是使用的产生式编号。
 * - Accept : 接受。
 */
struct Action {
    enum class Kind {
        Shift,
        Reduce,
        Accept
    };

    Kind kind;
    int value = 0;
};

/*
 * GrammarData 集中保存 SLR 分析需要的所有数据。
 *
 * productions  : 产生式数组。
 * terminals    : 终结符集合，例如 ident、number、+、while。
 * nonterminals : 非终结符集合，例如 Statement、Expression。
 * byLhs        : 按产生式左部建立索引，closure 时快速找到某非终结符的所有产生式。
 * first/follow : FIRST 集和 FOLLOW 集。
 * states       : LR(0) 项目集族，每个状态是一个 set<Item>。
 * transitions  : 项目集自动机边，(state, symbol) -> nextState。
 * actionTable  : SLR ACTION 表。
 * gotoTable    : SLR GOTO 表。
 */
struct GrammarData {
    std::vector<Production> productions;
    std::set<std::string> terminals;
    std::set<std::string> nonterminals;
    std::unordered_map<std::string, std::vector<int>> byLhs;
    std::map<std::string, std::set<std::string>> first;
    std::map<std::string, std::set<std::string>> follow;
    std::vector<std::set<Item>> states;
    std::map<std::pair<int, std::string>, int> transitions;
    std::map<std::pair<int, std::string>, Action> actionTable;
    std::map<std::pair<int, std::string>, int> gotoTable;
};

/*
 * makeProductions 写死本项目 SLR 版本的 PL/0 文法。
 *
 * 它和递归下降 Parser 的文法等价，但为了更适合 LR 构表，
 * 把一些“可选”和“重复”结构改写成了显式非终结符：
 *
 * - [ConstDecl] 变成 ConstPart -> const ... | epsilon
 * - { "," ident } 变成 IdentListTail -> "," ident IdentListTail | epsilon
 * - { "+" Term } 变成 ExprTail -> "+" Term ExprTail | ... | epsilon
 *
 * 空产生式用 rhs 为空 vector 表示，例如 {"ConstPart", {}}。
 *
 * 下面这个数组的下标就是“产生式编号”，后面 trace 里的 r16、r37 等
 * 都是引用这里的编号。所以不要把它只看成普通列表，它同时承担了
 * LR 分析表中 reduce 动作的索引作用。
 *
 * 编号大致分组：
 * - 0      : 增广文法入口 S' -> Program。
 * - 1~2    : Program、Block 的主结构。
 * - 3~12   : const/var 声明以及逗号列表。
 * - 13~15  : procedure 声明。
 * - 16~28  : 各类 Statement、StatementList、ExprList。
 * - 29~36  : Condition 和关系运算符。
 * - 37~50  : Expression、Term、Factor。
 */
std::vector<Production> makeProductions() {
    return {
        {kAugmentedStart, {kStart}},
        {kStart, {"Block", "."}},
        {"Block", {"ConstPart", "VarPart", "ProcPart", "Statement"}},
        {"ConstPart", {"const", "ConstDefs", ";"}},
        {"ConstPart", {}},
        {"ConstDefs", {"ident", "=", "number", "ConstDefsTail"}},
        {"ConstDefsTail", {",", "ident", "=", "number", "ConstDefsTail"}},
        {"ConstDefsTail", {}},
        {"VarPart", {"var", "IdentList", ";"}},
        {"VarPart", {}},
        {"IdentList", {"ident", "IdentListTail"}},
        {"IdentListTail", {",", "ident", "IdentListTail"}},
        {"IdentListTail", {}},
        {"ProcPart", {"ProcDecl", "ProcPart"}},
        {"ProcPart", {}},
        {"ProcDecl", {"procedure", "ident", ";", "Block", ";"}},
        {"Statement", {"ident", ":=", "Expression"}},
        {"Statement", {"call", "ident"}},
        {"Statement", {"begin", "Statement", "StatementList", "end"}},
        {"Statement", {"if", "Condition", "then", "Statement"}},
        {"Statement", {"while", "Condition", "do", "Statement"}},
        {"Statement", {"read", "(", "IdentList", ")"}},
        {"Statement", {"write", "(", "ExprList", ")"}},
        {"Statement", {}},
        {"StatementList", {";", "Statement", "StatementList"}},
        {"StatementList", {}},
        {"ExprList", {"Expression", "ExprListTail"}},
        {"ExprListTail", {",", "Expression", "ExprListTail"}},
        {"ExprListTail", {}},
        {"Condition", {"odd", "Expression"}},
        {"Condition", {"Expression", "RelOp", "Expression"}},
        {"RelOp", {"="}},
        {"RelOp", {"#"}},
        {"RelOp", {"<"}},
        {"RelOp", {"<="}},
        {"RelOp", {">"}},
        {"RelOp", {">="}},
        {"Expression", {"SignOpt", "Term", "ExprTail"}},
        {"SignOpt", {"+"}},
        {"SignOpt", {"-"}},
        {"SignOpt", {}},
        {"ExprTail", {"+", "Term", "ExprTail"}},
        {"ExprTail", {"-", "Term", "ExprTail"}},
        {"ExprTail", {}},
        {"Term", {"Factor", "TermTail"}},
        {"TermTail", {"*", "Factor", "TermTail"}},
        {"TermTail", {"/", "Factor", "TermTail"}},
        {"TermTail", {}},
        {"Factor", {"ident"}},
        {"Factor", {"number"}},
        {"Factor", {"(", "Expression", ")"}},
    };
}

/*
 * 把 Lexer 的 TokenType 映射成 SLR 文法中的终结符字符串。
 *
 * 例如：
 * - TokenType::Identifier -> "ident"
 * - TokenType::Number     -> "number"
 * - TokenType::While      -> "while"
 *
 * 这样 SLR 分析器不关心标识符具体名字，只关心它的类别。
 * 具体名字、数值和行列号仍保存在 Token 里；SLR 这里只做语法结构判断，
 * 不做符号表或四元式生成。
 *
 * Invalid 被映射为 "invalid"，parse() 中会先检测词法错误并直接停止，
 * 正常不会把 invalid 送进 LR 表继续分析。
 */
std::string tokenToTerminal(const Token& token) {
    switch (token.type) {
        case TokenType::Identifier: return "ident";
        case TokenType::Number: return "number";
        case TokenType::Const: return "const";
        case TokenType::Var: return "var";
        case TokenType::Procedure: return "procedure";
        case TokenType::Call: return "call";
        case TokenType::Begin: return "begin";
        case TokenType::End: return "end";
        case TokenType::If: return "if";
        case TokenType::Then: return "then";
        case TokenType::While: return "while";
        case TokenType::Do: return "do";
        case TokenType::Odd: return "odd";
        case TokenType::Read: return "read";
        case TokenType::Write: return "write";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Times: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Equal: return "=";
        case TokenType::NotEqual: return "#";
        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";
        case TokenType::Assign: return ":=";
        case TokenType::Comma: return ",";
        case TokenType::Semicolon: return ";";
        case TokenType::LParen: return "(";
        case TokenType::RParen: return ")";
        case TokenType::Period: return ".";
        case TokenType::EndOfFile: return kEnd;
        case TokenType::Invalid: return "invalid";
    }
    return "invalid";
}

// 把产生式转成可读字符串，用于 SLR trace 和 DOT 图。
// 例如 rhs 为空时输出 "Statement -> epsilon"，否则按空格连接右部符号。
std::string productionToString(const Production& production) {
    std::ostringstream out;
    out << production.lhs << " -> ";
    if (production.rhs.empty()) {
        out << kEpsilon;
    } else {
        for (std::size_t i = 0; i < production.rhs.size(); ++i) {
            if (i != 0) {
                out << ' ';
            }
            out << production.rhs[i];
        }
    }
    return out.str();
}

// 转义 Graphviz DOT 标签中的特殊字符。
// DOT 标签里双引号、反斜杠、换行都需要特殊处理，否则导出的状态图会语法错误。
std::string dotEscape(const std::string& text) {
    std::string escaped;
    for (char c : text) {
        switch (c) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "    ";
                break;
            default:
                escaped.push_back(c);
                break;
        }
    }
    return escaped;
}

// 把 LR(0) 项目转成带点的字符串，例如 Statement -> ident · := Expression。
// item.dot 的取值范围是 [0, rhs.size()]，等于 rhs.size() 表示点已经在末尾。
std::string itemToString(const Item& item, const std::vector<Production>& productions) {
    const Production& production = productions[item.production];
    std::ostringstream out;
    out << production.lhs << " -> ";
    if (production.rhs.empty()) {
        out << "·";
    } else {
        for (std::size_t i = 0; i <= production.rhs.size(); ++i) {
            if (i != 0) {
                out << ' ';
            }
            if (static_cast<int>(i) == item.dot) {
                out << "·";
                if (i != production.rhs.size()) {
                    out << ' ';
                }
            }
            if (i < production.rhs.size()) {
                out << production.rhs[i];
            }
        }
    }
    return out.str();
}

// 把 ACTION 表动作转成中文说明，便于 printTrace() 输出。
std::string actionToString(const Action& action, const std::vector<Production>& productions) {
    std::ostringstream out;
    switch (action.kind) {
        case Action::Kind::Shift:
            out << "移进 s" << action.value;
            break;
        case Action::Kind::Reduce:
            out << "归约 r" << action.value << " (" << productionToString(productions[action.value]) << ")";
            break;
        case Action::Kind::Accept:
            out << "接受";
            break;
    }
    return out.str();
}

// 把状态栈格式化成 "[0 3 8]"。
std::string joinStates(const std::vector<int>& states) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (i != 0) {
            out << " ";
        }
        out << states[i];
    }
    out << "]";
    return out.str();
}

// 把符号栈格式化成 "[$ begin Statement]"。
std::string joinSymbols(const std::vector<std::string>& symbols) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i != 0) {
            out << " ";
        }
        out << symbols[i];
    }
    out << "]";
    return out.str();
}

// 把当前位置 pos 之后的输入符号拼成字符串。
std::string remainingInput(const std::vector<std::string>& input, std::size_t pos) {
    std::ostringstream out;
    for (std::size_t i = pos; i < input.size(); ++i) {
        if (i != pos) {
            out << " ";
        }
        out << input[i];
    }
    return out.str();
}

/*
 * closure 计算 LR(0) 项目集闭包。
 *
 * 直观理解：
 * 如果项目中有：
 *   A -> α · B β
 * 点后面是非终结符 B，说明接下来可能要识别 B。
 * 那么 B 的所有产生式都应该加入项目集：
 *   B -> · γ
 *
 * 这个过程可能继续引入新的非终结符，所以用 while(changed) 迭代到稳定。
 */
std::set<Item> closure(const std::set<Item>& items, const GrammarData& grammar) {
    // result 从传入的核心项目开始。后面新增的项目都会插入这里。
    std::set<Item> result = items;

    // closure 是一个“不动点”计算：只要本轮能推出新项目，就还要再扫一轮。
    // 例如 A -> · B 加入了 B -> · C，下一轮还要继续由 C 展开。
    bool changed = true;
    while (changed) {
        changed = false;

        // 不能直接在遍历 result 时插入新元素，所以复制一份快照。
        // 新插入的项目会在下一轮 while 中被处理。
        std::vector<Item> snapshot(result.begin(), result.end());
        for (const Item& item : snapshot) {
            const Production& production = grammar.productions[item.production];

            // 点已经到右部末尾：
            //   A -> α ·
            // 这类项目表示“可以归约”，不会再引入新项目。
            if (item.dot >= static_cast<int>(production.rhs.size())) {
                continue;
            }

            // 找到点后面的符号。
            const std::string& symbol = production.rhs[item.dot];

            // 点后是终结符时不能展开，只有非终结符才能展开产生式。
            if (grammar.nonterminals.count(symbol) == 0) {
                continue;
            }

            // byLhs[symbol] 保存所有左部为 symbol 的产生式编号。
            // 例如 symbol == "Statement" 时，会取到赋值、call、begin、if 等所有分支。
            const auto found = grammar.byLhs.find(symbol);
            if (found == grammar.byLhs.end()) {
                continue;
            }
            for (int productionIndex : found->second) {
                // 加入 B -> · γ。
                // set::insert(...).second 为 true 表示这是新项目，闭包还没稳定。
                if (result.insert(Item{productionIndex, 0}).second) {
                    changed = true;
                }
            }
        }
    }
    return result;
}

/*
 * gotoItems 计算 GOTO(I, X)。
 *
 * 含义：
 * 在项目集 I 中，如果某些项目的点后面正好是符号 X，
 * 就把这些项目的点向右移动一格，再对结果求 closure。
 *
 * 例如：
 *   A -> α · X β
 * 读入 X 后变成：
 *   A -> α X · β
 */
std::set<Item> gotoItems(const std::set<Item>& items, const std::string& symbol, const GrammarData& grammar) {
    std::set<Item> moved;
    for (const Item& item : items) {
        const Production& production = grammar.productions[item.production];

        // 只有点后正好是 symbol 的项目，才可以在读入 symbol 后移动点。
        // 点后不是 symbol 的项目，对 GOTO(I, symbol) 没有贡献。
        if (item.dot < static_cast<int>(production.rhs.size()) && production.rhs[item.dot] == symbol) {
            moved.insert(Item{item.production, item.dot + 1});
        }
    }

    // moved 为空说明从当前项目集读入 symbol 没有任何合法转移。
    if (moved.empty()) {
        return moved;
    }

    // 点移动后，新的点后面可能出现非终结符，所以必须再求 closure。
    // 这一步对应 LR(0) 自动机边的目标状态。
    return closure(moved, grammar);
}

/*
 * 计算一个符号序列从 start 开始的 FIRST 集。
 *
 * FIRST(β) 表示 β 能推导出的串的第一个终结符集合。
 * 如果 β 可以推出空串，则包含 epsilon。
 *
 * FOLLOW 集计算时会用到 FIRST(β)。
 */
std::set<std::string> firstOfSequence(const std::vector<std::string>& sequence, std::size_t start, const GrammarData& grammar) {
    std::set<std::string> result;

    // allNullable 表示从 start 开始看到目前为止，所有符号都能推出 epsilon。
    // 如果整段都可空，最后 FIRST(这段序列) 也要包含 epsilon。
    bool allNullable = true;
    for (std::size_t i = start; i < sequence.size(); ++i) {
        const std::string& symbol = sequence[i];
        const auto found = grammar.first.find(symbol);

        // 正常情况下终结符和非终结符都会提前放进 grammar.first。
        // 这里是兜底：如果没找到，就把 symbol 当作一个终结符处理。
        if (found == grammar.first.end()) {
            result.insert(symbol);
            allNullable = false;
            break;
        }

        // FIRST(X) 中除了 epsilon 之外的终结符都属于 FIRST(当前序列)。
        for (const std::string& terminal : found->second) {
            if (terminal != kEpsilon) {
                result.insert(terminal);
            }
        }

        // 如果当前符号不能推出 epsilon，序列的第一个终结符已经被确定在这里，
        // 后面的符号不会影响 FIRST，直接停止。
        if (found->second.count(kEpsilon) == 0) {
            allNullable = false;
            break;
        }
    }

    // start == sequence.size() 时表示空后缀，FIRST(空后缀) = {epsilon}。
    // FOLLOW 计算经常会用到这种情况，例如 A -> α B。
    if (allNullable) {
        result.insert(kEpsilon);
    }
    return result;
}

/*
 * 向 ACTION 表添加动作。
 *
 * 理想 LR 文法中，同一个 (state, terminal) 只有一个动作。
 * 但本项目文法中有空语句等可空结构，可能出现轻微冲突。
 *
 * 这里的处理策略：
 * - 如果没有旧动作，直接加入。
 * - 如果新旧动作完全相同，忽略。
 * - 如果旧动作是 Reduce、新动作是 Shift，优先 Shift。
 *
 * 这个策略是为了避免在真正的语句开头前过早把 Statement 归约成 epsilon。
 */
void addAction(GrammarData& grammar, int state, const std::string& terminal, const Action& action) {
    const auto key = std::make_pair(state, terminal);
    const auto existing = grammar.actionTable.find(key);

    // 表格这个位置还没有动作，直接写入。
    if (existing == grammar.actionTable.end()) {
        grammar.actionTable[key] = action;
        return;
    }

    // 重复写入完全相同的动作没有影响，忽略即可。
    if (existing->second.kind == action.kind && existing->second.value == action.value) {
        return;
    }

    // 本项目 PL/0 文法里有空语句：
    //   Statement -> epsilon
    // 因此在 begin 后、分号后等位置，可能同时出现：
    //   1. 归约成空语句。
    //   2. 移进真正的语句开头，例如 ident、if、while。
    //
    // 为了不在真实语句前过早归约为空语句，这里采用 shift 优先。
    // 如果旧动作已经是 Shift、新动作是 Reduce，下面不会覆盖，也等价于 shift 优先。
    if (existing->second.kind == Action::Kind::Reduce && action.kind == Action::Kind::Shift) {
        grammar.actionTable[key] = action;
    }
}

/*
 * buildGrammar 构造完整 SLR 分析数据。
 *
 * 总流程：
 * 1. 建立产生式、终结符、非终结符、byLhs 索引。
 * 2. 迭代计算 FIRST 集。
 * 3. 迭代计算 FOLLOW 集。
 * 4. 从 I0 = closure(S' -> · Program) 开始，构造 LR(0) 项目集族。
 * 5. 根据项目集和 FOLLOW 集填写 ACTION/GOTO 表。
 */
GrammarData buildGrammar() {
    GrammarData grammar;
    grammar.productions = makeProductions();

    // 第一遍：所有产生式左部都是非终结符，同时建立 byLhs。
    // 这一步只看 lhs，不看 rhs，是为了先知道哪些名字属于非终结符。
    // 后面区分终结符/非终结符时要依赖这个集合。
    for (std::size_t i = 0; i < grammar.productions.size(); ++i) {
        const Production& production = grammar.productions[i];
        grammar.nonterminals.insert(production.lhs);
        grammar.byLhs[production.lhs].push_back(static_cast<int>(i));
    }

    // 第二遍：右部中不属于非终结符的符号就是终结符。
    // 例如 "Statement" 是非终结符，"if"、"ident"、":=" 是终结符。
    for (const Production& production : grammar.productions) {
        for (const std::string& symbol : production.rhs) {
            if (grammar.nonterminals.count(symbol) == 0) {
                grammar.terminals.insert(symbol);
            }
        }
    }
    grammar.terminals.insert(kEnd);

    // 终结符的 FIRST 集就是它自己。
    // FIRST("+") = {"+"}，FIRST("ident") = {"ident"}。
    for (const std::string& terminal : grammar.terminals) {
        grammar.first[terminal].insert(terminal);
    }

    // 非终结符的 FIRST/FOLLOW 集先建空集合。
    // 后面的迭代会逐步把终结符或 epsilon 填进去。
    for (const std::string& nonterminal : grammar.nonterminals) {
        grammar.first[nonterminal];
        grammar.follow[nonterminal];
    }

    // 迭代计算 FIRST：只要某轮有新元素加入，就继续。
    //
    // 对每条产生式 A -> β：
    //   FIRST(A) += FIRST(β)
    //
    // 因为 FIRST(β) 可能依赖其他非终结符的 FIRST，
    // 所以必须反复扫描，直到所有集合都不再变化。
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Production& production : grammar.productions) {
            // firstOfSequence 能同时处理普通右部和空右部。
            // 空右部会得到 {epsilon}，所以空产生式会让 FIRST(lhs) 包含 epsilon。
            std::set<std::string> first = firstOfSequence(production.rhs, 0, grammar);
            for (const std::string& symbol : first) {
                if (grammar.first[production.lhs].insert(symbol).second) {
                    changed = true;
                }
            }
        }
    }

    // 开始符号的 FOLLOW 集包含输入结束符 $。
    // 含义是：完整 Program 后面应该遇到 EOF。
    grammar.follow[kStart].insert(kEnd);

    // 迭代计算 FOLLOW。
    //
    // 对每条产生式 A -> α B β：
    //   1. FIRST(β) - {epsilon} 加入 FOLLOW(B)。
    //   2. 如果 β 可以为空，FOLLOW(A) 也加入 FOLLOW(B)。
    //
    // 这些规则也会互相依赖，所以同样用 changed 循环做到不动点。
    changed = true;
    while (changed) {
        changed = false;
        for (const Production& production : grammar.productions) {
            for (std::size_t i = 0; i < production.rhs.size(); ++i) {
                const std::string& symbol = production.rhs[i];

                // FOLLOW 只对非终结符有意义；终结符不需要 FOLLOW 集。
                if (grammar.nonterminals.count(symbol) == 0) {
                    continue;
                }

                // 对 A -> α B β，把 FIRST(β) 中的非 epsilon 符号加入 FOLLOW(B)。
                // i + 1 是 β 的起点；如果 B 已经是最后一个符号，β 为空。
                std::set<std::string> betaFirst = firstOfSequence(production.rhs, i + 1, grammar);
                for (const std::string& terminal : betaFirst) {
                    if (terminal != kEpsilon && grammar.follow[symbol].insert(terminal).second) {
                        changed = true;
                    }
                }

                // 如果 β 可空，或 B 已经在产生式末尾，把 FOLLOW(A) 加入 FOLLOW(B)。
                // 例如 A -> α B，此时 B 后面没有符号，B 后面能跟什么取决于 A 后面能跟什么。
                if (betaFirst.count(kEpsilon) != 0 || i + 1 == production.rhs.size()) {
                    for (const std::string& terminal : grammar.follow[production.lhs]) {
                        if (grammar.follow[symbol].insert(terminal).second) {
                            changed = true;
                        }
                    }
                }
            }
        }
    }

    // 初始状态 I0 = closure(S' -> · Program)。
    // Item{0, 0} 表示第 0 条产生式 S' -> · Program。
    // 对它求闭包后，会把 Program、Block、声明和语句入口都展开到 I0 中。
    grammar.states.push_back(closure({Item{0, 0}}, grammar));
    std::queue<int> pending;
    pending.push(0);

    // 所有可能触发 GOTO 的符号：终结符 + 非终结符。
    // 终结符边后面会成为 ACTION shift，非终结符边后面会成为 GOTO 表项。
    std::vector<std::string> symbols;
    symbols.insert(symbols.end(), grammar.terminals.begin(), grammar.terminals.end());
    symbols.insert(symbols.end(), grammar.nonterminals.begin(), grammar.nonterminals.end());

    // 广度优先构造 LR(0) 项目集族。
    // pending 里放的是已经发现、但还没有扩展所有出边的状态编号。
    while (!pending.empty()) {
        int state = pending.front();
        pending.pop();

        for (const std::string& symbol : symbols) {
            // $ 是输入结束符，不应该作为普通语法符号读入产生自动机边。
            if (symbol == kEnd) {
                continue;
            }
            std::set<Item> next = gotoItems(grammar.states[state], symbol, grammar);
            if (next.empty()) {
                continue;
            }

            // 如果 next 是新项目集，加入 states；否则复用已有状态编号。
            // std::set<Item> 支持相等比较，所以可以直接 find 整个项目集。
            auto found = std::find(grammar.states.begin(), grammar.states.end(), next);
            int nextState = 0;
            if (found == grammar.states.end()) {
                nextState = static_cast<int>(grammar.states.size());
                grammar.states.push_back(next);
                // 新状态以后也要继续扩展它的出边。
                pending.push(nextState);
            } else {
                nextState = static_cast<int>(std::distance(grammar.states.begin(), found));
            }

            // 记录 LR(0) 自动机边：GOTO(I_state, symbol) = I_nextState。
            // 这里的 transitions 还不是最终 GOTO 表，它同时包含终结符和非终结符边。
            grammar.transitions[{state, symbol}] = nextState;
        }
    }

    // 根据项目集自动机填写 ACTION 和 GOTO 表。
    for (std::size_t state = 0; state < grammar.states.size(); ++state) {
        for (const Item& item : grammar.states[state]) {
            const Production& production = grammar.productions[item.production];
            if (item.dot < static_cast<int>(production.rhs.size())) {
                const std::string& symbol = production.rhs[item.dot];

                // 点后还有符号时，看自动机是否存在读入该符号的边。
                const auto transition = grammar.transitions.find({static_cast<int>(state), symbol});
                if (transition == grammar.transitions.end()) {
                    continue;
                }
                if (grammar.terminals.count(symbol) != 0) {
                    // 点后是终结符：ACTION[state, symbol] = shift nextState。
                    addAction(grammar, static_cast<int>(state), symbol, Action{Action::Kind::Shift, transition->second});
                } else {
                    // 点后是非终结符：GOTO[state, symbol] = nextState。
                    grammar.gotoTable[{static_cast<int>(state), symbol}] = transition->second;
                }
                continue;
            }

            // 点已经在产生式末尾，说明可以归约。
            if (production.lhs == kAugmentedStart) {
                // S' -> Program · 且向前看是 $，接受。
                addAction(grammar, static_cast<int>(state), kEnd, Action{Action::Kind::Accept, 0});
                continue;
            }

            // SLR 的核心：按 FOLLOW(lhs) 填归约动作。
            // 如果状态里有 A -> α ·，并且当前输入符号 a 属于 FOLLOW(A)，
            // 就可以用 A -> α 归约，也就是 ACTION[state, a] = reduce A -> α。
            for (const std::string& terminal : grammar.follow[production.lhs]) {
                addAction(grammar, static_cast<int>(state), terminal, Action{Action::Kind::Reduce, item.production});
            }
        }
    }

    return grammar;
}

/*
 * cachedGrammar 缓存构造好的文法数据。
 *
 * buildGrammar 比较重，不需要每次 parse 都重新构造。
 * static 局部变量第一次调用时初始化，后续直接复用。
 */
GrammarData& cachedGrammar() {
    static GrammarData grammar = buildGrammar();
    return grammar;
}

}  // namespace

// 构造 SLRParser，保存 Token 序列。
SLRParser::SLRParser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)) {}

/*
 * parse 执行 SLR 分析。
 *
 * SLR 分析运行时只依赖：
 * - 输入符号串 input。
 * - 状态栈 stateStack。
 * - 符号栈 symbolStack。
 * - ACTION/GOTO 表。
 *
 * 每一步：
 * 1. state = 状态栈顶。
 * 2. lookahead = 当前输入符号。
 * 3. 查 ACTION[state, lookahead]。
 * 4. Shift：输入符号入符号栈，目标状态入状态栈，输入指针右移。
 * 5. Reduce：按产生式右部长度弹栈，再按 GOTO 转入左部状态。
 * 6. Accept：成功。
 */
bool SLRParser::parse() {
    GrammarData& grammar = cachedGrammar();

    // 每次 parse 都重新清空结果，避免同一个 SLRParser 对象重复调用时残留旧错误。
    syntaxErrors_.clear();
    trace_.clear();

    std::vector<std::string> input;
    std::vector<int> lines;
    input.reserve(tokens_.size());
    lines.reserve(tokens_.size());

    // 把 Token 序列转换成 SLR 终结符序列，同时保留行号用于报错。
    for (const Token& token : tokens_) {
        // SLR 表只能处理语法类别，不能修复词法错误。
        // 词法阶段发现 invalid 后，直接把错误停在这里。
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            syntaxErrors_.push_back(SyntaxError{token.line, "词法错误阻止 LR 分析"});
            return false;
        }

        // 例如用户源码里的 x、y、sum 都统一变成 ident。
        // 这就是为什么 LR 表规模不会随着标识符名字数量变化。
        input.push_back(tokenToTerminal(token));
        lines.push_back(token.line);
    }

    // 保证输入以 $ 结束。
    if (input.empty() || input.back() != kEnd) {
        input.push_back(kEnd);
        lines.push_back(tokens_.empty() ? 1 : tokens_.back().line);
    }

    // 初始状态栈只有状态 0；符号栈用 $ 作为栈底符号。
    std::vector<int> stateStack{0};
    std::vector<std::string> symbolStack{kEnd};

    // pos 指向当前还没有处理的输入符号；shift 会让 pos 右移，reduce 不移动 pos。
    std::size_t pos = 0;
    int step = 0;

    // 设置步数上限，避免错误表或异常输入导致无限循环。
    while (step < 10000) {
        // LR 分析只用“状态栈顶 + 当前输入符号”决定下一步动作。
        const int state = stateStack.back();
        const std::string& lookahead = input[pos];
        const auto actionFound = grammar.actionTable.find({state, lookahead});
        if (actionFound == grammar.actionTable.end()) {
            // 找不到动作表示当前前缀无法继续构成合法 PL/0 程序。
            // 如果错误 token 是新行第一个 token，缺失符号通常在上一行末尾。
            int line = lines[pos];
            if (pos > 0 && lines[pos - 1] < line) {
                line = lines[pos - 1];
            }
            syntaxErrors_.push_back(SyntaxError{line, "LR 分析表无动作"});
            trace_.push_back(SLRTraceRow{
                step,
                joinStates(stateStack),
                joinSymbols(symbolStack),
                remainingInput(input, pos),
                "错误"
            });
            return false;
        }

        const Action action = actionFound->second;

        // 先记录本步分析过程，再真正执行动作。
        // 这样 trace 看到的是“执行动作之前”的栈和剩余输入，和教材里的 LR 表一致。
        trace_.push_back(SLRTraceRow{
            step,
            joinStates(stateStack),
            joinSymbols(symbolStack),
            remainingInput(input, pos),
            actionToString(action, grammar.productions)
        });

        if (action.kind == Action::Kind::Accept) {
            // 接受动作只会出现在增广项目 S' -> Program · 且 lookahead 为 $ 的情况。
            return true;
        }

        if (action.kind == Action::Kind::Shift) {
            // 移进：当前输入终结符入符号栈，ACTION 指定的状态入状态栈。
            //
            // 例如 ACTION[0, "const"] = s4：
            //   symbolStack.push("const")
            //   stateStack.push(4)
            //   pos 指向下一个输入符号
            symbolStack.push_back(lookahead);
            stateStack.push_back(action.value);
            ++pos;
            ++step;
            continue;
        }

        // 归约：action.value 是产生式编号。
        const Production& production = grammar.productions[action.value];

        // 右部有几个符号，就从状态栈和符号栈各弹出几个。
        // 空产生式 rhs.size() == 0，不弹栈。
        //
        // 例如用 Expression -> SignOpt Term ExprTail 归约时弹出 3 个语法符号；
        // 如果是 Statement -> epsilon，则什么都不弹，直接根据当前状态做 GOTO。
        for (std::size_t i = 0; i < production.rhs.size(); ++i) {
            if (!symbolStack.empty()) {
                symbolStack.pop_back();
            }
            if (!stateStack.empty()) {
                stateStack.pop_back();
            }
        }

        // 弹栈后，状态栈顶表示“归约前所在的状态”。
        // 用 GOTO[gotoFrom, lhs] 找到归约后应该进入的状态。
        //
        // 可以把这一步理解成：刚刚识别出了一个 lhs 非终结符，
        // 于是自动机从 gotoFrom 状态沿 lhs 这条边前进。
        const int gotoFrom = stateStack.back();
        const auto gotoFound = grammar.gotoTable.find({gotoFrom, production.lhs});
        if (gotoFound == grammar.gotoTable.end()) {
            syntaxErrors_.push_back(SyntaxError{lines[pos], "LR GOTO 表无动作"});
            return false;
        }

        // 把产生式左部非终结符压入符号栈，把 GOTO 状态压入状态栈。
        // reduce 不消费输入，所以 pos 不变；下一轮仍然用同一个 lookahead 查表。
        symbolStack.push_back(production.lhs);
        stateStack.push_back(gotoFound->second);
        ++step;
    }

    syntaxErrors_.push_back(SyntaxError{lines[pos], "LR 分析步骤超过上限"});
    return false;
}

// 打印 SLR trace 表格。
void SLRParser::printTrace(std::ostream& output) const {
    output << "SLR分析过程:\n";
    output << "步骤\t状态栈\t符号栈\t输入\t动作\n";
    for (const SLRTraceRow& row : trace_) {
        // 每一行对应 parse() 主循环的一次查表结果。
        // 状态栈/符号栈是动作执行前的状态，动作列说明接下来要 shift/reduce/accept。
        output << row.step << '\t'
               << row.stateStack << '\t'
               << row.symbolStack << '\t'
               << row.input << '\t'
               << row.action << '\n';
    }
}

// 返回 SLR 分析错误。
const std::vector<SyntaxError>& SLRParser::syntaxErrors() const {
    return syntaxErrors_;
}

/*
 * writeDot 导出 LR(0) 项目集自动机。
 *
 * 输出是 Graphviz DOT 格式，可以用 graphviz 转成 png：
 *   dot -Tpng slr_states.dot -o slr_states.png
 *
 * 每个节点是一个项目集 Ii；
 * 每条边表示 GOTO(Ii, symbol) = Ij。
 */
void SLRParser::writeDot(std::ostream& output) {
    GrammarData& grammar = cachedGrammar();

    // rankdir=LR 让状态图从左到右排布，更接近自动机图的阅读习惯。
    // fontname 用等宽字体，项目里的点号和符号对齐效果会更稳定。
    output << "digraph SLRStates {\n";
    output << "  rankdir=LR;\n";
    output << "  node [shape=box, fontname=\"Menlo\", fontsize=10];\n";
    output << "  edge [fontname=\"Menlo\", fontsize=10];\n\n";

    for (std::size_t i = 0; i < grammar.states.size(); ++i) {
        std::ostringstream label;
        label << "I" << i;
        for (const Item& item : grammar.states[i]) {
            // 节点标签第一行是状态编号，后面每行是一条 LR(0) 项目。
            label << "\n" << itemToString(item, grammar.productions);
        }
        output << "  I" << i << " [label=\"" << dotEscape(label.str()) << "\"];\n";
    }

    output << "\n";
    for (const auto& transition : grammar.transitions) {
        int from = transition.first.first;
        const std::string& symbol = transition.first.second;
        int to = transition.second;

        // 每条边就是项目集构造时记录的 GOTO(I_from, symbol) = I_to。
        // 终结符边对应 shift，非终结符边对应真正的 GOTO 表项。
        output << "  I" << from << " -> I" << to
               << " [label=\"" << dotEscape(symbol) << "\"];\n";
    }

    output << "}\n";
}
