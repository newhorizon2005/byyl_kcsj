#include "Parser.h"

#include <utility>

Parser::Parser(std::vector<Token> tokens, SemanticAnalyzer* semantic)
    : tokens_(std::move(tokens)), semantic_(semantic) {}

bool Parser::parseProgram() {
    if (semantic_ != nullptr) {
        semantic_->emit("syss", "_", "_", "_");
    }
    parseBlock();
    expect(TokenType::Period, "程序应以 . 结束");
    if (!isAtEnd()) {
        addSyntaxError(current().line, "程序结束符 . 后存在多余内容");
    }
    if (semantic_ != nullptr) {
        semantic_->emit("syse", "_", "_", "_");
    }
    return syntaxErrors_.empty();
}

const std::vector<SyntaxError>& Parser::syntaxErrors() const {
    return syntaxErrors_;
}

const Token& Parser::current() const {
    return tokens_[pos_];
}

const Token& Parser::previous() const {
    if (pos_ == 0) {
        return tokens_[0];
    }
    return tokens_[pos_ - 1];
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::EndOfFile;
}

bool Parser::check(TokenType type) const {
    return !isAtEnd() && current().type == type;
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

Token Parser::advance() {
    if (!isAtEnd()) {
        ++pos_;
    }
    return previous();
}

Token Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    int line = current().line;
    if (pos_ > 0 && previous().line < current().line) {
        switch (type) {
            case TokenType::Semicolon:
            case TokenType::Do:
            case TokenType::Then:
            case TokenType::RParen:
            case TokenType::Period:
                line = previous().line;
                break;
            default:
                break;
        }
    }
    addSyntaxError(line, message);
    return Token{type, "", line, current().column, ""};
}

void Parser::addSyntaxError(int line, const std::string& message) {
    syntaxErrors_.push_back(SyntaxError{line, message});
    syntaxErrorLines_.insert(line);
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (previous().type == TokenType::Semicolon) {
            return;
        }
        switch (current().type) {
            case TokenType::Const:
            case TokenType::Var:
            case TokenType::Procedure:
            case TokenType::Call:
            case TokenType::Begin:
            case TokenType::If:
            case TokenType::While:
            case TokenType::Read:
            case TokenType::Write:
            case TokenType::End:
            case TokenType::Period:
                return;
            default:
                advance();
                break;
        }
    }
}

void Parser::parseBlock() {
    if (match(TokenType::Const)) {
        parseConstDecl();
    }
    if (match(TokenType::Var)) {
        parseVarDecl();
    }
    while (check(TokenType::Procedure)) {
        parseProcedureDecl();
    }
    parseStatement();
}

void Parser::parseConstDecl() {
    do {
        Token name = expect(TokenType::Identifier, "const 后应为标识符");
        expect(TokenType::Equal, "常量定义应使用 =");
        Token value = expect(TokenType::Number, "常量定义应为无符号整数");
        if (semantic_ != nullptr && !name.lexeme.empty()) {
            if (semantic_->declare(name, SymbolKind::Const, value.lexeme)) {
                semantic_->emit("const", name.lexeme, "_", "_");
                semantic_->emit("=", value.lexeme, "_", name.lexeme);
            }
        }
    } while (match(TokenType::Comma));
    expect(TokenType::Semicolon, "常量说明部分应以 ; 结束");
}

void Parser::parseVarDecl() {
    do {
        Token name = expect(TokenType::Identifier, "var 后应为标识符");
        if (semantic_ != nullptr && !name.lexeme.empty()) {
            if (semantic_->declare(name, SymbolKind::Var, "0")) {
                semantic_->emit("var", name.lexeme, "_", "_");
            }
        }
    } while (match(TokenType::Comma));
    expect(TokenType::Semicolon, "变量说明部分应以 ; 结束");
}

void Parser::parseProcedureDecl() {
    expect(TokenType::Procedure, "过程说明应以 procedure 开始");
    Token name = expect(TokenType::Identifier, "procedure 后应为标识符");
    if (semantic_ != nullptr && !name.lexeme.empty()) {
        if (semantic_->declare(name, SymbolKind::Procedure)) {
            semantic_->emit("procedure", name.lexeme, "_", "_");
        }
        semantic_->enterScope();
    }
    expect(TokenType::Semicolon, "过程首部应以 ; 结束");
    parseBlock();
    if (semantic_ != nullptr) {
        semantic_->emit("ret", "_", "_", "_");
        semantic_->exitScope();
    }
    expect(TokenType::Semicolon, "过程说明部分应以 ; 结束");
}

