#include "Semantic.h"

#include <utility>

SemanticAnalyzer::SemanticAnalyzer() {
    enterScope();
}

void SemanticAnalyzer::enterScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::exitScope() {
    if (scopes_.size() > 1) {
        scopes_.pop_back();
    }
}

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

const Symbol* SemanticAnalyzer::lookup(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto it = scope->find(name);
        if (it != scope->end()) {
            return &it->second;
        }
    }
    return nullptr;
}

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

void SemanticAnalyzer::requireDeclared(const Token& token) {
    if (lookup(token.lexeme) == nullptr) {
        addError(token.line, "未声明标识符: " + token.lexeme);
    }
}

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

void SemanticAnalyzer::requireReadable(const Token& token) {
    requireAssignable(token);
}

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

int SemanticAnalyzer::emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result) {
    quads_.push_back(Quad{op, arg1, arg2, result});
    return static_cast<int>(quads_.size());
}

void SemanticAnalyzer::patchResult(int quadIndex, const std::string& result) {
    if (quadIndex >= 1 && quadIndex <= static_cast<int>(quads_.size())) {
        quads_[quadIndex - 1].result = result;
    }
}

int SemanticAnalyzer::nextQuadIndex() const {
    return static_cast<int>(quads_.size()) + 1;
}

std::string SemanticAnalyzer::newTemp() {
    ++tempCounter_;
    return "T" + std::to_string(tempCounter_);
}

void SemanticAnalyzer::addError(int line, const std::string& message) {
    errors_.push_back(SemanticError{line, message});
}

const std::vector<SemanticError>& SemanticAnalyzer::errors() const {
    return errors_;
}

const std::vector<Quad>& SemanticAnalyzer::quads() const {
    return quads_;
}

const std::vector<Symbol>& SemanticAnalyzer::symbolsInOrder() const {
    return symbolsInOrder_;
}

std::string symbolKindName(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Const: return "const";
        case SymbolKind::Var: return "var";
        case SymbolKind::Procedure: return "procedure";
    }
    return "unknown";
}
