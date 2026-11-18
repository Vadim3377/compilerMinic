// parser.cpp - predictive recursive descent parser for MiniC (Part 1, Task 2).
//
// There is one function per nonterminal of the transformed grammar in
// new_grammar.txt. Each function looks only at the current token and picks a
// production by checking it against the FIRST sets (first_sets.txt), or
// against the FOLLOW sets (follow_sets.txt) when the production is ε. The
// comment above each function is the grammar rule it implements, so the code
// can be checked against the grammar line by line.
//
// Functions marked "starter" came with the coursework; the rest were written
// for Part 1, and some starter functions were fixed along the way (noted
// where it happened).

#include "parser.h"

#include <cstdlib>
#include <deque>

//===----------------------------------------------------------------------===//
// Token buffer (starter)
//
// CurTok is the token the parser is looking at. The starter's putBackToken
// was needed for the two-token lookahead in ParseExper; Step 10 made that
// rule LL(1), so it is gone.
//===----------------------------------------------------------------------===//

static TOKEN CurTok;
static std::deque<TOKEN> tok_buffer;

static TOKEN getNextToken() {

  if (tok_buffer.size() == 0)
    tok_buffer.push_back(gettok());

  TOKEN temp = tok_buffer.front();
  tok_buffer.pop_front();

  return CurTok = temp;
}

// Top-level declarations are collected here while parsing, and ParseProgram
// moves them into the ProgramAST root at the end.
static std::vector<std::unique_ptr<FunctionPrototypeAST>> ProgramExterns;
static std::vector<std::unique_ptr<ASTnode>> ProgramDecls;

// Report a syntax error at a token and stop. The message starts with
// "Syntax error" because the spec wants every failure to name its kind
// (Part 2 Step 0 changed the wording; before that it just said "Error").
// The return statements are never reached, but they keep the compiler happy
// at call sites that write "return LogError(...)".
static std::unique_ptr<ASTnode> LogError(TOKEN tok, const char *Str) {
  fprintf(stderr, "%d:%d Syntax error: %s\n", tok.lineNo, tok.columnNo, Str);
  exit(2);
  return nullptr;
}

