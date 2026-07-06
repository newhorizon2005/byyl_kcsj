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

constexpr const char* kAugmentedStart = "S'";
constexpr const char* kStart = "Program";
constexpr const char* kEnd = "$";
constexpr const char* kEpsilon = "epsilon";

struct Production {
    std::string lhs;
    std::vector<std::string> rhs;
};

struct Item {
    int production = 0;
    int dot = 0;

    bool operator==(const Item& other) const {
        return production == other.production && dot == other.dot;
    }

    bool operator<(const Item& other) const {
        if (production != other.production) {
            return production < other.production;
        }
        return dot < other.dot;
    }
};

struct Action {
    enum class Kind {
        Shift,
        Reduce,
        Accept
    };

    Kind kind;
    int value = 0;
};

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

std::set<Item> closure(const std::set<Item>& items, const GrammarData& grammar) {
    std::set<Item> result = items;
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<Item> snapshot(result.begin(), result.end());
        for (const Item& item : snapshot) {
            const Production& production = grammar.productions[item.production];
            if (item.dot >= static_cast<int>(production.rhs.size())) {
                continue;
            }
            const std::string& symbol = production.rhs[item.dot];
            if (grammar.nonterminals.count(symbol) == 0) {
                continue;
            }
            const auto found = grammar.byLhs.find(symbol);
            if (found == grammar.byLhs.end()) {
                continue;
            }
            for (int productionIndex : found->second) {
                if (result.insert(Item{productionIndex, 0}).second) {
                    changed = true;
                }
            }
        }
    }
    return result;
}

std::set<Item> gotoItems(const std::set<Item>& items, const std::string& symbol, const GrammarData& grammar) {
    std::set<Item> moved;
    for (const Item& item : items) {
        const Production& production = grammar.productions[item.production];
        if (item.dot < static_cast<int>(production.rhs.size()) && production.rhs[item.dot] == symbol) {
            moved.insert(Item{item.production, item.dot + 1});
        }
    }
    if (moved.empty()) {
        return moved;
    }
    return closure(moved, grammar);
}

std::set<std::string> firstOfSequence(const std::vector<std::string>& sequence, std::size_t start, const GrammarData& grammar) {
    std::set<std::string> result;
    bool allNullable = true;
    for (std::size_t i = start; i < sequence.size(); ++i) {
        const std::string& symbol = sequence[i];
        const auto found = grammar.first.find(symbol);
        if (found == grammar.first.end()) {
            result.insert(symbol);
            allNullable = false;
            break;
        }

        for (const std::string& terminal : found->second) {
            if (terminal != kEpsilon) {
                result.insert(terminal);
            }
        }
        if (found->second.count(kEpsilon) == 0) {
            allNullable = false;
            break;
        }
    }
    if (allNullable) {
        result.insert(kEpsilon);
    }
    return result;
}

void addAction(GrammarData& grammar, int state, const std::string& terminal, const Action& action) {
    const auto key = std::make_pair(state, terminal);
    const auto existing = grammar.actionTable.find(key);
    if (existing == grammar.actionTable.end()) {
        grammar.actionTable[key] = action;
        return;
    }

    if (existing->second.kind == action.kind && existing->second.value == action.value) {
        return;
    }

    // The PL/0 grammar contains optional empty statements. Prefer shift to avoid
    // reducing an empty statement before a real statement starter.
    if (existing->second.kind == Action::Kind::Reduce && action.kind == Action::Kind::Shift) {
        grammar.actionTable[key] = action;
    }
}

