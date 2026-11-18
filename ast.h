// ast.h - the abstract syntax tree.
//
// The parser builds a tree of these nodes (Part 1, Task 2), the tree can
// print itself (Part 1, Task 3), and each node turns itself into LLVM IR
// through codegen() (Part 2). The codegen() bodies are in codegen.cpp, and
// the printing helpers are in ast.cpp.

#ifndef MCCOMP_AST_H
#define MCCOMP_AST_H

#include "lexer.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <string>
#include <vector>

//===----------------------------------------------------------------------===//
// Printing helpers (Part 1, Task 3)
//
// Every node prints its own subtree as a string, header line first and then
// its children indented below it, using the same "|-" and "`-" connectors as
// clang -ast-dump. Building strings rather than printing directly means a
// parent can indent a child's whole subtree without the child knowing how
// deep it sits.
//===----------------------------------------------------------------------===//

std::string treeNode(const std::string &header,
                     const std::vector<std::string> &children);
std::string labelled(const std::string &label, const std::string &child);
std::string opToString(int op);

/// ASTnode - Base class for all AST nodes.
class ASTnode {

public:
  virtual ~ASTnode() {}
  virtual llvm::Value *codegen() { return nullptr; };
  virtual std::string to_string() const { return ""; };

  // Step 10. Only a variable or an array element may stand on the left of
  // "=". The parser asks the node itself, because the compiler is built
  // without RTTI, so dynamic_cast is not available.
  virtual bool isAssignable() const { return false; }
  // Step 10. The name to use in assignment error messages ("x", "a[...]").
  virtual std::string targetName() const { return ""; }
  // Step 12. The address an assignment stores to, with the type stored
  // there in *Ty. Only assignable nodes override it.
  virtual llvm::Value *codegenAddress(llvm::Type **Ty) { return nullptr; }
  // Step 13. The address of a whole array or a row, for passing it to a
  // function, with its array type in *ArrTy. nullptr means "not an array".
  virtual llvm::Value *codegenArrayRef(llvm::Type **ArrTy) { return nullptr; }
};

// Step 10. Prints array sizes after a name, e.g. "[3][4]". A 0 stands for an
// unsized first dimension of a parameter and prints as "[]".
std::string dimsToString(const std::vector<int> &dims);

std::string nodeToString(const ASTnode *node);

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const ASTnode &ast);

//===----------------------------------------------------------------------===//
// Node classes
//
// In Part 2 Step 0 every node that can be the subject of a semantic error got
// a TOKEN field. The token remembers the line and column it came from, so a
// type error can point at the exact operator, call or 'return' that caused
// it instead of just saying "something is wrong".
//===----------------------------------------------------------------------===//

/// IntASTnode - Class for integer literals like 1, 2, 10,
class IntASTnode : public ASTnode {
  int Val;
  TOKEN Tok;
  std::string Name;

public:
  IntASTnode(TOKEN tok, int val) : Val(val), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  llvm::Value *codegen() override;
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
  llvm::Value *codegen() override;
  std::string to_string() const override { return "BoolLiteral " + Tok.lexeme; }
};

/// FloatASTnode - Node class for floating point literals like "1.0".
class FloatASTnode : public ASTnode {
  double Val;
  TOKEN Tok;

public:
  FloatASTnode(TOKEN tok, double Val) : Val(Val), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  llvm::Value *codegen() override;
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
  // Step 0: declarations and assignments reuse this token to report errors
  // such as "undeclared variable" at the variable's own position.
  const TOKEN &getTok() const { return Tok; }
  llvm::Value *codegen() override; // Step 3: read the variable
  bool isAssignable() const override { return true; }      // Step 10
  std::string targetName() const override { return Name; } // Step 10
  llvm::Value *codegenAddress(llvm::Type **Ty) override;   // Step 12
  std::string to_string() const override { return "VarRef '" + Name + "'"; }
};

/// ParamAST - Class for a parameter declaration
/// (Step 13: Dims is empty for a scalar, or holds the sizes of an array
/// parameter such as "int a[10]"; a first size of 0 means "int a[]".)
class ParamAST {
  std::string Name;
  std::string Type;
  std::vector<int> Dims;

public:
  ParamAST(const std::string &name, const std::string &type,
           std::vector<int> dims = {})
      : Name(name), Type(type), Dims(std::move(dims)) {}
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  const std::vector<int> &getDims() const { return Dims; }
  std::string to_string() const {
    return "Param " + Type + " '" + Name + "'" + dimsToString(Dims);
  }
};

/// DeclAST - Base class for declarations, variables and functions
class DeclAST : public ASTnode {

public:
  virtual ~DeclAST() {}
};

/// VarDeclAST - Class for a variable declaration
/// (Step 10: Dims holds the sizes of an array, and is empty for a scalar.)
class VarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;
  std::vector<int> Dims;

public:
  VarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type,
             std::vector<int> dims = {})
      : Var(std::move(var)), Type(type), Dims(std::move(dims)) {}
  const std::string &getType() const { return Type; }
  const std::string &getName() const { return Var->getName(); }
  llvm::Value *codegen() override; // Step 3: declare a local (Step 11: or an array)
  std::string to_string() const override {
    return "VarDecl " + Type + " '" + Var->getName() + "'" + dimsToString(Dims);
  }
};

