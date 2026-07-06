#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../common/Token.h"

enum class SymbolKind {
    Const,
    Var,
    Procedure
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    std::string value;
    int level;
};

struct Quad {
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
};

struct SemanticError {
    int line;
    std::string message;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    void enterScope();
    void exitScope();

    bool declare(const Token& token, SymbolKind kind, const std::string& value = "");
    const Symbol* lookup(const std::string& name) const;
    const Symbol* lookupCurrent(const std::string& name) const;

    void requireDeclared(const Token& token);
    void requireAssignable(const Token& token);
    void requireReadable(const Token& token);
    void requireCallable(const Token& token);
    void requireValue(const Token& token);

    int emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result);
    void patchResult(int quadIndex, const std::string& result);
    int nextQuadIndex() const;
    std::string newTemp();

    void addError(int line, const std::string& message);

    const std::vector<SemanticError>& errors() const;
    const std::vector<Quad>& quads() const;
    const std::vector<Symbol>& symbolsInOrder() const;

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
    std::vector<Symbol> symbolsInOrder_;
    std::vector<SemanticError> errors_;
    std::vector<Quad> quads_;
    int tempCounter_ = 0;
};

std::string symbolKindName(SymbolKind kind);
