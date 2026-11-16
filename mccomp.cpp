#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h" // EliminateUnreachableBlocks (codegen)
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <string.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::sys;

FILE *pFile;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns one of these for known things.
enum TOKEN_TYPE {

  IDENT = -1,        // [a-zA-Z_][a-zA-Z_0-9]*
  ASSIGN = int('='), // '='

  // delimiters
  LBRA = int('{'),  // left brace
  RBRA = int('}'),  // right brace
  LPAR = int('('),  // left parenthesis
  LBOX = int('['),  // left bracket
  RBOX = int(']'),  // right bracket
  RPAR = int(')'),  // right parenthesis
  SC = int(';'),    // semicolon
  COMMA = int(','), // comma

  // types
  INT_TOK = -2,   // "int"
  VOID_TOK = -3,  // "void"
  FLOAT_TOK = -4, // "float"
  BOOL_TOK = -5,  // "bool"

  // keywords
  EXTERN = -6,  // "extern"
  IF = -7,      // "if"
  ELSE = -8,    // "else"
  WHILE = -9,   // "while"
  RETURN = -10, // "return"
  // TRUE   = -12,     // "true"
  // FALSE   = -13,     // "false"

  // literals
  INT_LIT = -14,   // [0-9]+
  FLOAT_LIT = -15, // [0-9]+.[0-9]+
  BOOL_LIT = -16,  // "true" or "false" key words

  // logical operators
  AND = -17, // "&&"
  OR = -18,  // "||"

  // operators
  PLUS = int('+'),    // addition or unary plus
  MINUS = int('-'),   // substraction or unary negative
  ASTERIX = int('*'), // multiplication
  DIV = int('/'),     // division
  MOD = int('%'),     // modular
  NOT = int('!'),     // unary negation

  // comparison operators
  EQ = -19,      // equal
  NE = -20,      // not equal
  LE = -21,      // less than or equal to
  LT = int('<'), // less than
  GE = -23,      // greater than or equal to
  GT = int('>'), // greater than

  // special tokens
  EOF_TOK = 0, // signal end of file

  // invalid
  INVALID = -100 // signal invalid token
};

// TOKEN class is used to keep track of information about a token
class TOKEN {
public:
  TOKEN() = default;
  int type = -100;
  std::string lexeme;
  int lineNo;
  int columnNo;
  const std::string getIdentifierStr() const;
  const int getIntVal() const;
  const float getFloatVal() const;
  const bool getBoolVal() const;
};

static std::string globalLexeme;
static int lineNo, columnNo;

const std::string TOKEN::getIdentifierStr() const {
  if (type != IDENT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getIdentifierStr called on non-IDENT token");
    exit(2);
  }
  return lexeme;
}

const int TOKEN::getIntVal() const {
  if (type != INT_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getIntVal called on non-INT_LIT token");
    exit(2);
  }
  return strtod(lexeme.c_str(), nullptr);
}

const float TOKEN::getFloatVal() const {
  if (type != FLOAT_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getFloatVal called on non-FLOAT_LIT token");
    exit(2);
  }
  return strtof(lexeme.c_str(), nullptr);
}

const bool TOKEN::getBoolVal() const {
  if (type != BOOL_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getBoolVal called on non-BOOL_LIT token");
    exit(2);
  }
  return (lexeme == "true");
}

static TOKEN returnTok(std::string lexVal, int tok_type) {
  TOKEN return_tok;
  return_tok.lexeme = lexVal;
  return_tok.type = tok_type;
  return_tok.lineNo = lineNo;
  return_tok.columnNo = columnNo - lexVal.length() - 1;
  return return_tok;
}