/// GlobVarDeclAST - Class for a Global variable declaration
class GlobVarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;
  std::vector<int> Dims; // Step 10: array sizes, empty for a scalar

public:
  GlobVarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type,
                 std::vector<int> dims = {})
      : Var(std::move(var)), Type(type), Dims(std::move(dims)) {}
  const std::string &getType() const { return Type; }
  const std::string &getName() const { return Var->getName(); }
  llvm::Value *codegen() override; // Step 8: create the global (Step 11: or an array)
  std::string to_string() const override {
    return "GlobalVarDecl " + Type + " '" + Var->getName() + "'" +
           dimsToString(Dims);
  }
};

/// FunctionPrototypeAST - Class for a function declaration's signature.
/// It is not an ASTnode because it is never a statement or an expression on
/// its own; it is either an extern or the first half of a FunctionDeclAST.
/// That is also why its codegen() returns a Function* and has no override.
class FunctionPrototypeAST {
  TOKEN Tok; // Step 0: the function name, so "conflicting declarations" can point at it
  std::string Name;
  std::string Type;
  std::vector<std::unique_ptr<ParamAST>> Params;

public:
  FunctionPrototypeAST(TOKEN tok, const std::string &name,
                       const std::string &type,
                       std::vector<std::unique_ptr<ParamAST>> params)
      : Tok(tok), Name(name), Type(type), Params(std::move(params)) {}

  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  int getSize() const { return Params.size(); }
  const TOKEN &getTok() const { return Tok; }
  llvm::Function *codegen();
  std::vector<std::unique_ptr<ParamAST>> &getParams() { return Params; }

  // A prototype is printed in two different ways: as an "ExternDecl" on its
  // own, or as the header of a "FunctionDecl" whose body sits next to the
  // parameters. So the header and the parameter lines are separate helpers.
  std::string header(const std::string &kind) const {
    return kind + " '" + Name + "' -> " + Type;
  }
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

/// ExprAST - Binary expression. Op is the TOKEN_TYPE of the operator; it is an
/// int rather than a char because tokens such as EQ, AND and LE are negative.
class ExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Node1;
  TOKEN Tok; // Step 0: the operator token, for type errors like "bool % int"
  int Op;    // a copy of Tok.type, because codegen switches on it constantly
  std::unique_ptr<ASTnode> Node2;

public:
  ExprAST(std::unique_ptr<ASTnode> node1, TOKEN opTok,
          std::unique_ptr<ASTnode> node2)
      : Node1(std::move(node1)), Tok(opTok), Op(opTok.type),
        Node2(std::move(node2)) {}
  llvm::Value *codegen() override; // Step 4: arithmetic and comparisons
  llvm::Value *codegenLogical();   // Step 7: short-circuit && and ||
  std::string to_string() const override {
    return treeNode("BinaryOp '" + opToString(Op) + "'",
                    {nodeToString(Node1.get()), nodeToString(Node2.get())});
  }
};

/// UnaryExprAST - Unary minus or logical not, e.g. -x, !b
/// (a node we added in Part 1; the starter code had no class for these).
class UnaryExprAST : public ASTnode {
  TOKEN Tok;
  int Op;
  std::unique_ptr<ASTnode> Operand;

public:
  UnaryExprAST(TOKEN opTok, std::unique_ptr<ASTnode> operand)
      : Tok(opTok), Op(opTok.type), Operand(std::move(operand)) {}
  llvm::Value *codegen() override; // Step 4: -x and !x
  std::string to_string() const override {
    return treeNode("UnaryOp '" + opToString(Op) + "'",
                    {nodeToString(Operand.get())});
  }
};

/// AssignAST - Assignment "target = expr". In Part 1 the target was always a
/// plain variable. Step 10 widens it to any assignable node, so that array
/// elements like a[i][j] can be assigned too; Tok is the "=" token.
class AssignAST : public ASTnode {
  TOKEN Tok;
  std::unique_ptr<ASTnode> Target;
  std::unique_ptr<ASTnode> Val;

public:
  AssignAST(TOKEN eqTok, std::unique_ptr<ASTnode> target,
            std::unique_ptr<ASTnode> val)
      : Tok(eqTok), Target(std::move(target)), Val(std::move(val)) {}
  llvm::Value *codegen() override; // Step 3: store into the variable (Step 12: or element)
  std::string to_string() const override {
    return treeNode("Assign",
                    {nodeToString(Target.get()), nodeToString(Val.get())});
  }
};