static std::unique_ptr<FunctionPrototypeAST> LogErrorP(TOKEN tok,
                                                       const char *Str) {
  LogError(tok, Str);
  exit(2);
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Recursive Descent - Function call for each production
//===----------------------------------------------------------------------===//

static std::unique_ptr<ASTnode> ParseDecl();
static std::unique_ptr<ASTnode> ParseStmt();
static std::unique_ptr<ASTnode> ParseBlock();
static std::unique_ptr<ASTnode> ParseExper();
static std::unique_ptr<ParamAST> ParseParam();
static std::unique_ptr<VarDeclAST> ParseLocalDecl();
static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime();
static std::unique_ptr<ASTnode> ParseRval();
static std::unique_ptr<ASTnode> ParseUnaryExpr();

// element ::= FLOAT_LIT   (starter)
static std::unique_ptr<ASTnode> ParseFloatNumberExpr() {
  auto Result = std::make_unique<FloatASTnode>(CurTok, CurTok.getFloatVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// element ::= INT_LIT   (starter)
static std::unique_ptr<ASTnode> ParseIntNumberExpr() {
  auto Result = std::make_unique<IntASTnode>(CurTok, CurTok.getIntVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// element ::= BOOL_LIT   (starter)
static std::unique_ptr<ASTnode> ParseBoolExpr() {
  auto Result = std::make_unique<BoolASTnode>(CurTok, CurTok.getBoolVal());
  getNextToken(); // consume the literal
  return std::move(Result);
}

// param_list_prime ::= "," param param_list_prime
//                   |  ε                              (starter)
static std::vector<std::unique_ptr<ParamAST>> ParseParamListPrime() {
  std::vector<std::unique_ptr<ParamAST>> param_list;

  if (CurTok.type == COMMA) { // more parameters in list
    getNextToken();           // eat ","

    auto param = ParseParam();
    if (param) {
      printf("found param in param_list_prime: %s\n", param->getName().c_str());
      param_list.push_back(std::move(param));
      auto param_list_prime = ParseParamListPrime();
      for (unsigned i = 0; i < param_list_prime.size(); i++) {
        param_list.push_back(std::move(param_list_prime.at(i)));
      }
    }
  } else if (CurTok.type == RPAR) { // FOLLOW(param_list_prime)
    // expand by param_list_prime ::= ε
  } else {
    LogError(CurTok, "expected ',' or ')' in list of parameter declarations");
  }

  return param_list;
}

// dims ::= "[" INT_LIT "]" dims | ε
// Step 10. The sizes after a name in an array declaration, as in
// "int a[3][4];". Sizes must be integer literals, because the memory is
// reserved when the program is compiled. A parameter may leave the first
// size out ("int a[]"), stored as 0, since a function never needs it to
// find an element (see the theory section on row-major layout). Whether
// there are at most 3 dimensions and the sizes are positive is checked in
// codegen, so the error can say "array dimension".
static std::vector<int> ParseDims(bool allowUnsizedFirst) {
  std::vector<int> dims;
  while (CurTok.type == LBOX) {
    getNextToken(); // eat "["
    if (CurTok.type == INT_LIT) {
      dims.push_back(CurTok.getIntVal());
      getNextToken(); // eat the size
    } else if (CurTok.type == RBOX && allowUnsizedFirst && dims.empty()) {
      dims.push_back(0);
    } else {
      LogError(CurTok, "expected an integer literal as the array size");
    }
    if (CurTok.type != RBOX)
      LogError(CurTok, "expected ']' after array size");
    getNextToken(); // eat "]"
  }
  return dims;
}

// param ::= var_type IDENT param_dims
// (Step 10 added param_dims, so a parameter can be an array: "int a[10]";
// Step 13 generates code for such parameters.)
//
// Starter, fixed in Part 1: the original version read the type, then never
// created the ParamAST, so every parameter silently vanished from the
// function's signature. It now checks both tokens and builds the node.
static std::unique_ptr<ParamAST> ParseParam() {

  if (CurTok.type != INT_TOK && CurTok.type != FLOAT_TOK &&
      CurTok.type != BOOL_TOK) {
    LogError(CurTok, "expected 'int', 'float' or 'bool' in parameter declaration");
    return nullptr;
  }
  std::string Type = CurTok.lexeme;
  getNextToken(); // eat the type token

  if (CurTok.type != IDENT) {
    LogError(CurTok, "expected identifier in parameter declaration");
    return nullptr;
  }
  std::string Name = CurTok.getIdentifierStr();
  getNextToken(); // eat "IDENT"

  std::vector<int> dims = ParseDims(true);
  return std::make_unique<ParamAST>(Name, Type, std::move(dims));
}

// param_list ::= param param_list_prime   (starter)
static std::vector<std::unique_ptr<ParamAST>> ParseParamList() {
  std::vector<std::unique_ptr<ParamAST>> param_list;

  auto param = ParseParam();
  if (param) {
    param_list.push_back(std::move(param));
    auto param_list_prime = ParseParamListPrime();
    for (unsigned i = 0; i < param_list_prime.size(); i++) {
      param_list.push_back(std::move(param_list_prime.at(i)));
    }
  }

  return param_list;
}

// params ::= param_list
//         |  "void"
//         |  ε                (starter)
static std::vector<std::unique_ptr<ParamAST>> ParseParams() {
  std::vector<std::unique_ptr<ParamAST>> param_list;

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(param_list)

    auto list = ParseParamList();
    for (unsigned i = 0; i < list.size(); i++) {
      param_list.push_back(std::move(list.at(i)));
    }

  } else if (CurTok.type == VOID_TOK) { // FIRST("void")
    getNextToken(); // eat 'void'
    if (CurTok.type != RPAR) {
      LogError(CurTok, "expected ')', after 'void' in \
       end of function declaration");
    }
  } else if (CurTok.type == RPAR) { // FOLLOW(params)
    // expand by params ::= ε
  } else {
    LogError(
        CurTok,
        "expected 'int', 'bool' or 'float' in function declaration or ') in \
       end of function declaration");
  }

  return param_list;
}

//===----------------------------------------------------------------------===//
// Expressions (Part 1, Task 2)
//
// The starter grammar had a single ambiguous, left-recursive rule
// "rval ::= rval op rval | ...". A recursive descent parser cannot use that
// directly: on "rval ::= rval ..." the function would call itself forever
// without consuming a token. So the grammar was rewritten into one level per
// precedence (|| lowest, then &&, ==, <, +, *, unary), with left recursion
// removed the usual way:
//
//     X  ::= Next X'
//     X' ::= op Next X' | ε
//
// Removing left recursion makes the grammar look right-associative, which
// would turn a - b - c into a - (b - c). To keep the correct (a - b) - c,
// each X' function takes the tree built so far as a parameter (an inherited
// attribute), combines it with the next operand, and passes the bigger tree
// down again.
//
// Each X' function saves the whole operator TOKEN rather than just its type
// so that semantic errors can later report where the operator was.
//===----------------------------------------------------------------------===//

// FIRST(expr) = { "-", "!", "(", IDENT, INT_LIT, FLOAT_LIT, BOOL_LIT }
// Several statement rules start with expr, so this set is used all over the
// parser and lives in one helper instead of being copied into each check.
static bool inFirstExpr(int t) {
  return t == MINUS || t == NOT || t == LPAR || t == IDENT || t == INT_LIT ||
         t == FLOAT_LIT || t == BOOL_LIT;
}

// FOLLOW sets of the X' nonterminals, used to decide when to take the ε
// production. They nest: whatever may follow a lower-precedence level may
// also follow a higher one, plus the operators of the level in between.
// For example after "a * b" we may see "+", because "a * b + c" is valid.
// FOLLOW(rval') = { ")", ",", ";", "]", "=" }
// Step 10 added "]" (an index expression ends there, as in a[i + 1]) and
// "=" (the left side of an assignment is now parsed as an rval first).
static bool inFollowRvalPrime(int t) {
  return t == RPAR || t == COMMA || t == SC || t == RBOX || t == ASSIGN;
}
// FOLLOW(and_expr') = FOLLOW(rval') + { "||" }
static bool inFollowAndPrime(int t) { return inFollowRvalPrime(t) || t == OR; }
// FOLLOW(eq_expr') = FOLLOW(and_expr') + { "&&" }
static bool inFollowEqPrime(int t) { return inFollowAndPrime(t) || t == AND; }
// FOLLOW(rel_expr') = FOLLOW(eq_expr') + { "==", "!=" }
static bool inFollowRelPrime(int t) {
  return inFollowEqPrime(t) || t == EQ || t == NE;
}
// FOLLOW(add_expr') = FOLLOW(rel_expr') + { "<", "<=", ">", ">=" }
static bool inFollowAddPrime(int t) {
  return inFollowRelPrime(t) || t == LT || t == LE || t == GT || t == GE;
}
// FOLLOW(mul_expr') = FOLLOW(add_expr') + { "+", "-" }
static bool inFollowMulPrime(int t) {
  return inFollowAddPrime(t) || t == PLUS || t == MINUS;
}
// FOLLOW(ident_tail) = FOLLOW(mul_expr') + { "*", "/", "%" }
static bool inFollowIdentTail(int t) {
  return inFollowMulPrime(t) || t == ASTERIX || t == DIV || t == MOD;
}

// arg_list  ::= expr arg_list'
// arg_list' ::= "," expr arg_list' | ε
// arg_list' is tail recursive, so it is written as a loop.
static std::vector<std::unique_ptr<ASTnode>> ParseArgList() {
  std::vector<std::unique_ptr<ASTnode>> args;
  args.push_back(ParseExper());
  while (CurTok.type == COMMA) {
    getNextToken(); // eat ","
    args.push_back(ParseExper());
  }
  if (CurTok.type != RPAR) // FOLLOW(arg_list') = { ")" }
    LogError(CurTok, "expected ',' or ')' in function call arguments");
  return args;
}

// args ::= arg_list | ε
static std::vector<std::unique_ptr<ASTnode>> ParseArgs() {
  std::vector<std::unique_ptr<ASTnode>> args;
  if (inFirstExpr(CurTok.type)) {
    args = ParseArgList();
  } else if (CurTok.type == RPAR) { // FOLLOW(args): expand by ε
  } else {
    LogError(CurTok, "expected expression or ')' in function call");
  }
  return args;
}

// indices ::= "[" expr "]" indices | ε
// Step 10. The index expressions of an element access, as many as are
// written. Each one is a full expression, so a[(i + 1) * f(k)] works, and
// FOLLOW(rval') contains "]" so the expression stops at the bracket.
static std::vector<std::unique_ptr<ASTnode>> ParseIndices() {
  std::vector<std::unique_ptr<ASTnode>> indices;
  while (CurTok.type == LBOX) {
    getNextToken(); // eat "["
    indices.push_back(ParseExper());
    if (CurTok.type != RBOX)
      LogError(CurTok, "expected ']' after array index");
    getNextToken(); // eat "]"
  }
  return indices;
}

// ident_tail ::= "(" args ")" | "[" expr "]" indices | ε
// The starter grammar had "IDENT | IDENT ( args )", two alternatives with the
// same first token. Left factoring pulls IDENT out, and this function decides
// afterwards whether it was a call, a plain variable or (Step 10) an array
// element. Element access sits here, beside calls, because the spec puts it
// at the same precedence as a function call.
static std::unique_ptr<ASTnode> ParseIdentTail(TOKEN identTok) {
  std::string Name = identTok.getIdentifierStr();

  if (CurTok.type == LBOX) { // array element: a[i], m[i][j]
    auto indices = ParseIndices();
    return std::make_unique<ArrayAccessAST>(identTok, Name, std::move(indices));
  }

  if (CurTok.type == LPAR) { // function call
    getNextToken();          // eat "("
    auto args = ParseArgs();
    if (CurTok.type != RPAR)
      return LogError(CurTok, "expected ')' to close function call");
    getNextToken(); // eat ")"
    return std::make_unique<ArgsAST>(identTok, Name, std::move(args));
  }
  if (inFollowIdentTail(CurTok.type)) { // plain variable: expand by ε
    return std::make_unique<VariableASTnode>(identTok, Name);
  }
  std::string msg = "expected an operator, '(', ';', ')' or ',' after "
                    "identifier '" + Name + "', but found '" + CurTok.lexeme + "'";
  return LogError(CurTok, msg.c_str());
}

// primary ::= "(" expr ")" | IDENT ident_tail | INT_LIT | FLOAT_LIT | BOOL_LIT
// Brackets need no AST node of their own: the tree's shape already records
// the grouping, so we return the inner expression as it is.
static std::unique_ptr<ASTnode> ParsePrimary() {
  if (CurTok.type == LPAR) {
    getNextToken(); // eat "("
    auto expr = ParseExper();
    if (CurTok.type != RPAR)
      return LogError(CurTok, "expected ')' to close parenthesised expression");
    getNextToken(); // eat ")"
    return expr;
  }
  if (CurTok.type == IDENT) {
    TOKEN identTok = CurTok;
    getNextToken(); // eat IDENT
    return ParseIdentTail(identTok);
  }
  if (CurTok.type == INT_LIT)
    return ParseIntNumberExpr();
  if (CurTok.type == FLOAT_LIT)
    return ParseFloatNumberExpr();
  if (CurTok.type == BOOL_LIT)
    return ParseBoolExpr();
  return LogError(CurTok, "expected an expression (identifier, literal or '(')");
}

// unary_expr ::= "-" unary_expr | "!" unary_expr | primary
// Unary operators are right recursive by nature (--x is -(-x)), so there is
// no X' function here and no left recursion to remove.
static std::unique_ptr<ASTnode> ParseUnaryExpr() {
  if (CurTok.type == MINUS || CurTok.type == NOT) {
    TOKEN op = CurTok;
    getNextToken(); // eat operator
    auto operand = ParseUnaryExpr();
    return std::make_unique<UnaryExprAST>(op, std::move(operand));
  }
  return ParsePrimary();
}

// mul_expr' ::= ("*" | "/" | "%") unary_expr mul_expr' | ε
static std::unique_ptr<ASTnode> ParseMulExprPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == ASTERIX || CurTok.type == DIV || CurTok.type == MOD) {
    TOKEN op = CurTok;
    getNextToken(); // eat operator
    auto rhs = ParseUnaryExpr();
    auto node = std::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
    return ParseMulExprPrime(std::move(node));
  }
  if (inFollowMulPrime(CurTok.type))
    return lhs;
  // FOLLOW(mul_expr') already contains every operator of the lower levels,
  // so a bad token straight after an operand is always caught here, and
  // this is the place for the most helpful error message.
  std::string msg = "expected an operator, ';', ')', ']', ',' or '=' after "
                    "expression, but found '" + CurTok.lexeme + "'";
  return LogError(CurTok, msg.c_str());
}

// mul_expr ::= unary_expr mul_expr'
static std::unique_ptr<ASTnode> ParseMulExpr() {
  return ParseMulExprPrime(ParseUnaryExpr());
}

// add_expr' ::= ("+" | "-") mul_expr add_expr' | ε
static std::unique_ptr<ASTnode> ParseAddExprPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == PLUS || CurTok.type == MINUS) {
    TOKEN op = CurTok;
    getNextToken(); // eat operator
    auto rhs = ParseMulExpr();
    auto node = std::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
    return ParseAddExprPrime(std::move(node));
  }
  if (inFollowAddPrime(CurTok.type))
    return lhs;
  return LogError(CurTok, "unexpected token in expression");
}