// Read file line by line -- or look for \n and if found add 1 to line number
// and reset column number to 0
/// gettok - Return the next token from standard input.
static TOKEN gettok() {

  static int LastChar = ' ';
  static int NextChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar)) {
    if (LastChar == '\n' || LastChar == '\r') {
      lineNo++;
      columnNo = 1;
    }
    LastChar = getc(pFile);
    columnNo++;
  }

  if (isalpha(LastChar) ||
      (LastChar == '_')) { // identifier: [a-zA-Z_][a-zA-Z_0-9]*
    globalLexeme = LastChar;
    columnNo++;

    while (isalnum((LastChar = getc(pFile))) || (LastChar == '_')) {
      globalLexeme += LastChar;
      columnNo++;
    }

    if (globalLexeme == "int")
      return returnTok("int", INT_TOK);
    if (globalLexeme == "bool")
      return returnTok("bool", BOOL_TOK);
    if (globalLexeme == "float")
      return returnTok("float", FLOAT_TOK);
    if (globalLexeme == "void")
      return returnTok("void", VOID_TOK);
    if (globalLexeme == "bool")
      return returnTok("bool", BOOL_TOK);
    if (globalLexeme == "extern")
      return returnTok("extern", EXTERN);
    if (globalLexeme == "if")
      return returnTok("if", IF);
    if (globalLexeme == "else")
      return returnTok("else", ELSE);
    if (globalLexeme == "while")
      return returnTok("while", WHILE);
    if (globalLexeme == "return")
      return returnTok("return", RETURN);
    if (globalLexeme == "true") {
      //   BoolVal = true;
      return returnTok("true", BOOL_LIT);
    }
    if (globalLexeme == "false") {
      //   BoolVal = false;
      return returnTok("false", BOOL_LIT);
    }
    return returnTok(globalLexeme.c_str(), IDENT);
  }

  if (LastChar == '=') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // EQ: ==
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("==", EQ);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("=", ASSIGN);
    }
  }

  if (LastChar == '{') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("{", LBRA);
  }
  if (LastChar == '}') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("}", RBRA);
  }
  if (LastChar == '(') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("(", LPAR);
  }
  if (LastChar == ')') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(")", RPAR);
  }
  if (LastChar == ';') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(";", SC);
  }
  if (LastChar == ',') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(",", COMMA);
  }

  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9]+.
    std::string NumStr;

    if (LastChar == '.') { // Floatingpoint Number: .[0-9]+
      do {
        NumStr += LastChar;
        LastChar = getc(pFile);
        columnNo++;
      } while (isdigit(LastChar));

      //   FloatVal = strtof(NumStr.c_str(), nullptr);
      return returnTok(NumStr, FLOAT_LIT);
    } else {
      do { // Start of Number: [0-9]+
        NumStr += LastChar;
        LastChar = getc(pFile);
        columnNo++;
      } while (isdigit(LastChar));

      if (LastChar == '.') { // Floatingpoint Number: [0-9]+.[0-9]+)
        do {
          NumStr += LastChar;
          LastChar = getc(pFile);
          columnNo++;
        } while (isdigit(LastChar));

        // FloatVal = strtof(NumStr.c_str(), nullptr);
        return returnTok(NumStr, FLOAT_LIT);
      } else { // Integer : [0-9]+
        // IntVal = strtod(NumStr.c_str(), nullptr);
        return returnTok(NumStr, INT_LIT);
      }
    }
  }

  if (LastChar == '&') {
    NextChar = getc(pFile);
    if (NextChar == '&') { // AND: &&
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("&&", AND);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("&", int('&'));
    }
  }

  if (LastChar == '|') {
    NextChar = getc(pFile);
    if (NextChar == '|') { // OR: ||
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("||", OR);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("|", int('|'));
    }
  }

  if (LastChar == '!') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // NE: !=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("!=", NE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("!", NOT);
      ;
    }
  }

  if (LastChar == '<') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // LE: <=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("<=", LE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("<", LT);
    }
  }

  if (LastChar == '>') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // GE: >=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok(">=", GE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok(">", GT);
    }
  }

  if (LastChar == '/') { // could be division or could be the start of a comment
    LastChar = getc(pFile);
    columnNo++;
    if (LastChar == '/') { // definitely a comment
      do {
        LastChar = getc(pFile);
        columnNo++;
      } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

      if (LastChar != EOF)
        return gettok();
    } else
      return returnTok("/", DIV);
  }

  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF) {
    columnNo++;
    return returnTok("0", EOF_TOK);
  }

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  std::string s(1, ThisChar);
  LastChar = getc(pFile);
  columnNo++;
  return returnTok(s, int(ThisChar));
}

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static TOKEN CurTok;
static std::deque<TOKEN> tok_buffer;

static TOKEN getNextToken() {

  if (tok_buffer.size() == 0)
    tok_buffer.push_back(gettok());

  TOKEN temp = tok_buffer.front();
  tok_buffer.pop_front();

  return CurTok = temp;
}

static void putBackToken(TOKEN tok) { tok_buffer.push_front(tok); }

//===----------------------------------------------------------------------===//
// AST printing helpers
//
// Each node's to_string() returns its own subtree as text, in pre-order:
// the node's header line first, then each child indented under it with
// clang-style tree connectors ("|-" for a child, "`-" for the last child).
//===----------------------------------------------------------------------===//

// Prefix the first line of s with `first` and every other line with `rest`.
static std::string prefixLines(const std::string &s, const std::string &first,
                               const std::string &rest) {
  std::string out;
  size_t start = 0;
  bool firstLine = true;
  while (true) {
    size_t nl = s.find('\n', start);
    std::string line;
    if (nl == std::string::npos)
      line = s.substr(start);
    else
      line = s.substr(start, nl - start);

    if (firstLine) {
      out += first;
      firstLine = false;
    } else {
      out += "\n" + rest;
    }
    out += line;

    if (nl == std::string::npos)
      break;
    start = nl + 1;
  }
  return out;
}

// Build a tree: header line followed by indented children.
static std::string treeNode(const std::string &header,
                            const std::vector<std::string> &children) {
  std::string out = header;
  for (size_t i = 0; i < children.size(); i++) {
    out += "\n";
    if (i + 1 == children.size())
      out += prefixLines(children[i], "`-", "  ");
    else
      out += prefixLines(children[i], "|-", "| ");
  }
  return out;
}

// Wrap a subtree under a label, e.g. "Then" or "Condition".
static std::string labelled(const std::string &label,
                            const std::string &child) {
  return treeNode(label, {child});
}

// Printable form of an operator token.
static std::string opToString(int op) {
  switch (op) {
  case PLUS:    return "+";
  case MINUS:   return "-";
  case ASTERIX: return "*";
  case DIV:     return "/";
  case MOD:     return "%";
  case NOT:     return "!";
  case LT:      return "<";
  case GT:      return ">";
  case LE:      return "<=";
  case GE:      return ">=";
  case EQ:      return "==";
  case NE:      return "!=";
  case AND:     return "&&";
  case OR:      return "||";
  default:      return "?";
  }
}