void Parser::parseStatement() {
    if (match(TokenType::Identifier)) {
        Token name = previous();
        if (semantic_ != nullptr) {
            semantic_->requireAssignable(name);
        }
        expect(TokenType::Assign, "赋值语句应使用 :=");
        std::string value = parseExpression();
        if (semantic_ != nullptr) {
            semantic_->emit(":=", value, "_", name.lexeme);
        }
        return;
    }

    if (match(TokenType::Call)) {
        Token name = expect(TokenType::Identifier, "call 后应为过程标识符");
        if (semantic_ != nullptr && !name.lexeme.empty()) {
            semantic_->requireCallable(name);
            semantic_->emit("call", name.lexeme, "_", "_");
        }
        return;
    }

    if (match(TokenType::Begin)) {
        parseStatement();
        while (!isAtEnd() && !check(TokenType::End)) {
            if (match(TokenType::Semicolon)) {
                if (check(TokenType::End)) {
                    break;
                }
                parseStatement();
                continue;
            }
            if (isStatementStart(current().type)) {
                addSyntaxError(current().line, "语句之间缺少 ;");
                parseStatement();
                continue;
            }
            addSyntaxError(current().line, "复合语句中存在无法识别的成分");
            advance();
        }
        expect(TokenType::End, "begin 应以 end 匹配");
        return;
    }

    if (match(TokenType::If)) {
        ConditionValue condition = parseCondition();
        int falseJump = 0;
        if (semantic_ != nullptr) {
            falseJump = semantic_->emit("j" + inverseRelation(condition.op), condition.left, condition.right, "$?");
        }
        expect(TokenType::Then, "if 条件后缺少 then");
        parseStatement();
        if (semantic_ != nullptr && falseJump != 0) {
            semantic_->patchResult(falseJump, quadTarget(semantic_->nextQuadIndex()));
        }
        return;
    }

    if (match(TokenType::While)) {
        int conditionStart = semantic_ != nullptr ? semantic_->nextQuadIndex() : 0;
        ConditionValue condition = parseCondition();
        int falseJump = 0;
        if (semantic_ != nullptr) {
            falseJump = semantic_->emit("j" + inverseRelation(condition.op), condition.left, condition.right, "$?");
        }
        expect(TokenType::Do, "while 条件后缺少 do");
        parseStatement();
        if (semantic_ != nullptr) {
            semantic_->emit("j", "_", "_", quadTarget(conditionStart));
            semantic_->patchResult(falseJump, quadTarget(semantic_->nextQuadIndex()));
        }
        return;
    }

    if (match(TokenType::Read)) {
        expect(TokenType::LParen, "read 后缺少 (");
        Token name = expect(TokenType::Identifier, "read 参数应为标识符");
        if (semantic_ != nullptr && !name.lexeme.empty()) {
            semantic_->requireReadable(name);
            semantic_->emit("read", name.lexeme, "_", "_");
        }
        while (match(TokenType::Comma)) {
            name = expect(TokenType::Identifier, "read 参数应为标识符");
            if (semantic_ != nullptr && !name.lexeme.empty()) {
                semantic_->requireReadable(name);
                semantic_->emit("read", name.lexeme, "_", "_");
            }
        }
        expect(TokenType::RParen, "read 参数列表缺少 )");
        return;
    }

    if (match(TokenType::Write)) {
        expect(TokenType::LParen, "write 后缺少 (");
        std::string value = parseExpression();
        if (semantic_ != nullptr) {
            semantic_->emit("write", value, "_", "_");
        }
        while (match(TokenType::Comma)) {
            value = parseExpression();
            if (semantic_ != nullptr) {
                semantic_->emit("write", value, "_", "_");
            }
        }
        expect(TokenType::RParen, "write 参数列表缺少 )");
        return;
    }

    // Empty statement is valid in PL/0.
}

ConditionValue Parser::parseCondition() {
    if (match(TokenType::Odd)) {
        std::string value = parseExpression();
        return ConditionValue{"odd", value, "_"};
    }

    std::string left = parseExpression();
    if (!isRelation(current().type)) {
        addSyntaxError(current().line, "条件中缺少关系运算符");
        return ConditionValue{"#", "0", "0"};
    }
    std::string op = relationLexeme(current().type);
    advance();
    std::string right = parseExpression();
    return ConditionValue{op, left, right};
}

std::string Parser::parseExpression() {
    bool negative = false;
    if (match(TokenType::Plus)) {
        negative = false;
    } else if (match(TokenType::Minus)) {
        negative = true;
    }

    std::string left = parseTerm();
    if (negative && semantic_ != nullptr) {
        std::string temp = semantic_->newTemp();
        semantic_->emit("-", "0", left, temp);
        left = temp;
    }

    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        std::string op = current().lexeme;
        advance();
        std::string right = parseTerm();
        if (semantic_ != nullptr) {
            std::string temp = semantic_->newTemp();
            semantic_->emit(op, left, right, temp);
            left = temp;
        }
    }
    return left;
}

std::string Parser::parseTerm() {
    std::string left = parseFactor();
    while (check(TokenType::Times) || check(TokenType::Slash)) {
        std::string op = current().lexeme;
        advance();
        std::string right = parseFactor();
        if (semantic_ != nullptr) {
            std::string temp = semantic_->newTemp();
            semantic_->emit(op, left, right, temp);
            left = temp;
        }
    }
    return left;
}

std::string Parser::parseFactor() {
    if (match(TokenType::Identifier)) {
        Token name = previous();
        if (semantic_ != nullptr) {
            semantic_->requireValue(name);
        }
        return name.lexeme;
    }
    if (match(TokenType::Number)) {
        return previous().lexeme;
    }
    if (match(TokenType::LParen)) {
        std::string value = parseExpression();
        expect(TokenType::RParen, "表达式缺少右括号 )");
        return value;
    }

    addSyntaxError(current().line, "表达式因子错误");
    if (!isAtEnd()) {
        advance();
    }
    return "0";
}

std::string Parser::relationLexeme(TokenType type) const {
    switch (type) {
        case TokenType::Equal: return "=";
        case TokenType::NotEqual: return "#";
        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";
        default: return "#";
    }
}

std::string Parser::inverseRelation(const std::string& op) const {
    if (op == "=") return "#";
    if (op == "#") return "=";
    if (op == "<") return ">=";
    if (op == "<=") return ">";
    if (op == ">") return "<=";
    if (op == ">=") return "<";
    if (op == "odd") return "notodd";
    return "=";
}

bool Parser::isRelation(TokenType type) const {
    switch (type) {
        case TokenType::Equal:
        case TokenType::NotEqual:
        case TokenType::Less:
        case TokenType::LessEqual:
        case TokenType::Greater:
        case TokenType::GreaterEqual:
            return true;
        default:
            return false;
    }
}

std::string Parser::quadTarget(int index) const {
    return "$" + std::to_string(index);
}