GrammarData buildGrammar() {
    GrammarData grammar;
    grammar.productions = makeProductions();

    for (std::size_t i = 0; i < grammar.productions.size(); ++i) {
        const Production& production = grammar.productions[i];
        grammar.nonterminals.insert(production.lhs);
        grammar.byLhs[production.lhs].push_back(static_cast<int>(i));
    }

    for (const Production& production : grammar.productions) {
        for (const std::string& symbol : production.rhs) {
            if (grammar.nonterminals.count(symbol) == 0) {
                grammar.terminals.insert(symbol);
            }
        }
    }
    grammar.terminals.insert(kEnd);

    for (const std::string& terminal : grammar.terminals) {
        grammar.first[terminal].insert(terminal);
    }
    for (const std::string& nonterminal : grammar.nonterminals) {
        grammar.first[nonterminal];
        grammar.follow[nonterminal];
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const Production& production : grammar.productions) {
            std::set<std::string> first = firstOfSequence(production.rhs, 0, grammar);
            for (const std::string& symbol : first) {
                if (grammar.first[production.lhs].insert(symbol).second) {
                    changed = true;
                }
            }
        }
    }

    grammar.follow[kStart].insert(kEnd);
    changed = true;
    while (changed) {
        changed = false;
        for (const Production& production : grammar.productions) {
            for (std::size_t i = 0; i < production.rhs.size(); ++i) {
                const std::string& symbol = production.rhs[i];
                if (grammar.nonterminals.count(symbol) == 0) {
                    continue;
                }

                std::set<std::string> betaFirst = firstOfSequence(production.rhs, i + 1, grammar);
                for (const std::string& terminal : betaFirst) {
                    if (terminal != kEpsilon && grammar.follow[symbol].insert(terminal).second) {
                        changed = true;
                    }
                }

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

    grammar.states.push_back(closure({Item{0, 0}}, grammar));
    std::queue<int> pending;
    pending.push(0);

    std::vector<std::string> symbols;
    symbols.insert(symbols.end(), grammar.terminals.begin(), grammar.terminals.end());
    symbols.insert(symbols.end(), grammar.nonterminals.begin(), grammar.nonterminals.end());

    while (!pending.empty()) {
        int state = pending.front();
        pending.pop();

        for (const std::string& symbol : symbols) {
            if (symbol == kEnd) {
                continue;
            }
            std::set<Item> next = gotoItems(grammar.states[state], symbol, grammar);
            if (next.empty()) {
                continue;
            }

            auto found = std::find(grammar.states.begin(), grammar.states.end(), next);
            int nextState = 0;
            if (found == grammar.states.end()) {
                nextState = static_cast<int>(grammar.states.size());
                grammar.states.push_back(next);
                pending.push(nextState);
            } else {
                nextState = static_cast<int>(std::distance(grammar.states.begin(), found));
            }
            grammar.transitions[{state, symbol}] = nextState;
        }
    }

    for (std::size_t state = 0; state < grammar.states.size(); ++state) {
        for (const Item& item : grammar.states[state]) {
            const Production& production = grammar.productions[item.production];
            if (item.dot < static_cast<int>(production.rhs.size())) {
                const std::string& symbol = production.rhs[item.dot];
                const auto transition = grammar.transitions.find({static_cast<int>(state), symbol});
                if (transition == grammar.transitions.end()) {
                    continue;
                }
                if (grammar.terminals.count(symbol) != 0) {
                    addAction(grammar, static_cast<int>(state), symbol, Action{Action::Kind::Shift, transition->second});
                } else {
                    grammar.gotoTable[{static_cast<int>(state), symbol}] = transition->second;
                }
                continue;
            }

            if (production.lhs == kAugmentedStart) {
                addAction(grammar, static_cast<int>(state), kEnd, Action{Action::Kind::Accept, 0});
                continue;
            }

            for (const std::string& terminal : grammar.follow[production.lhs]) {
                addAction(grammar, static_cast<int>(state), terminal, Action{Action::Kind::Reduce, item.production});
            }
        }
    }

    return grammar;
}

GrammarData& cachedGrammar() {
    static GrammarData grammar = buildGrammar();
    return grammar;
}

}  // namespace

SLRParser::SLRParser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)) {}