/// ASTnode - Base class for all AST nodes.
class ASTnode {

public:
  virtual ~ASTnode() {}
  virtual Value *codegen() { return nullptr; };
  virtual std::string to_string() const { return ""; };
};

// to_string() of a possibly-null child.
static std::string nodeToString(const ASTnode *node) {
  if (node == nullptr)
    return "<empty>";
  return node->to_string();
}

/// IntASTnode - Class for integer literals like 1, 2, 10,
class IntASTnode : public ASTnode {
  int Val;
  TOKEN Tok;
  std::string Name;

public:
  IntASTnode(TOKEN tok, int val) : Val(val), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  std::string to_string() const override {
    return "IntLiteral " + std::to_string(Val);
  }
};

/// BoolASTnode - Class for boolean literals true and false,
class BoolASTnode : public ASTnode {
  bool Bool;
  TOKEN Tok;

public:
  BoolASTnode(TOKEN tok, bool B) : Bool(B), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  std::string to_string() const override { return "BoolLiteral " + Tok.lexeme; }
};

/// FloatASTnode - Node class for floating point literals like "1.0".
class FloatASTnode : public ASTnode {
  double Val;
  TOKEN Tok;

public:
  FloatASTnode(TOKEN tok, double Val) : Val(Val), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  std::string to_string() const override {
    return "FloatLiteral " + Tok.lexeme;
  }
};

/// VariableASTnode - Class for referencing a variable (i.e. identifier), like
/// "a".
enum IDENT_TYPE { IDENTIFIER = 0 };
class VariableASTnode : public ASTnode {
protected:
  TOKEN Tok;
  std::string Name;
  IDENT_TYPE VarType;

public:
  VariableASTnode(TOKEN tok, const std::string &Name)
      : Tok(tok), Name(Name), VarType(IDENT_TYPE::IDENTIFIER) {}
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Tok.lexeme; }
  const IDENT_TYPE getVarType() const { return VarType; }
  const TOKEN &getTok() const { return Tok; } // position, for error messages
  std::string to_string() const override { return "VarRef '" + Name + "'"; }
};

/// ParamAST - Class for a parameter declaration
class ParamAST {
  std::string Name;
  std::string Type;

public:
  ParamAST(const std::string &name, const std::string &type)
      : Name(name), Type(type) {}
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  std::string to_string() const { return "Param " + Type + " '" + Name + "'"; }
};

/// DeclAST - Base class for declarations, variables and functions
class DeclAST : public ASTnode {

public:
  virtual ~DeclAST() {}
};

/// VarDeclAST - Class for a variable declaration
class VarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;

public:
  VarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type)
      : Var(std::move(var)), Type(type) {}
  const std::string &getType() const { return Type; }
  const std::string &getName() const { return Var->getName(); }
  std::string to_string() const override {
    return "VarDecl " + Type + " '" + Var->getName() + "'";
  }
};

/// GlobVarDeclAST - Class for a Global variable declaration
class GlobVarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;

public:
  GlobVarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type)
      : Var(std::move(var)), Type(type) {}
  const std::string &getType() const { return Type; }
  const std::string &getName() const { return Var->getName(); }
  std::string to_string() const override {
    return "GlobalVarDecl " + Type + " '" + Var->getName() + "'";
  }
};

/// FunctionPrototypeAST - Class for a function declaration's signature
class FunctionPrototypeAST {
  TOKEN Tok; // the function-name token: position for error messages
  std::string Name;
  std::string Type;
  std::vector<std::unique_ptr<ParamAST>> Params; // vector of parameters

public:
  FunctionPrototypeAST(TOKEN tok, const std::string &name,
                       const std::string &type,
                       std::vector<std::unique_ptr<ParamAST>> params)
      : Tok(tok), Name(name), Type(type), Params(std::move(params)) {}

  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  int getSize() const { return Params.size(); }
  const TOKEN &getTok() const { return Tok; }
  std::vector<std::unique_ptr<ParamAST>> &getParams() { return Params; }

  // header line, e.g. "FunctionDecl 'add' -> int"
  std::string header(const std::string &kind) const {
    return kind + " '" + Name + "' -> " + Type;
  }
  // one printed line per parameter
  std::vector<std::string> paramStrings() const {
    std::vector<std::string> kids;
    for (const auto &param : Params)
      kids.push_back(param->to_string());
    return kids;
  }
  std::string to_string(const std::string &kind) const {
    return treeNode(header(kind), paramStrings());
  }
};

/// ExprAST - Binary expression. Op is the TOKEN_TYPE of the operator
/// (int, because tokens such as EQ, AND, LE are negative values).
class ExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Node1;
  TOKEN Tok; // the operator token: position for error messages
  int Op;    // Tok.type, kept separately because it is used so often
  std::unique_ptr<ASTnode> Node2;

public:
  ExprAST(std::unique_ptr<ASTnode> node1, TOKEN opTok,
          std::unique_ptr<ASTnode> node2)
      : Node1(std::move(node1)), Tok(opTok), Op(opTok.type),
        Node2(std::move(node2)) {}
  std::string to_string() const override {
    return treeNode("BinaryOp '" + opToString(Op) + "'",
                    {nodeToString(Node1.get()), nodeToString(Node2.get())});
  }
};

