#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "../common/Token.h"
#include "Parser.h"

struct SLRTraceRow {
    int step;
    std::string stateStack;
    std::string symbolStack;
    std::string input;
    std::string action;
};

class SLRParser {
public:
    explicit SLRParser(std::vector<Token> tokens);

    bool parse();
    void printTrace(std::ostream& output) const;
    const std::vector<SyntaxError>& syntaxErrors() const;

    static void writeDot(std::ostream& output);

private:
    struct Impl;
    std::vector<Token> tokens_;
    std::vector<SyntaxError> syntaxErrors_;
    std::vector<SLRTraceRow> trace_;
};