// add_expr ::= mul_expr add_expr'
static std::unique_ptr<ASTnode> ParseAddExpr() {
  return ParseAddExprPrime(ParseMulExpr());
}

// rel_expr' ::= ("<=" | "<" | ">=" | ">") add_expr rel_expr' | ε
static std::unique_ptr<ASTnode> ParseRelExprPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == LE || CurTok.type == LT || CurTok.type == GE ||
      CurTok.type == GT) {
    TOKEN op = CurTok;
    getNextToken(); // eat operator
    auto rhs = ParseAddExpr();
    auto node = std::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
    return ParseRelExprPrime(std::move(node));
  }
  if (inFollowRelPrime(CurTok.type))
    return lhs;
  return LogError(CurTok, "unexpected token in expression");
}

// rel_expr ::= add_expr rel_expr'
static std::unique_ptr<ASTnode> ParseRelExpr() {
  return ParseRelExprPrime(ParseAddExpr());
}

// eq_expr' ::= ("==" | "!=") rel_expr eq_expr' | ε
static std::unique_ptr<ASTnode> ParseEqExprPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == EQ || CurTok.type == NE) {
    TOKEN op = CurTok;
    getNextToken(); // eat operator
    auto rhs = ParseRelExpr();
    auto node = std::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
    return ParseEqExprPrime(std::move(node));
  }
  if (inFollowEqPrime(CurTok.type))
    return lhs;
  return LogError(CurTok, "unexpected token in expression");
}