bool SLRParser::parse() {
    GrammarData& grammar = cachedGrammar();
    syntaxErrors_.clear();
    trace_.clear();

    std::vector<std::string> input;
    std::vector<int> lines;
    input.reserve(tokens_.size());
    lines.reserve(tokens_.size());
    for (const Token& token : tokens_) {
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            syntaxErrors_.push_back(SyntaxError{token.line, "词法错误阻止 LR 分析"});
            return false;
        }
        input.push_back(tokenToTerminal(token));
        lines.push_back(token.line);
    }
    if (input.empty() || input.back() != kEnd) {
        input.push_back(kEnd);
        lines.push_back(tokens_.empty() ? 1 : tokens_.back().line);
    }

    std::vector<int> stateStack{0};
    std::vector<std::string> symbolStack{kEnd};
    std::size_t pos = 0;
    int step = 0;

    while (step < 10000) {
        const int state = stateStack.back();
        const std::string& lookahead = input[pos];
        const auto actionFound = grammar.actionTable.find({state, lookahead});
        if (actionFound == grammar.actionTable.end()) {
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
        trace_.push_back(SLRTraceRow{
            step,
            joinStates(stateStack),
            joinSymbols(symbolStack),
            remainingInput(input, pos),
            actionToString(action, grammar.productions)
        });

        if (action.kind == Action::Kind::Accept) {
            return true;
        }

        if (action.kind == Action::Kind::Shift) {
            symbolStack.push_back(lookahead);
            stateStack.push_back(action.value);
            ++pos;
            ++step;
            continue;
        }

        const Production& production = grammar.productions[action.value];
        for (std::size_t i = 0; i < production.rhs.size(); ++i) {
            if (!symbolStack.empty()) {
                symbolStack.pop_back();
            }
            if (!stateStack.empty()) {
                stateStack.pop_back();
            }
        }

        const int gotoFrom = stateStack.back();
        const auto gotoFound = grammar.gotoTable.find({gotoFrom, production.lhs});
        if (gotoFound == grammar.gotoTable.end()) {
            syntaxErrors_.push_back(SyntaxError{lines[pos], "LR GOTO 表无动作"});
            return false;
        }

        symbolStack.push_back(production.lhs);
        stateStack.push_back(gotoFound->second);
        ++step;
    }

    syntaxErrors_.push_back(SyntaxError{lines[pos], "LR 分析步骤超过上限"});
    return false;
}

void SLRParser::printTrace(std::ostream& output) const {
    output << "SLR分析过程:\n";
    output << "步骤\t状态栈\t符号栈\t输入\t动作\n";
    for (const SLRTraceRow& row : trace_) {
        output << row.step << '\t'
               << row.stateStack << '\t'
               << row.symbolStack << '\t'
               << row.input << '\t'
               << row.action << '\n';
    }
}

const std::vector<SyntaxError>& SLRParser::syntaxErrors() const {
    return syntaxErrors_;
}

void SLRParser::writeDot(std::ostream& output) {
    GrammarData& grammar = cachedGrammar();

    output << "digraph SLRStates {\n";
    output << "  rankdir=LR;\n";
    output << "  node [shape=box, fontname=\"Menlo\", fontsize=10];\n";
    output << "  edge [fontname=\"Menlo\", fontsize=10];\n\n";

    for (std::size_t i = 0; i < grammar.states.size(); ++i) {
        std::ostringstream label;
        label << "I" << i;
        for (const Item& item : grammar.states[i]) {
            label << "\n" << itemToString(item, grammar.productions);
        }
        output << "  I" << i << " [label=\"" << dotEscape(label.str()) << "\"];\n";
    }

    output << "\n";
    for (const auto& transition : grammar.transitions) {
        int from = transition.first.first;
        const std::string& symbol = transition.first.second;
        int to = transition.second;
        output << "  I" << from << " -> I" << to
               << " [label=\"" << dotEscape(symbol) << "\"];\n";
    }

    output << "}\n";
}
