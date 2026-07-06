#include "Parser.h"

#include <utility>

/*
 * 构造 Parser。
 *
 * tokens_(std::move(tokens))
 *   把 Lexer 输出的 Token 序列移动进 Parser，之后 Parser 用 pos_ 在数组上移动。
 *
 * semantic_(semantic)
 *   保存可选的语义分析器。
 *   - parse-ll 模式传 nullptr，只检查语法。
 *   - sem-ll/target/run 模式传 SemanticAnalyzer，边解析边做语义动作。
 */
Parser::Parser(std::vector<Token> tokens, SemanticAnalyzer* semantic)
    : tokens_(std::move(tokens)), semantic_(semantic) {}

/*
 * parseProgram 对应文法：
 *
 *   Program -> Block "."
 *
 * 执行流程：
 * 1. 如果启用了语义分析，先发一条 syss 四元式，表示程序开始。
 * 2. 调用 parseBlock() 分析主程序块。
 * 3. 要求程序以句点 "." 结束。
 * 4. 如果句点后还有额外 Token，记录错误。
 * 5. 如果启用了语义分析，发 syse 四元式，表示程序结束。
 * 6. 返回是否没有语法错误。
 */
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

// 返回语法错误数组。const 引用避免拷贝。
const std::vector<SyntaxError>& Parser::syntaxErrors() const {
    return syntaxErrors_;
}

// 当前 Token。pos_ 总是指向“下一个将被分析的 Token”。
const Token& Parser::current() const {
    return tokens_[pos_];
}

/*
 * 返回刚刚消费过的 Token。
 *
 * 如果 pos_ == 0，说明还没消费过任何 Token，为了避免下标越界，返回 tokens_[0]。
 */
const Token& Parser::previous() const {
    if (pos_ == 0) {
        return tokens_[0];
    }
    return tokens_[pos_ - 1];
}

// Parser 不直接看字符，而是看 Lexer 追加的 EndOfFile Token。
bool Parser::isAtEnd() const {
    return current().type == TokenType::EndOfFile;
}

// 判断当前 Token 类型是否匹配。EOF 时返回 false，避免继续匹配普通符号。
bool Parser::check(TokenType type) const {
    return !isAtEnd() && current().type == type;
}

/*
 * match 是递归下降里最常用的“可选匹配”工具。
 *
 * 例如：
 *   if (match(TokenType::Const)) parseConstDecl();
 *
 * 含义是：如果当前就是 const，就消费它并进入常量声明分析；
 * 如果不是，就什么都不做，让调用者选择其他分支。
 */
bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

/*
 * advance 消费一个 Token。
 *
 * 注意：
 * - 如果已经是 EOF，不再向后移动，避免越界。
 * - 返回 previous()，也就是刚消费掉的那个 Token。
 */
Token Parser::advance() {
    if (!isAtEnd()) {
        ++pos_;
    }
    return previous();
}

/*
 * expect 是“强制匹配”。
 *
 * 递归下降里有些符号是语法必须出现的，例如：
 *   const a = 1;
 *           ^
 *           这里必须是 =
 *
 * 执行流程：
 * 1. 如果当前 Token 就是期望类型，直接 advance() 返回它。
 * 2. 否则记录语法错误。
 * 3. 返回一个“占位 Token”，让后续分析尽量继续，而不是立刻崩掉。
 *
 * 中间的 line 调整：
 * 如果当前 Token 已经到了下一行，而缺的是 ;、then、do、) 或 .，
 * 错误更可能属于上一行，所以把报错行号修正到 previous().line。
 */
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

// 记录一条语法错误，同时把行号放进 set，便于后续扩展为“同一行去重”。
void Parser::addSyntaxError(int line, const std::string& message) {
    syntaxErrors_.push_back(SyntaxError{line, message});
    syntaxErrorLines_.insert(line);
}