/// UnaryExprAST - Unary minus or logical not, e.g. -x, !b
class UnaryExprAST : public ASTnode {
  TOKEN Tok; // the operator token: position for error messages
  int Op;
  std::unique_ptr<ASTnode> Operand;

public:
  UnaryExprAST(TOKEN opTok, std::unique_ptr<ASTnode> operand)
      : Tok(opTok), Op(opTok.type), Operand(std::move(operand)) {}
  std::string to_string() const override {
    return treeNode("UnaryOp '" + opToString(Op) + "'",
                    {nodeToString(Operand.get())});
  }
};

/// AssignAST - Assignment to a variable, e.g. x = expr
class AssignAST : public ASTnode {
  std::unique_ptr<VariableASTnode> Var;
  std::unique_ptr<ASTnode> Val;

public:
  AssignAST(std::unique_ptr<VariableASTnode> var, std::unique_ptr<ASTnode> val)
      : Var(std::move(var)), Val(std::move(val)) {}
  std::string to_string() const override {
    return treeNode("Assign", {nodeToString(Var.get()), nodeToString(Val.get())});
  }
};

/// BlockAST - Class for a block with declarations followed by statements
class BlockAST : public ASTnode {
  std::vector<std::unique_ptr<VarDeclAST>> LocalDecls; // vector of local decls
  std::vector<std::unique_ptr<ASTnode>> Stmts;         // vector of statements

public:
  BlockAST(std::vector<std::unique_ptr<VarDeclAST>> localDecls,
           std::vector<std::unique_ptr<ASTnode>> stmts)
      : LocalDecls(std::move(localDecls)), Stmts(std::move(stmts)) {}
  std::string to_string() const override {
    std::vector<std::string> kids;
    for (const auto &decl : LocalDecls)
      kids.push_back(nodeToString(decl.get()));
    for (const auto &stmt : Stmts)
      kids.push_back(nodeToString(stmt.get()));
    if (kids.empty())
      return "Block (empty)";
    return treeNode("Block", kids);
  }
};

/// FunctionDeclAST - This class represents a function definition itself.
class FunctionDeclAST : public DeclAST {
  std::unique_ptr<FunctionPrototypeAST> Proto;
  std::unique_ptr<ASTnode> Block;

public:
  FunctionDeclAST(std::unique_ptr<FunctionPrototypeAST> Proto,
                  std::unique_ptr<ASTnode> Block)
      : Proto(std::move(Proto)), Block(std::move(Block)) {}
  std::string to_string() const override {
    // parameters and body are siblings under the function node
    std::vector<std::string> kids = Proto->paramStrings();
    kids.push_back(nodeToString(Block.get()));
    return treeNode(Proto->header("FunctionDecl"), kids);
  }
};

/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ASTnode {
  TOKEN Tok; // the 'if' token: position for error messages
  std::unique_ptr<ASTnode> Cond, Then, Else;

public:
  IfExprAST(TOKEN tok, std::unique_ptr<ASTnode> Cond,
            std::unique_ptr<ASTnode> Then, std::unique_ptr<ASTnode> Else)
      : Tok(tok), Cond(std::move(Cond)), Then(std::move(Then)),
        Else(std::move(Else)) {}
  std::string to_string() const override {
    std::vector<std::string> kids;
    kids.push_back(labelled("Condition", nodeToString(Cond.get())));
    kids.push_back(labelled("Then", nodeToString(Then.get())));
    if (Else)
      kids.push_back(labelled("Else", nodeToString(Else.get())));
    return treeNode("If", kids);
  }
};

/// WhileExprAST - Expression class for while.
class WhileExprAST : public ASTnode {
  TOKEN Tok; // the 'while' token: position for error messages
  std::unique_ptr<ASTnode> Cond, Body; // Body is null for "while (c) ;"

public:
  WhileExprAST(TOKEN tok, std::unique_ptr<ASTnode> cond,
               std::unique_ptr<ASTnode> body)
      : Tok(tok), Cond(std::move(cond)), Body(std::move(body)) {}
  std::string to_string() const override {
    return treeNode("While", {labelled("Condition", nodeToString(Cond.get())),
                              labelled("Body", nodeToString(Body.get()))});
  }
};

/// ReturnAST - Class for a return value
class ReturnAST : public ASTnode {
  TOKEN Tok; // the 'return' token: position for error messages
  std::unique_ptr<ASTnode> Val;

public:
  ReturnAST(TOKEN tok, std::unique_ptr<ASTnode> value)
      : Tok(tok), Val(std::move(value)) {}
  std::string to_string() const override {
    if (!Val)
      return "Return (void)";
    return treeNode("Return", {nodeToString(Val.get())});
  }
};

/// ArgsAST - Class for a function argumetn in a function call
class ArgsAST : public ASTnode {
  TOKEN Tok; // the function-name token: position for error messages
  std::string Callee;
  std::vector<std::unique_ptr<ASTnode>> ArgsList;

public:
  ArgsAST(TOKEN tok, const std::string &Callee,
          std::vector<std::unique_ptr<ASTnode>> list)
      : Tok(tok), Callee(Callee), ArgsList(std::move(list)) {}
  std::string to_string() const override {
    std::vector<std::string> kids;
    for (const auto &arg : ArgsList)
      kids.push_back(nodeToString(arg.get()));
    if (kids.empty())
      return "Call '" + Callee + "' (no arguments)";
    return treeNode("Call '" + Callee + "'", kids);
  }
};