/// ArrayAccessAST - An array element or row, e.g. a[i], m[i][j * 2]
/// (Step 10). Every index is an expression; how many there are against how
/// many dimensions the array has is checked in codegen (Step 12).
class ArrayAccessAST : public ASTnode {
  TOKEN Tok;
  std::string Name;
  std::vector<std::unique_ptr<ASTnode>> Indices;

public:
  ArrayAccessAST(TOKEN tok, const std::string &name,
                 std::vector<std::unique_ptr<ASTnode>> indices)
      : Tok(tok), Name(name), Indices(std::move(indices)) {}
  llvm::Value *codegen() override;                           // Step 12
  bool isAssignable() const override { return true; }        // Step 10
  std::string targetName() const override { return Name + "[...]"; }
  llvm::Value *codegenAddress(llvm::Type **Ty) override;     // Step 12
  std::string to_string() const override {
    std::vector<std::string> kids;
    for (const auto &index : Indices)
      kids.push_back(labelled("Index", nodeToString(index.get())));
    return treeNode("ArrayAccess '" + Name + "'", kids);
  }
};

/// BlockAST - Class for a block with declarations followed by statements
class BlockAST : public ASTnode {
  std::vector<std::unique_ptr<VarDeclAST>> LocalDecls;
  std::vector<std::unique_ptr<ASTnode>> Stmts;

public:
  BlockAST(std::vector<std::unique_ptr<VarDeclAST>> localDecls,
           std::vector<std::unique_ptr<ASTnode>> stmts)
      : LocalDecls(std::move(localDecls)), Stmts(std::move(stmts)) {}
  // Step 2 gives a block two entry points; codegen.cpp explains why.
  llvm::Value *codegen() override;
  llvm::Value *codegenInScope();
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
  llvm::Value *codegen() override;
  std::string to_string() const override {
    // The parameters and the body are printed as siblings under the function.
    std::vector<std::string> kids = Proto->paramStrings();
    kids.push_back(nodeToString(Block.get()));
    return treeNode(Proto->header("FunctionDecl"), kids);
  }
};

/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ASTnode {
  TOKEN Tok;
  std::unique_ptr<ASTnode> Cond, Then, Else;

public:
  IfExprAST(TOKEN tok, std::unique_ptr<ASTnode> Cond,
            std::unique_ptr<ASTnode> Then, std::unique_ptr<ASTnode> Else)
      : Tok(tok), Cond(std::move(Cond)), Then(std::move(Then)),
        Else(std::move(Else)) {}
  llvm::Value *codegen() override; // Step 6: branch on the condition
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
  TOKEN Tok;
  std::unique_ptr<ASTnode> Cond, Body; // Body is null for "while (c) ;"

public:
  WhileExprAST(TOKEN tok, std::unique_ptr<ASTnode> cond,
               std::unique_ptr<ASTnode> body)
      : Tok(tok), Cond(std::move(cond)), Body(std::move(body)) {}
  llvm::Value *codegen() override; // Step 6: loop back to the condition
  std::string to_string() const override {
    return treeNode("While", {labelled("Condition", nodeToString(Cond.get())),
                              labelled("Body", nodeToString(Body.get()))});
  }
};

/// ReturnAST - Class for a return value
class ReturnAST : public ASTnode {
  TOKEN Tok;
  std::unique_ptr<ASTnode> Val; // null for "return;"

public:
  ReturnAST(TOKEN tok, std::unique_ptr<ASTnode> value)
      : Tok(tok), Val(std::move(value)) {}
  llvm::Value *codegen() override;
  std::string to_string() const override {
    if (!Val)
      return "Return (void)";
    return treeNode("Return", {nodeToString(Val.get())});
  }
};

/// ArgsAST - A function call: the callee's name and its argument expressions.
class ArgsAST : public ASTnode {
  TOKEN Tok;
  std::string Callee;
  std::vector<std::unique_ptr<ASTnode>> ArgsList;

public:
  ArgsAST(TOKEN tok, const std::string &Callee,
          std::vector<std::unique_ptr<ASTnode>> list)
      : Tok(tok), Callee(Callee), ArgsList(std::move(list)) {}
  llvm::Value *codegen() override; // Step 5: call the function
  std::string to_string() const override {
    std::vector<std::string> kids;
    for (const auto &arg : ArgsList)
      kids.push_back(nodeToString(arg.get()));
    if (kids.empty())
      return "Call '" + Callee + "' (no arguments)";
    return treeNode("Call '" + Callee + "'", kids);
  }
};

/// ProgramAST - Root of the tree, added in Part 1 Task 3. The starter parser
/// threw each top-level declaration away once it was parsed, so there was
/// nothing to print; this node keeps them all, externs first.
class ProgramAST : public ASTnode {
  std::vector<std::unique_ptr<FunctionPrototypeAST>> Externs;
  std::vector<std::unique_ptr<ASTnode>> Decls;

public:
  ProgramAST(std::vector<std::unique_ptr<FunctionPrototypeAST>> externs,
             std::vector<std::unique_ptr<ASTnode>> decls)
      : Externs(std::move(externs)), Decls(std::move(decls)) {}
  llvm::Value *codegen() override;
  std::string to_string() const override {
    std::vector<std::string> kids;
    for (const auto &ext : Externs)
      kids.push_back(ext->to_string("ExternDecl"));
    for (const auto &decl : Decls)
      kids.push_back(nodeToString(decl.get()));
    return treeNode("Program", kids);
  }
};

#endif