/*
 * synchronize 是错误恢复函数。
 *
 * 思路：
 * 语法出错后，不要继续从错误 Token 开始硬分析，否则容易产生大量连锁错误。
 * 可以一路跳过，直到遇到比较像“新语法结构开头”的 Token。
 *
 * 当前 Parser 主要通过 expect() 的占位 Token 继续分析，synchronize() 保留为
 * 更强错误恢复的工具。
 */
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

/*
 * parseBlock 对应文法：
 *
 *   Block -> [ConstDecl] [VarDecl] { ProcedureDecl } Statement
 *
 * 方括号 [] 表示可选，花括号 {} 表示可重复。
 *
 * 执行流程：
 * 1. 如果看到 const，就分析常量声明。
 * 2. 如果看到 var，就分析变量声明。
 * 3. 只要接下来是 procedure，就不断分析过程声明。
 * 4. 最后分析一条语句。PL/0 的块一定以一个 Statement 结尾；
 *    如果什么语句都没有，parseStatement() 会接受空语句。
 */
void Parser::parseBlock() {
    // PL/0 的块结构顺序是固定的：
    // const 声明必须在 var 前，var 必须在 procedure 前，最后才是语句。
    // 所以这里不能把几个 if/while 随意调换。
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

/*
 * parseConstDecl 对应文法：
 *
 *   ConstDecl -> "const" ident "=" number { "," ident "=" number } ";"
 *
 * 注意：调用这个函数前，parseBlock() 已经 match 掉了 const，
 * 所以这里从 ident 开始读。
 *
 * do-while 的作用：
 * - 至少读取一个常量定义。
 * - 如果后面有逗号，就继续读取下一个。
 *
 * 语义动作：
 * - declare(... Const ...) 把常量写入符号表。
 * - emit("const", name, "_", "_") 声明常量。
 * - emit("=", value, "_", name) 把常量值赋给该名字。
 */
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

/*
 * parseVarDecl 对应文法：
 *
 *   VarDecl -> "var" ident { "," ident } ";"
 *
 * 和常量声明类似，调用前 var 已经被 parseBlock() 消费。
 *
 * 每读到一个变量名：
 * - 语义分析器检查是否重复声明。
 * - 成功后生成 var 四元式，表示运行时需要有这个变量。
 */
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

/*
 * parseProcedureDecl 对应文法：
 *
 *   ProcedureDecl -> "procedure" ident ";" Block ";"
 *
 * 这里和 parseConstDecl/parseVarDecl 不同：
 * parseBlock() 的 while 只是 check(TokenType::Procedure)，没有提前消费 procedure，
 * 所以本函数开头要 expect(TokenType::Procedure)。
 *
 * 语义动作：
 * 1. 声明过程名，生成 procedure 四元式。
 * 2. enterScope() 进入过程自己的作用域。
 * 3. 分析过程体 Block。
 * 4. emit("ret") 表示过程返回。
 * 5. exitScope() 离开过程作用域。
 */
void Parser::parseProcedureDecl() {
    expect(TokenType::Procedure, "过程说明应以 procedure 开始");
    Token name = expect(TokenType::Identifier, "procedure 后应为标识符");
    if (semantic_ != nullptr && !name.lexeme.empty()) {
        // 过程名属于当前外层作用域；过程体内部的局部变量要放进新的作用域。
        // 因此先 declare 过程名，再 enterScope() 分析过程体。
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

/*
 * parseStatement 对应文法：
 *
 *   Statement -> ident ":=" Expression
 *              | "call" ident
 *              | "begin" Statement { ";" Statement } "end"
 *              | "if" Condition "then" Statement
 *              | "while" Condition "do" Statement
 *              | "read" "(" ident { "," ident } ")"
 *              | "write" "(" Expression { "," Expression } ")"
 *              | epsilon
 *
 * 它的判断方式非常直观：
 * 看当前 Token 是什么，就进入相应分支。
 * 如果所有分支都不匹配，就把它当作空语句 epsilon。
 */
void Parser::parseStatement() {
    /*
     * 赋值语句：
     *
     *   ident ":=" Expression
     *
     * match(Identifier) 成功后，previous() 就是赋值目标。
     */
    if (match(TokenType::Identifier)) {
        Token name = previous();
        if (semantic_ != nullptr) {
            // 左边必须是变量，不能是常量或过程名。
            semantic_->requireAssignable(name);
        }
        expect(TokenType::Assign, "赋值语句应使用 :=");

        // parseExpression 返回表达式结果的位置，可能是变量名、数字或临时变量 Tn。
        // 这里先完整解析右值，再发赋值四元式，保证 a := b + c 会先生成加法临时结果。
        std::string value = parseExpression();
        if (semantic_ != nullptr) {
            // 四元式：(:=, value, _, name)
            semantic_->emit(":=", value, "_", name.lexeme);
        }
        return;
    }

    /*
     * 过程调用：
     *
     *   call ident
     */
    if (match(TokenType::Call)) {
        Token name = expect(TokenType::Identifier, "call 后应为过程标识符");
        if (semantic_ != nullptr && !name.lexeme.empty()) {
            semantic_->requireCallable(name);
            semantic_->emit("call", name.lexeme, "_", "_");
        }
        return;
    }

    /*
     * 复合语句：
     *
     *   begin Statement { ";" Statement } end
     *
     * begin/end 把多条语句组合成一条“大语句”。
     */
    if (match(TokenType::Begin)) {
        // begin 后先尝试解析第一条语句。即使是空语句也合法。
        // 例如 begin end 中，parseStatement() 会走到最后的 epsilon 分支。
        parseStatement();

        // 继续读直到 end。中间正常情况应该是 ; Statement。
        while (!isAtEnd() && !check(TokenType::End)) {
            if (match(TokenType::Semicolon)) {
                // 允许 end 前有一个多余分号，例如 begin a:=1; end。
                if (check(TokenType::End)) {
                    break;
                }
                // 分号后应该是一条新的 Statement。
                parseStatement();
                continue;
            }
            if (isStatementStart(current().type)) {
                // 当前像一条新语句的开头，但前面没有分号，报“缺少 ;”后继续解析。
                addSyntaxError(current().line, "语句之间缺少 ;");
                parseStatement();
                continue;
            }
            // 当前既不是分号，也不像语句开头，只能跳过这个无法识别的 Token。
            addSyntaxError(current().line, "复合语句中存在无法识别的成分");
            advance();
        }
        expect(TokenType::End, "begin 应以 end 匹配");
        return;
    }

    /*
     * if 语句：
     *
     *   if Condition then Statement
     *
     * 四元式采用“反条件跳转”：
     *   if a < b then S
     *
     * 生成：
     *   j>= a b $?     // 条件为假时跳过 S
     *   S 的四元式
     *   ...            // 回填 $? 为这里
     */
    if (match(TokenType::If)) {
        ConditionValue condition = parseCondition();
        int falseJump = 0;
        if (semantic_ != nullptr) {
            // 把条件取反，例如 < 变成 >=。result 先写 "$?"，等 then 语句结束后再回填。
            //
            // 例：if a < b then write(a)
            // 生成 j>= a b $?，当条件为假时跳到 then 语句后面。
            falseJump = semantic_->emit("j" + inverseRelation(condition.op), condition.left, condition.right, "$?");
        }
        expect(TokenType::Then, "if 条件后缺少 then");
        parseStatement();
        if (semantic_ != nullptr && falseJump != 0) {
            // 回填到下一条四元式，也就是 then 分支之后的位置。
            semantic_->patchResult(falseJump, quadTarget(semantic_->nextQuadIndex()));
        }
        return;
    }

    /*
     * while 语句：
     *
     *   while Condition do Statement
     *
     * 四元式结构：
     *   conditionStart:
     *       计算条件
     *       j<反关系> ... $?   // 条件为假时跳出循环
     *       循环体
     *       j _ _ $conditionStart
     *   afterLoop:
     *
     * falseJump 最后回填到 afterLoop。
     */
    if (match(TokenType::While)) {
        // conditionStart 记录条件判断开始处，循环体结束后要无条件跳回这里。
        int conditionStart = semantic_ != nullptr ? semantic_->nextQuadIndex() : 0;
        ConditionValue condition = parseCondition();
        int falseJump = 0;
        if (semantic_ != nullptr) {
            // 和 if 一样，先生成“条件为假则跳出循环”的占位跳转。
            falseJump = semantic_->emit("j" + inverseRelation(condition.op), condition.left, condition.right, "$?");
        }
        expect(TokenType::Do, "while 条件后缺少 do");
        parseStatement();
        if (semantic_ != nullptr) {
            // 循环体结束后无条件跳回条件开始。
            semantic_->emit("j", "_", "_", quadTarget(conditionStart));
            // 条件为假时跳到循环后第一条四元式。
            semantic_->patchResult(falseJump, quadTarget(semantic_->nextQuadIndex()));
        }
        return;
    }

    /*
     * read 语句：
     *
     *   read "(" ident { "," ident } ")"
     *
     * read 参数必须是变量，因为输入值要写入它。
     */
    if (match(TokenType::Read)) {
        expect(TokenType::LParen, "read 后缺少 (");
        Token name = expect(TokenType::Identifier, "read 参数应为标识符");
        if (semantic_ != nullptr && !name.lexeme.empty()) {
            // 输入操作会修改变量，所以这里检查的是 requireReadable，
            // 语义层会拒绝常量、过程名或未声明名字。
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

    /*
     * write 语句：
     *
     *   write "(" Expression { "," Expression } ")"
     *
     * write 参数是表达式，所以可以输出变量、数字、或者 a+b 这种计算结果。
     */
    if (match(TokenType::Write)) {
        expect(TokenType::LParen, "write 后缺少 (");
        std::string value = parseExpression();
        if (semantic_ != nullptr) {
            // write 输出表达式结果，不要求参数必须是变量。
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

/*
 * parseCondition 对应文法：
 *
 *   Condition -> "odd" Expression
 *              | Expression RelOp Expression
 *
 * 这个函数只返回条件结构，不直接生成跳转四元式。
 * 因为 if 和 while 对条件的使用方式不同，需要由外层决定跳转目标。
 */
ConditionValue Parser::parseCondition() {
    if (match(TokenType::Odd)) {
        // odd 是一元条件，只有一个表达式操作数。
        // right 用 "_" 占位，方便后续统一生成跳转四元式。
        std::string value = parseExpression();
        return ConditionValue{"odd", value, "_"};
    }

    // 普通关系条件先解析左表达式，再要求一个关系运算符，最后解析右表达式。
    // 这样 a + 1 < b * 2 会先分别得到左右两边表达式的结果位置。
    std::string left = parseExpression();
    if (!isRelation(current().type)) {
        // 缺少关系运算符时，返回一个永假/占位条件，保证外层 if/while 仍能继续走完。
        addSyntaxError(current().line, "条件中缺少关系运算符");
        return ConditionValue{"#", "0", "0"};
    }
    std::string op = relationLexeme(current().type);
    // 消费关系运算符本身，例如 < 或 >=。
    advance();
    std::string right = parseExpression();
    return ConditionValue{op, left, right};
}

/*
 * parseExpression 对应文法：
 *
 *   Expression -> [ "+" | "-" ] Term { ( "+" | "-" ) Term }
 *
 * 这里处理加减法以及开头的一元正负号。
 * 因为 Expression 调用 Term，而 Term 内部处理乘除，所以自然实现了：
 *   乘除优先级高于加减。
 *
 * 返回值：
 * - 如果不做语义分析，返回字符串仍用于语法流程占位。
 * - 如果做语义分析，返回表达式结果所在位置：
 *   可能是变量名、数字常量，也可能是临时变量 T1/T2。
 */
std::string Parser::parseExpression() {
    bool negative = false;

    // 处理可选的一元 + 或 -。
    // 一元 + 不需要生成代码；一元 - 会在第一个 Term 解析完后处理。
    if (match(TokenType::Plus)) {
        negative = false;
    } else if (match(TokenType::Minus)) {
        negative = true;
    }

    // 先解析第一个 Term。
    // 这一步会把乘除链作为一个整体读完，所以加减不会抢占乘除优先级。
    std::string left = parseTerm();

    // 一元负号 -x 等价于 0 - x，因此生成一个临时变量保存结果。
    if (negative && semantic_ != nullptr) {
        std::string temp = semantic_->newTemp();
        semantic_->emit("-", "0", left, temp);
        left = temp;
    }

    // 循环处理后续 + Term 或 - Term，实现左结合：
    // a - b - c 会被处理成 ((a - b) - c)。
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        std::string op = current().lexeme;
        advance();
        std::string right = parseTerm();
        if (semantic_ != nullptr) {
            // 每次二元运算都生成一个新临时变量。
            // 例如 a + b + c：
            //   T1 = a + b
            //   T2 = T1 + c
            std::string temp = semantic_->newTemp();
            semantic_->emit(op, left, right, temp);
            left = temp;
        }
    }
    return left;
}

/*
 * parseTerm 对应文法：
 *
 *   Term -> Factor { ( "*" | "/" ) Factor }
 *
 * 它和 parseExpression 的结构一样，只是运算符换成乘除。
 * 因为 parseExpression 只把 parseTerm 的整体结果当作一个操作数，
 * 所以 2 + 3 * 4 会先在 Term 中得到 3*4，再回到 Expression 做加法。
 */
std::string Parser::parseTerm() {
    std::string left = parseFactor();
    while (check(TokenType::Times) || check(TokenType::Slash)) {
        std::string op = current().lexeme;
        // 先保存运算符，再消费它，然后解析右侧 Factor。
        advance();
        std::string right = parseFactor();
        if (semantic_ != nullptr) {
            // 和 Expression 一样，乘除也按左结合生成临时变量。
            // 例如 a / b * c 会生成 T1 = a / b，再生成 T2 = T1 * c。
            std::string temp = semantic_->newTemp();
            semantic_->emit(op, left, right, temp);
            left = temp;
        }
    }
    return left;
}

/*
 * parseFactor 对应文法：
 *
 *   Factor -> ident | number | "(" Expression ")"
 *
 * 因子是表达式中最小的单位。
 * - ident：变量或常量名，必须能作为值使用，过程名不能出现在表达式里。
 * - number：数字字面量，直接返回原文。
 * - "(" Expression ")"：括号表达式，通过递归调用 parseExpression 实现。
 */
std::string Parser::parseFactor() {
    if (match(TokenType::Identifier)) {
        Token name = previous();
        if (semantic_ != nullptr) {
            // 表达式里出现的标识符必须能“取值”。
            // 变量和常量可以取值，过程名不能作为普通表达式因子。
            semantic_->requireValue(name);
        }
        return name.lexeme;
    }
    if (match(TokenType::Number)) {
        // 数字字面量不需要查符号表，直接把原始文本作为操作数返回。
        return previous().lexeme;
    }
    if (match(TokenType::LParen)) {
        // 括号通过递归重新进入 parseExpression，
        // 让 (a + b) * c 中的 a + b 先成为一个整体结果。
        std::string value = parseExpression();
        expect(TokenType::RParen, "表达式缺少右括号 )");
        return value;
    }

    // 三种合法因子都没有匹配到，说明表达式当前位置有语法错误。
    // 消费一个 token 是为了向前推进，避免同一个错误点反复报错。
    addSyntaxError(current().line, "表达式因子错误");
    if (!isAtEnd()) {
        advance();
    }
    return "0";
}

// 把关系运算符 Token 转成四元式使用的字符串。
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

/*
 * 返回关系运算符的“反关系”。
 *
 * 用途：if/while 生成的是“条件为假时跳转”的四元式。
 *
 * 例如：
 *   if a < b then S
 * 条件为假就是 a >= b，因此生成 j>=。
 */
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

// 判断 TokenType 是否是关系运算符。
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

// 四元式跳转目标用 "$编号" 表示，例如 "$12"。
std::string Parser::quadTarget(int index) const {
    return "$" + std::to_string(index);
}