/// ProgramAST - Root of the AST: extern declarations followed by top-level
/// variable and function declarations.
class ProgramAST : public ASTnode {
  std::vector<std::unique_ptr<FunctionPrototypeAST>> Externs;
  std::vector<std::unique_ptr<ASTnode>> Decls;

public:
  ProgramAST(std::vector<std::unique_ptr<FunctionPrototypeAST>> externs,
             std::vector<std::unique_ptr<ASTnode>> decls)
      : Externs(std::move(externs)), Decls(std::move(decls)) {}
  std::string to_string() const override {
    std::vector<std::string> kids;
    for (const auto &ext : Externs)
      kids.push_back(ext->to_string("ExternDecl"));
    for (const auto &decl : Decls)
      kids.push_back(nodeToString(decl.get()));
    return treeNode("Program", kids);
  }
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const ASTnode &ast) {
  os << ast.to_string();
  return os;
}

// Top-level nodes collected by the parser, assembled into the root at the end.
static std::vector<std::unique_ptr<FunctionPrototypeAST>> ProgramExterns;
static std::vector<std::unique_ptr<ASTnode>> ProgramDecls;

/// LogError* - These are little helper function for error handling.
std::unique_ptr<ASTnode> LogError(TOKEN tok, const char *Str) {
  // "Syntax error": the spec asks a diagnostic to name its kind
  fprintf(stderr, "%d:%d Syntax error: %s\n", tok.lineNo, tok.columnNo, Str);
  exit(2);
  return nullptr;
}

std::unique_ptr<FunctionPrototypeAST> LogErrorP(TOKEN tok, const char *Str) {
  LogError(tok, Str);
  exit(2);
  return nullptr;
}

std::unique_ptr<ASTnode> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
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