// eq_expr ::= rel_expr eq_expr'
static std::unique_ptr<ASTnode> ParseEqExpr() {
  return ParseEqExprPrime(ParseRelExpr());
}

// and_expr' ::= "&&" eq_expr and_expr' | ε
static std::unique_ptr<ASTnode> ParseAndExprPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == AND) {
    TOKEN op = CurTok;
    getNextToken(); // eat "&&"
    auto rhs = ParseEqExpr();
    auto node = std::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
    return ParseAndExprPrime(std::move(node));
  }
  if (inFollowAndPrime(CurTok.type))
    return lhs;
  return LogError(CurTok, "unexpected token in expression");
}

// and_expr ::= eq_expr and_expr'
static std::unique_ptr<ASTnode> ParseAndExpr() {
  return ParseAndExprPrime(ParseEqExpr());
}

// rval' ::= "||" and_expr rval' | ε
static std::unique_ptr<ASTnode> ParseRvalPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == OR) {
    TOKEN op = CurTok;
    getNextToken(); // eat "||"
    auto rhs = ParseAndExpr();
    auto node = std::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
    return ParseRvalPrime(std::move(node));
  }
  if (inFollowRvalPrime(CurTok.type))
    return lhs;
  return LogError(CurTok, "expected an operator, ')', ',' or ';' in expression");
}

