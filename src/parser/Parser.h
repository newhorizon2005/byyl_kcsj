#pragma once

#include <set>
#include <string>
#include <vector>

#include "../common/Token.h"
#include "../semantic/Semantic.h"

struct SyntaxError {
    int line;
    std::string message;
};

struct ConditionValue {
    std::string op;
    std::string left;
    std::string right;
};

class Parser {
public:
    Parser(std::vector<Token> tokens, SemanticAnalyzer* semantic = nullptr);

    bool parseProgram();
    const std::vector<SyntaxError>& syntaxErrors() const;

private:
    const Token& current() const;
    const Token& previous() const;
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token advance();
    Token expect(TokenType type, const std::string& message);
    void addSyntaxError(int line, const std::string& message);
    void synchronize();

    void parseBlock();
    void parseConstDecl();
    void parseVarDecl();
    void parseProcedureDecl();
    void parseStatement();
    ConditionValue parseCondition();
    std::string parseExpression();
    std::string parseTerm();
    std::string parseFactor();

    std::string relationLexeme(TokenType type) const;
    std::string inverseRelation(const std::string& op) const;
    bool isRelation(TokenType type) const;
    std::string quadTarget(int index) const;

    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    std::vector<SyntaxError> syntaxErrors_;
    std::set<int> syntaxErrorLines_;
    SemanticAnalyzer* semantic_;
};
