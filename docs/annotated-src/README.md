# 注释版源码学习说明

这个目录是从项目根目录的 `src/` 复制出来的学习副本，只用于阅读和理解代码。

原始源码仍然在 `src/`，项目实际编译、测试、运行都使用原始源码，不使用这个目录。

## 建议阅读顺序

1. `common/Token.h`、`common/Token.cpp`

   先理解 Token 是什么，以及保留字、标识符、数字、运算符、界符这些类别如何区分。

2. `lexer/Lexer.h`、`lexer/Lexer.cpp`

   看源代码字符如何被扫描成 Token 序列。重点理解 `pos_`、`line_`、`column_` 如何移动。

3. `parser/Parser.h`、`parser/Parser.cpp`

   这是最重要的部分。它是递归下降语法分析器，每个 `parseXXX` 函数基本对应一个文法非终结符。

4. `semantic/Semantic.h`、`semantic/Semantic.cpp`

   看符号表、语义错误、四元式是怎么保存的。Parser 识别语法时会调用这里的函数。

5. `target/TargetCode.h`、`target/TargetCode.cpp`

   看四元式如何翻译成目标代码，以及 `run` 模式如何解释执行目标指令。

6. `parser/SLRParser.h`、`parser/SLRParser.cpp`

   这部分用于理解 SLR 分析表、LR(0) 项目集、移进归约过程。它不生成四元式。

7. `main.cpp`

   最后看命令行入口，理解 `lex`、`parse-ll`、`sem-ll`、`target`、`run` 等模式如何串起各个模块。

## 核心调用链

```text
源代码文件
  -> Lexer
  -> Token 序列
  -> Parser
  -> SemanticAnalyzer
  -> 四元式
  -> TargetCode
  -> 目标指令
  -> TargetMachine 执行
```

其中：

- `parse-ll` 只走到 `Parser`。
- `sem-ll` 走到 `SemanticAnalyzer` 并输出四元式。
- `target` 走到目标代码打印。
- `run` 会真正解释执行目标代码并输出运行结果。
- `parse-lr` 和 `lr-dot` 用于展示 SLR 分析，不是主语义生成路径。