// rval ::= and_expr rval'
static std::unique_ptr<ASTnode> ParseRval() {
  return ParseRvalPrime(ParseAndExpr());
}

// expr      ::= rval expr_tail
// expr_tail ::= "=" expr | ε
//
// Step 10 rewrote this rule. Part 1 had "expr ::= IDENT "=" expr | rval",
// which needed two tokens of lookahead to see the "=". With arrays the "=" can
// be arbitrarily far away ("a[i + 1][j] = 3"), so no fixed lookahead works.
// The fix used by real C compilers: parse the left side as an ordinary
// expression first, and only when the next token is "=" check that what we
// parsed can be assigned to (a variable or an array element). The grammar is
// LL(1) again; "x + 1 = 2" parses as an rval and is then rejected here.
// The right side calls ParseExper again, so a = b = c is right associative.
static std::unique_ptr<ASTnode> ParseExper() {
  if (!inFirstExpr(CurTok.type))
    return LogError(CurTok, "expected an expression");
  auto lhs = ParseRval();

  if (CurTok.type == ASSIGN) {
    TOKEN eqTok = CurTok;
    if (!lhs->isAssignable())
      return LogError(eqTok, "the left-hand side of '=' must be a variable "
                             "or an array element");
    getNextToken(); // eat "="
    auto val = ParseExper();
    return std::make_unique<AssignAST>(eqTok, std::move(lhs), std::move(val));
  }
  return lhs;
}

//===----------------------------------------------------------------------===//
// Statements, blocks and declarations (mostly starter)
//===----------------------------------------------------------------------===//

// expr_stmt ::= expr ";"
//            |  ";"
static std::unique_ptr<ASTnode> ParseExperStmt() {

  if (CurTok.type == SC) { // empty statement
    getNextToken();        // eat ;
    return nullptr;
  } else {
    auto expr = ParseExper();
    if (expr) {
      if (CurTok.type == SC) {
        getNextToken(); // eat ;
        return expr;
      } else {
        LogError(CurTok, "expected ';' to end expression statement");
      }
    } else
      return nullptr;
  }
  return nullptr;
}

// else_stmt  ::= "else" block
//             |  ε
//
// Starter, fixed in Part 1: the ε branch now checks the real
// FOLLOW(else_stmt) from follow_sets.txt. The old list also accepted "else"
// and "+", and MiniC has no unary plus.
static std::unique_ptr<ASTnode> ParseElseStmt() {

  if (CurTok.type == ELSE) { // FIRST(else_stmt)
    getNextToken(); // eat "else"

    if (!(CurTok.type == LBRA)) {
      return LogError(
          CurTok, "expected { to start else block of if-then-else statment");
    }
    auto Else = ParseBlock();
    if (!Else)
      return nullptr;
    return Else;
  } else if (inFirstExpr(CurTok.type) || CurTok.type == SC ||
             CurTok.type == LBRA || CurTok.type == WHILE ||
             CurTok.type == IF || CurTok.type == RETURN ||
             CurTok.type == RBRA) { // FOLLOW(else_stmt)
    // expand by else_stmt ::= ε
    return nullptr;
  } else
    LogError(CurTok, "expected 'else' or the start of the next statement or '}'");

  return nullptr;
}

// if_stmt ::= "if" "(" expr ")" block else_stmt   (starter)
static std::unique_ptr<ASTnode> ParseIfStmt() {
  TOKEN ifTok = CurTok;
  getNextToken(); // eat the if.
  if (CurTok.type == LPAR) {
    getNextToken(); // eat (
    auto Cond = ParseExper();
    if (!Cond)
      return nullptr;
    if (CurTok.type != RPAR)
      return LogError(CurTok, "expected )");
    getNextToken(); // eat )

    if (!(CurTok.type == LBRA)) {
      return LogError(CurTok, "expected { to start then block of if statment");
    }

    auto Then = ParseBlock();
    if (!Then)
      return nullptr;
    auto Else = ParseElseStmt();

    return std::make_unique<IfExprAST>(ifTok, std::move(Cond), std::move(Then),
                                       std::move(Else));

  } else
    return LogError(CurTok, "expected (");

  return nullptr;
}

// return_stmt ::= "return" ";"
//             |  "return" expr ";"      (starter)
static std::unique_ptr<ASTnode> ParseReturnStmt() {
  TOKEN retTok = CurTok;
  getNextToken(); // eat the return
  if (CurTok.type == SC) {
    getNextToken(); // eat the ;
    return std::make_unique<ReturnAST>(retTok, nullptr);
  } else if (inFirstExpr(CurTok.type)) { // FIRST(expr)
    auto val = ParseExper();
    if (!val)
      return nullptr;

    if (CurTok.type == SC) {
      getNextToken(); // eat the ;
      return std::make_unique<ReturnAST>(retTok, std::move(val));
    } else
      return LogError(CurTok, "expected ';'");
  } else
    return LogError(CurTok, "expected ';' or expression");

  return nullptr;
}