// element ::= FLOAT_LIT
static std::unique_ptr<ASTnode> ParseFloatNumberExpr() {
  auto Result = std::make_unique<FloatASTnode>(CurTok, CurTok.getFloatVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// element ::= INT_LIT
static std::unique_ptr<ASTnode> ParseIntNumberExpr() {
  auto Result = std::make_unique<IntASTnode>(CurTok, CurTok.getIntVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// element ::= BOOL_LIT
static std::unique_ptr<ASTnode> ParseBoolExpr() {
  auto Result = std::make_unique<BoolASTnode>(CurTok, CurTok.getBoolVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// param_list_prime ::= "," param param_list_prime
//                   |  ε
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
    // do nothing
  } else {
    LogError(CurTok, "expected ',' or ')' in list of parameter declarations");
  }

  return param_list;
}

// param ::= var_type IDENT
static std::unique_ptr<ParamAST> ParseParam() {

  if (CurTok.type != INT_TOK && CurTok.type != FLOAT_TOK &&
      CurTok.type != BOOL_TOK) {
    LogError(CurTok, "expected 'int', 'float' or 'bool' in parameter declaration");
    return nullptr;
  }
  std::string Type = CurTok.lexeme; // keep track of the type of the param
  getNextToken();                   // eat the type token

  if (CurTok.type != IDENT) {
    LogError(CurTok, "expected identifier in parameter declaration");
    return nullptr;
  }
  std::string Name = CurTok.getIdentifierStr();
  getNextToken(); // eat "IDENT"

  return std::make_unique<ParamAST>(Name, Type);
}

// param_list ::= param param_list_prime
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
//         |  ε
static std::vector<std::unique_ptr<ParamAST>> ParseParams() {
  std::vector<std::unique_ptr<ParamAST>> param_list;

  std::string Type;
  std::string Name = "";

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(param_list)

    auto list = ParseParamList();
    for (unsigned i = 0; i < list.size(); i++) {
      param_list.push_back(std::move(list.at(i)));
    }

  } else if (CurTok.type == VOID_TOK) { // FIRST("void")
    // void
    // check that the next token is a )
    getNextToken(); // eat 'void'
    if (CurTok.type != RPAR) {
      LogError(CurTok, "expected ')', after 'void' in \
       end of function declaration");
    }
  } else if (CurTok.type == RPAR) { // FOLLOW(params)
    // expand by params ::= ε
    // do nothing
  } else {
    LogError(
        CurTok,
        "expected 'int', 'bool' or 'float' in function declaration or ') in \
       end of function declaration");
  }

  return param_list;
}

//===----------------------------------------------------------------------===//
// Expressions
//
// Each precedence level X is parsed as   X ::= Next X'
//                                        X' ::= op Next X' | ε
// The left operand built so far is passed down into X' (inherited
// attribute) so that operators are left-associative: a - b - c = (a - b) - c.
//===----------------------------------------------------------------------===//

// FIRST(expr) = { "-", "!", "(", IDENT, INT_LIT, FLOAT_LIT, BOOL_LIT }
static bool inFirstExpr(int t) {
  return t == MINUS || t == NOT || t == LPAR || t == IDENT || t == INT_LIT ||
         t == FLOAT_LIT || t == BOOL_LIT;
}

// FOLLOW sets of the X' nonterminals. Each level's FOLLOW set is the FOLLOW
// set of the level below it plus that level's operators.
// FOLLOW(rval') = { ")", ",", ";" }
static bool inFollowRvalPrime(int t) {
  return t == RPAR || t == COMMA || t == SC;
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

// ident_tail ::= "(" args ")" | ε
static std::unique_ptr<ASTnode> ParseIdentTail(TOKEN identTok) {
  std::string Name = identTok.getIdentifierStr();

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
static std::unique_ptr<ASTnode> ParseUnaryExpr() {
  if (CurTok.type == MINUS || CurTok.type == NOT) {
    TOKEN op = CurTok; // the whole token, not just its type: keeps the position
    getNextToken(); // eat operator
    auto operand = ParseUnaryExpr();
    return std::make_unique<UnaryExprAST>(op, std::move(operand));
  }
  return ParsePrimary();
}

// mul_expr' ::= ("*" | "/" | "%") unary_expr mul_expr' | ε
static std::unique_ptr<ASTnode> ParseMulExprPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == ASTERIX || CurTok.type == DIV || CurTok.type == MOD) {
    TOKEN op = CurTok; // the whole token, not just its type: keeps the position
    getNextToken(); // eat operator
    auto rhs = ParseUnaryExpr();
    auto node = std::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
    return ParseMulExprPrime(std::move(node));
  }
  if (inFollowMulPrime(CurTok.type))
    return lhs;
  // FOLLOW(mul_expr') contains every operator of the lower levels, so a bad
  // token after an operand is always detected here first.
  std::string msg = "expected an operator, ';', ')' or ',' after expression, "
                    "but found '" + CurTok.lexeme + "'";
  return LogError(CurTok, msg.c_str());
}

// mul_expr ::= unary_expr mul_expr'
static std::unique_ptr<ASTnode> ParseMulExpr() {
  return ParseMulExprPrime(ParseUnaryExpr());
}

// add_expr' ::= ("+" | "-") mul_expr add_expr' | ε
static std::unique_ptr<ASTnode> ParseAddExprPrime(std::unique_ptr<ASTnode> lhs) {
  if (CurTok.type == PLUS || CurTok.type == MINUS) {
    TOKEN op = CurTok; // the whole token, not just its type: keeps the position
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
    TOKEN op = CurTok; // the whole token, not just its type: keeps the position
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
    TOKEN op = CurTok; // the whole token, not just its type: keeps the position
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
    TOKEN op = CurTok; // save the operator before it is consumed
    getNextToken();    // eat "&&"
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
    TOKEN op = CurTok; // save the operator before it is consumed
    getNextToken();    // eat "||"
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

// expr ::= IDENT "=" expr
//      |  rval
// Both alternatives can start with IDENT, so one extra token of lookahead
// is used (LL(2)): assignment only if IDENT is followed by "=".
static std::unique_ptr<ASTnode> ParseExper() {
  if (CurTok.type == IDENT) {
    TOKEN identTok = CurTok;
    TOKEN next = getNextToken(); // peek at the token after IDENT
    if (next.type == ASSIGN) {
      getNextToken(); // eat "="
      auto var =
          std::make_unique<VariableASTnode>(identTok, identTok.getIdentifierStr());
      auto val = ParseExper(); // right-associative: a = b = c
      return std::make_unique<AssignAST>(std::move(var), std::move(val));
    }
    putBackToken(next); // not an assignment: undo the peek
    CurTok = identTok;
  }
  if (!inFirstExpr(CurTok.type))
    return LogError(CurTok, "expected an expression");
  return ParseRval();
}

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
static std::unique_ptr<ASTnode> ParseElseStmt() {

  if (CurTok.type == ELSE) { // FIRST(else_stmt)
    // expand by else_stmt  ::= "else" "{" stmt "}"
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
    // expand by else_stmt  ::= ε
    // return an empty statement
    return nullptr;
  } else
    LogError(CurTok, "expected 'else' or the start of the next statement or '}'");

  return nullptr;
}

// if_stmt ::= "if" "(" expr ")" block else_stmt
static std::unique_ptr<ASTnode> ParseIfStmt() {
  TOKEN ifTok = CurTok; // position of 'if', for error messages
  getNextToken();       // eat the if.
  if (CurTok.type == LPAR) {
    getNextToken(); // eat (
    // condition.
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
//             |  "return" expr ";"
static std::unique_ptr<ASTnode> ParseReturnStmt() {
  TOKEN retTok = CurTok; // position of 'return', for error messages
  getNextToken();        // eat the return
  if (CurTok.type == SC) {
    getNextToken(); // eat the ;
    // return a null value
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
static std::unique_ptr<ASTnode> ParseWhileStmt() {
  TOKEN whileTok = CurTok; // position of 'while', for error messages
  getNextToken();          // eat the while.
  if (CurTok.type == LPAR) {
    getNextToken(); // eat (
    // condition.
    auto Cond = ParseExper();
    if (!Cond)
      return nullptr;
    if (CurTok.type != RPAR)
      return LogError(CurTok, "expected )");
    getNextToken(); // eat )

    // Body is null for the empty statement ";" (as in "while (c) ;").
    // That is still a loop, so the node is built either way. (Returning
    // nullptr here used to drop the whole loop from the AST.)
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
//      |  return_stmt
static std::unique_ptr<ASTnode> ParseStmt() {

  if (inFirstExpr(CurTok.type) || CurTok.type == SC) { // FIRST(expr_stmt)
    // expand by stmt ::= expr_stmt
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
  }
  // else if(CurTok.type == RBRA) { // FOLLOW(stmt_list_prime)
  //  expand by stmt_list_prime ::= ε
  //  do nothing
  //}
  else { // syntax error
    return LogError(CurTok, "expected a statement (expression, ';', '{', "
                            "'if', 'while' or 'return')");
  }
  return nullptr;
}

// stmt_list ::= stmt stmt_list | ε
// (a block may contain no statements, so this is the same as stmt_list_prime)
static std::vector<std::unique_ptr<ASTnode>> ParseStmtList() {
  return ParseStmtListPrime();
}

// stmt_list_prime ::= stmt stmt_list_prime
//                  |  ε
static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime() {
  std::vector<std::unique_ptr<ASTnode>> stmt_list; // vector of statements
  if (inFirstExpr(CurTok.type) || CurTok.type == SC || CurTok.type == LBRA ||
      CurTok.type == WHILE || CurTok.type == IF ||
      CurTok.type == RETURN) { // FIRST(stmt)
    // expand by stmt_list ::= stmt stmt_list_prime
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
    // do nothing
  } else {
    LogError(CurTok, "expected a statement or '}'");
  }
  return stmt_list; // note stmt_list can be empty as we can have empty blocks,
                    // etc.
}

// local_decls_prime ::= local_decl local_decls_prime
//                    |  ε
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDeclsPrime() {
  std::vector<std::unique_ptr<VarDeclAST>>
      local_decls_prime; // vector of local decls

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
    // expand by local_decls_prime ::=  ε
    // do nothing;
  } else {
    LogError(CurTok, "expected a local variable declaration, a statement "
                     "or '}'");
  }

  return local_decls_prime;
}

// local_decl ::= var_type IDENT ";"
// var_type ::= "int"
//           |  "float"
//           |  "bool"
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
      Name = CurTok.getIdentifierStr(); // save the identifier name
      auto ident = std::make_unique<VariableASTnode>(CurTok, Name);
      local_decl = std::make_unique<VarDeclAST>(std::move(ident), Type);

      getNextToken(); // eat 'IDENT'
      if (CurTok.type != SC) {
        LogError(CurTok, "Expected ';' to end local variable declaration");
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
// (same as local_decls_prime; the old version had ',' instead of ';' and no
// '}' in its FOLLOW set, so blocks with no statements were rejected)
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDecls() {
  return ParseLocalDeclsPrime();
}

// parse block
// block ::= "{" local_decls stmt_list "}"
static std::unique_ptr<ASTnode> ParseBlock() {
  std::vector<std::unique_ptr<VarDeclAST>> local_decls; // vector of local decls
  std::vector<std::unique_ptr<ASTnode>> stmt_list;      // vector of statements

  getNextToken(); // eat '{'

  local_decls = ParseLocalDecls();
  fprintf(stderr, "Parsed a set of local variable declaration\n");
  stmt_list = ParseStmtList();
  fprintf(stderr, "Parsed a list of statements\n");
  if (CurTok.type == RBRA)
    getNextToken(); // eat '}'
  else {            // syntax error
    LogError(CurTok, "expected '}' , close body of block");
    return nullptr;
  }

  return std::make_unique<BlockAST>(std::move(local_decls),
                                    std::move(stmt_list));
}

// decl ::= var_decl
//       |  fun_decl
static std::unique_ptr<ASTnode> ParseDecl() {
  std::string IdName;
  TOKEN IdTok; // the identifier token: position for error messages
  std::vector<std::unique_ptr<ParamAST>> param_list;

  TOKEN PrevTok = CurTok; // to keep track of the type token

  if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
      CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {
    getNextToken(); // eat the VOID_TOK, INT_TOK, BOOL_TOK or FLOAT_TOK

    if (CurTok.type == IDENT) {
      IdName = CurTok.getIdentifierStr(); // save the identifier name
      IdTok = CurTok;                      // and where it is
      auto ident = std::make_unique<VariableASTnode>(CurTok, IdName);
      getNextToken(); // eat the IDENT
      if (CurTok.type ==
          SC) {         // found ';' then this is a global variable declaration.
        getNextToken(); // eat ;
        fprintf(stderr, "Parsed a variable declaration\n");

        if (PrevTok.type != VOID_TOK)
          return std::make_unique<GlobVarDeclAST>(std::move(ident),
                                                  PrevTok.lexeme);
        else
          return LogError(PrevTok,
                          "Cannot have variable declaration with type 'void'");
      } else if (CurTok.type ==
                 LPAR) { // found '(' then this is a function declaration.
        getNextToken();  // eat (

        auto P =
            ParseParams(); // parse the parameters, returns a vector of params
        // if (P.size() == 0) return nullptr;
        fprintf(stderr, "Parsed parameter list for function\n");

        if (CurTok.type != RPAR) // syntax error
          return LogError(CurTok, "expected ')' in function declaration");

        getNextToken();          // eat )
        if (CurTok.type != LBRA) // syntax error
          return LogError(
              CurTok, "expected '{' in function declaration, function body");

        auto B = ParseBlock(); // parse the function body
        if (!B)
          return nullptr;
        else
          fprintf(stderr, "Parsed block of statements in function\n");

        // now create a Function prototype
        // create a Function body
        // put these to together
        // and return a std::unique_ptr<FunctionDeclAST>
        fprintf(stderr, "Parsed a function declaration\n");

        auto Proto = std::make_unique<FunctionPrototypeAST>(
            IdTok, IdName, PrevTok.lexeme, std::move(P));
        return std::make_unique<FunctionDeclAST>(std::move(Proto),
                                                 std::move(B));
      } else
        return LogError(CurTok, "expected ';' or ('");
    } else
      return LogError(CurTok, "expected an identifier");

  } else
    LogError(CurTok,
             "expected 'void', 'int' or 'float' or EOF token"); // syntax error

  return nullptr;
}

// decl_list_prime ::= decl decl_list_prime
//                  |  ε
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
    // do nothing
  } else { // syntax error
    LogError(CurTok, "expected 'void', 'int', 'bool' or 'float' or EOF token");
  }
}

// decl_list ::= decl decl_list_prime
static void ParseDeclList() {
  auto decl = ParseDecl();
  if (decl) {
    fprintf(stderr, "Parsed a top-level variable or function declaration\n");
    ProgramDecls.push_back(std::move(decl));
    ParseDeclListPrime();
  }
}

// extern ::= "extern" type_spec IDENT "(" params ")" ";"
static std::unique_ptr<FunctionPrototypeAST> ParseExtern() {
  std::string IdName;
  TOKEN IdTok; // the identifier token: position for error messages
  TOKEN PrevTok;

  if (CurTok.type == EXTERN) {
    getNextToken(); // eat the EXTERN

    if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
        CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {

      PrevTok = CurTok; // to keep track of the type token
      getNextToken();   // eat the VOID_TOK, INT_TOK, BOOL_TOK or FLOAT_TOK

      if (CurTok.type == IDENT) {
        IdName = CurTok.getIdentifierStr(); // save the identifier name
        IdTok = CurTok;                      // and where it is
        auto ident = std::make_unique<VariableASTnode>(CurTok, IdName);
        getNextToken(); // eat the IDENT

        if (CurTok.type ==
            LPAR) {       // found '(' - this is an extern function declaration.
          getNextToken(); // eat (

          // parse the parameters (may be empty for "()" or "(void)")
          auto P = ParseParams();
          fprintf(stderr, "Parsed parameter list for external function\n");

          if (CurTok.type != RPAR) // syntax error
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
                        "declaration\n"); // syntax error
  }

  return nullptr;
}

// extern_list_prime ::= extern extern_list_prime
//                   |  ε
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
    // expand by decl_list_prime ::= ε
    // do nothing
  } else { // syntax error
    LogError(CurTok, "expected 'extern' or 'void',  'int' ,  'float',  'bool'");
  }
}

// extern_list ::= extern extern_list_prime
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
static void parser() {
  if (CurTok.type == EOF_TOK)
    return;
  ParseExternList();
  if (CurTok.type == EOF_TOK) { // decl_list must contain at least one decl
    LogError(CurTok, "expected at least one variable or function declaration");
    return;
  }
  ParseDeclList();
  if (CurTok.type == EOF_TOK)
    return;
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;

//===----------------------------------------------------------------------===//
// AST Printer
//===----------------------------------------------------------------------===//

// void IntASTnode::display(int tabs) {
//   printf("%s\n",getType().c_str());
// }

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
  if (argc == 2) {
    pFile = fopen(argv[1], "r");
    if (pFile == NULL) {
      perror("Error opening file");
      return 1; // stop here: reading from a null FILE* crashes the lexer
    }
  } else {
    std::cout << "Usage: ./code InputFile\n";
    return 1;
  }

  // initialize line number and column numbers to zero
  lineNo = 1;
  columnNo = 1;

  // get the first token
  // (the lexer-only loop that printed every token was removed: it consumed
  //  the whole input before the parser could see it)
  getNextToken();

  // Make the module, which holds all the code.
  TheModule = std::make_unique<Module>("mini-c", TheContext);

  // Run the parser now. Any lexical or syntax error exits with status 2
  // from LogError, so reaching the next line means the parse succeeded.
  parser();
  fprintf(stderr, "Parsing Finished: program is syntactically valid\n");

  // Task 3: assemble the root of the AST and print it
  auto RootASTnode = std::make_unique<ProgramAST>(std::move(ProgramExterns),
                                                  std::move(ProgramDecls));
  llvm::outs() << "********************* AST (begin) *********************\n";
  llvm::outs() << *RootASTnode << "\n";
  llvm::outs() << "********************* AST (end) ***********************\n";
  llvm::outs().flush();

  printf(
      "********************* FINAL IR (begin) ****************************\n");
  // Print out all of the generated code into a file called output.ll
  // printf("%s\n", argv[1]);
  auto Filename = "output.ll";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message();
    return 1;
  }
  // TheModule->print(errs(), nullptr); // print IR to terminal
  TheModule->print(dest, nullptr);
  printf(
      "********************* FINAL IR (end) ******************************\n");

  fclose(pFile); // close the file that contains the code that was parsed
  return 0;
}