// while_stmt ::= "while" "(" expr ")" stmt
//
// Starter, fixed in Part 2 Step 0: the body of "while (c) ;" is the empty
// statement, which the parser represents as nullptr. The old code took that
// nullptr as a failure and dropped the whole loop, so a loop that should run
// (and might never terminate) disappeared from the program. Now the node is
// always built and codegen treats a null body as "do nothing".
static std::unique_ptr<ASTnode> ParseWhileStmt() {
  TOKEN whileTok = CurTok;
  getNextToken(); // eat the while.
  if (CurTok.type == LPAR) {
    getNextToken(); // eat (
    auto Cond = ParseExper();
    if (!Cond)
      return nullptr;
    if (CurTok.type != RPAR)
      return LogError(CurTok, "expected )");
    getNextToken(); // eat )

    auto Body = ParseStmt();

    return std::make_unique<WhileExprAST>(whileTok, std::move(Cond),
                                          std::move(Body));
  } else
    return LogError(CurTok, "expected (");
}

// stmt ::= expr_stmt
//      |  block
//      |  if_stmt
//      |  while_stmt
//      |  return_stmt          (starter)
static std::unique_ptr<ASTnode> ParseStmt() {

  if (inFirstExpr(CurTok.type) || CurTok.type == SC) { // FIRST(expr_stmt)
    auto expr_stmt = ParseExperStmt();
    fprintf(stderr, "Parsed an expression statement\n");
    return expr_stmt;
  } else if (CurTok.type == LBRA) { // FIRST(block)
    auto block_stmt = ParseBlock();
    if (block_stmt) {
      fprintf(stderr, "Parsed a block\n");
      return block_stmt;
    }
  } else if (CurTok.type == IF) { // FIRST(if_stmt)
    auto if_stmt = ParseIfStmt();
    if (if_stmt) {
      fprintf(stderr, "Parsed an if statment\n");
      return if_stmt;
    }
  } else if (CurTok.type == WHILE) { // FIRST(while_stmt)
    auto while_stmt = ParseWhileStmt();
    if (while_stmt) {
      fprintf(stderr, "Parsed a while statment\n");
      return while_stmt;
    }
  } else if (CurTok.type == RETURN) { // FIRST(return_stmt)
    auto return_stmt = ParseReturnStmt();
    if (return_stmt) {
      fprintf(stderr, "Parsed a return statment\n");
      return return_stmt;
    }
  } else {
    return LogError(CurTok, "expected a statement (expression, ';', '{', "
                            "'if', 'while' or 'return')");
  }
  return nullptr;
}

// stmt_list ::= stmt stmt_list | ε
//
// Starter, fixed in Part 1: the starter version always parsed one statement
// first, so a block with declarations and no statements, such as
// "{ int x; }", was a syntax error. A block may be empty, which makes
// stmt_list and stmt_list_prime the same rule.
static std::vector<std::unique_ptr<ASTnode>> ParseStmtList() {
  return ParseStmtListPrime();
}

// stmt_list_prime ::= stmt stmt_list_prime
//                  |  ε
static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime() {
  std::vector<std::unique_ptr<ASTnode>> stmt_list;
  if (inFirstExpr(CurTok.type) || CurTok.type == SC || CurTok.type == LBRA ||
      CurTok.type == WHILE || CurTok.type == IF ||
      CurTok.type == RETURN) { // FIRST(stmt)
    auto stmt = ParseStmt();
    if (stmt) {
      stmt_list.push_back(std::move(stmt));
    }
    auto stmt_prime = ParseStmtListPrime();
    for (unsigned i = 0; i < stmt_prime.size(); i++) {
      stmt_list.push_back(std::move(stmt_prime.at(i)));
    }

  } else if (CurTok.type == RBRA) { // FOLLOW(stmt_list_prime)
    // expand by stmt_list_prime ::= ε
  } else {
    LogError(CurTok, "expected a statement or '}'");
  }
  return stmt_list;
}

// local_decls_prime ::= local_decl local_decls_prime
//                    |  ε
//
// Starter, fixed in Part 1: "}" was missing from FOLLOW(local_decls_prime),
// which is the second reason "{ int x; }" used to be rejected.
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDeclsPrime() {
  std::vector<std::unique_ptr<VarDeclAST>> local_decls_prime;

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(local_decl)
    auto local_decl = ParseLocalDecl();
    if (local_decl) {
      local_decls_prime.push_back(std::move(local_decl));
    }
    auto prime = ParseLocalDeclsPrime();
    for (unsigned i = 0; i < prime.size(); i++) {
      local_decls_prime.push_back(std::move(prime.at(i)));
    }
  } else if (inFirstExpr(CurTok.type) || CurTok.type == SC ||
             CurTok.type == LBRA || CurTok.type == IF || CurTok.type == WHILE ||
             CurTok.type == RETURN ||
             CurTok.type == RBRA) { // FOLLOW(local_decls_prime)
    // expand by local_decls_prime ::= ε
  } else {
    LogError(CurTok, "expected a local variable declaration, a statement "
                     "or '}'");
  }

  return local_decls_prime;
}

// local_decl ::= var_type IDENT dims ";"
// var_type ::= "int"
//           |  "float"
//           |  "bool"             (starter; Step 10 added dims for arrays)
static std::unique_ptr<VarDeclAST> ParseLocalDecl() {
  TOKEN PrevTok;
  std::string Type;
  std::string Name = "";
  std::unique_ptr<VarDeclAST> local_decl;

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(var_type)
    PrevTok = CurTok;
    getNextToken(); // eat 'int' or 'float or 'bool'
    if (CurTok.type == IDENT) {
      Type = PrevTok.lexeme;
      Name = CurTok.getIdentifierStr();
      auto ident = std::make_unique<VariableASTnode>(CurTok, Name);

      getNextToken(); // eat 'IDENT'
      std::vector<int> dims = ParseDims(false);
      local_decl =
          std::make_unique<VarDeclAST>(std::move(ident), Type, std::move(dims));
      if (CurTok.type != SC) {
        LogError(CurTok, "Expected '[' or ';' to end local variable declaration");
        return nullptr;
      }
      getNextToken(); // eat ';'
      fprintf(stderr, "Parsed a local variable declaration\n");
    } else {
      LogError(CurTok, "expected identifier' in local variable declaration");
      return nullptr;
    }
  }
  return local_decl;
}

// local_decls ::= local_decl local_decls | ε
// Same rule as local_decls_prime once the grammar was fixed.
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDecls() {
  return ParseLocalDeclsPrime();
}

// block ::= "{" local_decls stmt_list "}"   (starter)
static std::unique_ptr<ASTnode> ParseBlock() {
  std::vector<std::unique_ptr<VarDeclAST>> local_decls;
  std::vector<std::unique_ptr<ASTnode>> stmt_list;

  getNextToken(); // eat '{'

  local_decls = ParseLocalDecls();
  fprintf(stderr, "Parsed a set of local variable declaration\n");
  stmt_list = ParseStmtList();
  fprintf(stderr, "Parsed a list of statements\n");
  if (CurTok.type == RBRA)
    getNextToken(); // eat '}'
  else {
    LogError(CurTok, "expected '}' , close body of block");
    return nullptr;
  }

  return std::make_unique<BlockAST>(std::move(local_decls),
                                    std::move(stmt_list));
}

// decl ::= var_decl
//       |  fun_decl
//
// Both start with "type IDENT", so this starter function reads those two
// tokens first and then looks at the third: ";" means a global variable,
// "[" a global array (Step 10) and "(" a function. One Part 1 fix: the starter read the identifier
// before checking it was one, so "int ;" stopped with a confusing internal
// message instead of a syntax error.
static std::unique_ptr<ASTnode> ParseDecl() {
  std::string IdName;
  TOKEN IdTok;
  std::vector<std::unique_ptr<ParamAST>> param_list;

  TOKEN PrevTok = CurTok; // the type token

  if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
      CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {
    getNextToken(); // eat the VOID_TOK, INT_TOK, BOOL_TOK or FLOAT_TOK

    if (CurTok.type == IDENT) {
      IdName = CurTok.getIdentifierStr();
      IdTok = CurTok;
      auto ident = std::make_unique<VariableASTnode>(CurTok, IdName);
      getNextToken(); // eat the IDENT
      if (CurTok.type == SC || CurTok.type == LBOX) { // global variable or array
        std::vector<int> dims = ParseDims(false);
        if (CurTok.type != SC)
          return LogError(CurTok, "expected ';' after global array declaration");
        getNextToken(); // eat ;
        fprintf(stderr, "Parsed a variable declaration\n");

        if (PrevTok.type != VOID_TOK)
          return std::make_unique<GlobVarDeclAST>(
              std::move(ident), PrevTok.lexeme, std::move(dims));
        else
          return LogError(PrevTok,
                          "Cannot have variable declaration with type 'void'");
      } else if (CurTok.type == LPAR) { // function declaration
        getNextToken(); // eat (

        auto P = ParseParams();
        fprintf(stderr, "Parsed parameter list for function\n");

        if (CurTok.type != RPAR)
          return LogError(CurTok, "expected ')' in function declaration");

        getNextToken(); // eat )
        if (CurTok.type != LBRA)
          return LogError(
              CurTok, "expected '{' in function declaration, function body");

        auto B = ParseBlock();
        if (!B)
          return nullptr;
        else
          fprintf(stderr, "Parsed block of statements in function\n");

        fprintf(stderr, "Parsed a function declaration\n");

        auto Proto = std::make_unique<FunctionPrototypeAST>(
            IdTok, IdName, PrevTok.lexeme, std::move(P));
        return std::make_unique<FunctionDeclAST>(std::move(Proto),
                                                 std::move(B));
      } else
        return LogError(CurTok, "expected ';', '[' or '('");
    } else
      return LogError(CurTok, "expected an identifier");

  } else
    LogError(CurTok, "expected 'void', 'int' or 'float' or EOF token");

  return nullptr;
}

// decl_list_prime ::= decl decl_list_prime
//                  |  ε                     (starter, now stores each decl)
static void ParseDeclListPrime() {
  if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
      CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) { // FIRST(decl)

    if (auto decl = ParseDecl()) {
      fprintf(stderr, "Parsed a top-level variable or function declaration\n");
      ProgramDecls.push_back(std::move(decl));
    }
    ParseDeclListPrime();
  } else if (CurTok.type == EOF_TOK) { // FOLLOW(decl_list_prime)
    // expand by decl_list_prime ::= ε
  } else {
    LogError(CurTok, "expected 'void', 'int', 'bool' or 'float' or EOF token");
  }
}

// decl_list ::= decl decl_list_prime   (starter, now stores each decl)
static void ParseDeclList() {
  auto decl = ParseDecl();
  if (decl) {
    fprintf(stderr, "Parsed a top-level variable or function declaration\n");
    ProgramDecls.push_back(std::move(decl));
    ParseDeclListPrime();
  }
}

// extern ::= "extern" type_spec IDENT "(" params ")" ";"
//
// Starter, fixed in Part 1: it returned nothing when the parameter list was
// empty, so "extern int read();" was thrown away and the parse then failed.
// An empty list is now accepted, the same as for a function definition.
static std::unique_ptr<FunctionPrototypeAST> ParseExtern() {
  std::string IdName;
  TOKEN IdTok;
  TOKEN PrevTok;

  if (CurTok.type == EXTERN) {
    getNextToken(); // eat the EXTERN

    if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
        CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {

      PrevTok = CurTok; // the return type token
      getNextToken();   // eat the VOID_TOK, INT_TOK, BOOL_TOK or FLOAT_TOK

      if (CurTok.type == IDENT) {
        IdName = CurTok.getIdentifierStr();
        IdTok = CurTok;
        auto ident = std::make_unique<VariableASTnode>(CurTok, IdName);
        getNextToken(); // eat the IDENT

        if (CurTok.type == LPAR) {
          getNextToken(); // eat (

          auto P = ParseParams();
          fprintf(stderr, "Parsed parameter list for external function\n");

          if (CurTok.type != RPAR)
            return LogErrorP(
                CurTok, "expected ')' in closing extern function declaration");

          getNextToken(); // eat )

          if (CurTok.type == SC) {
            getNextToken(); // eat ";"
            auto Proto = std::make_unique<FunctionPrototypeAST>(
                IdTok, IdName, PrevTok.lexeme, std::move(P));
            return Proto;
          } else
            return LogErrorP(
                CurTok,
                "expected ;' in ending extern function declaration statement");
        } else
          return LogErrorP(CurTok,
                           "expected (' in extern function declaration");
      } else
        return LogErrorP(CurTok,
                         "expected identifier in extern function declaration");

    } else
      LogErrorP(CurTok, "expected 'void', 'int' or 'float' in extern function "
                        "declaration\n");
  }

  return nullptr;
}

// extern_list_prime ::= extern extern_list_prime
//                   |  ε                    (starter, now stores each extern)
static void ParseExternListPrime() {

  if (CurTok.type == EXTERN) { // FIRST(extern)
    if (auto Extern = ParseExtern()) {
      fprintf(stderr,
              "Parsed a top-level external function declaration -- 2\n");
      ProgramExterns.push_back(std::move(Extern));
    }
    ParseExternListPrime();
  } else if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
             CurTok.type == FLOAT_TOK ||
             CurTok.type == BOOL_TOK) { // FOLLOW(extern_list_prime)
    // expand by extern_list_prime ::= ε
  } else {
    LogError(CurTok, "expected 'extern' or 'void',  'int' ,  'float',  'bool'");
  }
}

// extern_list ::= extern extern_list_prime   (starter, now stores each extern)
static void ParseExternList() {
  auto Extern = ParseExtern();
  if (Extern) {
    fprintf(stderr, "Parsed a top-level external function declaration -- 1\n");
    ProgramExterns.push_back(std::move(Extern));
    if (CurTok.type == EXTERN)
      ParseExternListPrime();
  }
}

// program ::= extern_list decl_list
// The extern list is optional in practice (ParseExtern returns nothing when
// there is no "extern"), but at least one real declaration must follow.
static void parser() {
  if (CurTok.type == EOF_TOK)
    return;
  ParseExternList();
  if (CurTok.type == EOF_TOK) {
    LogError(CurTok, "expected at least one variable or function declaration");
    return;
  }
  ParseDeclList();
  if (CurTok.type == EOF_TOK)
    return;
}

// Entry point used by main(). It reads the first token, runs the parser and
// wraps everything it collected into the ProgramAST root (Part 1, Task 3).
std::unique_ptr<ProgramAST> ParseProgram() {
  getNextToken();
  parser();
  return std::make_unique<ProgramAST>(std::move(ProgramExterns),
                                      std::move(ProgramDecls));
}
