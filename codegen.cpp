// codegen.cpp - turns the AST into LLVM IR (Part 2).
//
// The work is split into steps that each make one more kind of MiniC
// program compile:
//   Step 1  shared helpers: types, widening, truth values, symbol tables
//   Step 2  literals, functions, blocks, return and the program root
//   Step 3  local variables, reading a variable, assignment
//   Step 4  unary and binary operators, with implicit widening
// Later steps (calls, if/while, && and ||, globals)
// add their codegen() bodies to the end of this file.
//
// Two rules hold everywhere. First, every MiniC variable lives in memory (an
// alloca on the stack, or a global), and is read with load and written with
// store. LLVM registers are SSA and can only be assigned once, so memory is
// the simple way to model variables that change; LLVM's mem2reg pass can turn
// them back into registers later. Second, semantic errors are found here
// while generating code, not in a separate pass: codegen() is the point where
// we know every operand's type anyway.

#include "codegen.h"
#include "ast.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace llvm;

LLVMContext TheContext;
std::unique_ptr<Module> TheModule;

// The builder remembers an insertion point (a basic block) and appends each
// instruction we create to it. Only code generation uses it.
static IRBuilder<> Builder(TheContext);

//===----------------------------------------------------------------------===//
// Step 1 - helpers shared by every codegen() body
//===----------------------------------------------------------------------===//

// Step 1. Every semantic error goes through this one function so that the
// messages all look the same: position, then "Semantic error", then the kind
// in brackets (type, scope, arity, undeclared). The spec asks for the kind of
// error to be named and for a non-zero exit status; we use 2 for every error
// in the MiniC program. It returns a Value* only so callers can
// write "return SemanticError(...)" inside a codegen() body.
static Value *SemanticError(const TOKEN &tok, const char *kind,
                            const std::string &msg) {
  fprintf(stderr, "%d:%d Semantic error (%s): %s\n", tok.lineNo, tok.columnNo,
          kind, msg.c_str());
  exit(2);
  return nullptr;
}

// Step 1. The three MiniC value types as LLVM types: int is a 32-bit
// integer, float a 32-bit float, and bool a 1-bit integer (i1), which is
// what comparison instructions produce, so conditions need no conversion.
static Type *IntTy() { return Type::getInt32Ty(TheContext); }
static Type *FloatTy() { return Type::getFloatTy(TheContext); }
static Type *BoolTy() { return Type::getInt1Ty(TheContext); }

// Step 1. The AST stores types as the strings the parser saw ("int",
// "void", ...), so we need a way from those names to LLVM types. Anything
// that is not a value type must be "void", the only other type keyword.
static Type *typeFromName(const std::string &name) {
  if (name == "int")
    return IntTy();
  if (name == "float")
    return FloatTy();
  if (name == "bool")
    return BoolTy();
  return Type::getVoidTy(TheContext);
}

// Step 1. The reverse of typeFromName, only for error messages: "cannot
// return float from a function returning int" reads much better than
// printing LLVM's own names i32 and float.
static std::string typeName(Type *T) {
  if (T->isIntegerTy(1))
    return "bool";
  if (T->isIntegerTy(32))
    return "int";
  if (T->isFloatTy())
    return "float";
  return "void";
}

// Step 1. MiniC allows only widening conversions, bool -> int -> float, so
// the types form a chain. Giving each one a rank turns "is this conversion
// allowed?" into a single comparison: From may become To when
// rank(From) <= rank(To). void has no value, so it gets -1 and never passes.
static int typeRank(Type *T) {
  if (T->isIntegerTy(1))
    return 0;
  if (T->isIntegerTy(32))
    return 1;
  if (T->isFloatTy())
    return 2;
  return -1;
}

// Step 1. Implicit conversion, used wherever a value meets an expected type:
// returns, assignments, call arguments and binary operators. If the
// conversion widens, we emit the instruction that does it. A bool is
// unsigned (true must become 1, not -1), so it uses zext and uitofp, while
// an int is signed and uses sitofp. If it would narrow, for example float to
// int, we return nullptr and let the caller report the error, because only
// the caller knows whether it was a return, an assignment or an argument.
static Value *widen(Value *V, Type *To) {
  Type *From = V->getType();
  if (From == To)
    return V;
  if (typeRank(From) < 0 || typeRank(From) > typeRank(To))
    return nullptr;
  if (To->isFloatTy()) {
    if (From->isIntegerTy(1))
      return Builder.CreateUIToFP(V, To, "booltofp");
    return Builder.CreateSIToFP(V, To, "inttofp");
  }
  return Builder.CreateZExt(V, To, "booltoint");
}

// Step 1. Conditions in MiniC follow C: any int or float can be used as a
// condition, and zero means false. A branch instruction needs an i1, so this
// compares the value with zero. For floats it uses an unordered comparison,
// so NaN counts as true, the same as in C. Later steps use it for if, while,
// ! and the operands of && and ||.
static Value *toBool(Value *V) {
  Type *T = V->getType();
  if (T->isIntegerTy(1))
    return V;
  if (T->isFloatTy())
    return Builder.CreateFCmpUNE(V, ConstantFP::get(T, 0.0), "tobool");
  return Builder.CreateICmpNE(V, ConstantInt::get(T, 0), "tobool");
}

// Step 1. A call to a void function is a valid statement but has no value,
// so "x = f();" or "return f();" with a void f is a type error. Every place
// that needs a value passes its operand through here first, which also
// stops a void value from reaching LLVM, where it would fail the verifier
// with a much less helpful message.
static Value *requireValue(Value *V, const TOKEN &tok,
                           const std::string &what) {
  if (!V || V->getType()->isVoidTy())
    return SemanticError(tok, "type", what + " is void: it has no value");
  return V;
}

// Step 1. The symbol table. Locals are a stack of scopes, one map per open
// block, with the innermost scope at the back. Entering a block pushes an
// empty map and leaving it pops the map, so names declared inside a block
// disappear at its closing brace. Globals never go out of scope, so they
// sit in their own map that is searched last.
static std::vector<std::map<std::string, AllocaInst *>> Scopes;
static std::map<std::string, GlobalVariable *> GlobalNamedValues;

// Step 1. What a name lookup gives back: the address of the variable (an
// alloca or a global) and the type stored at that address. LLVM pointers do
// not carry the type they point to, so we keep it next to the pointer, ready
// for the load.
struct VarInfo {
  Value *Ptr = nullptr;
  Type *Ty = nullptr;
};

// Step 1. Finds which declaration a name refers to. Searching from the
// innermost scope outwards is what makes shadowing work: in
// "int x; { float x; x = 1.0; }" the inner x is found first. If no scope has
// the name we fall back to the globals, and if they do not have it either,
// Ptr stays null and the caller reports "undeclared variable".
static VarInfo lookupVariable(const std::string &name) {
  VarInfo info;
  for (int i = (int)Scopes.size() - 1; i >= 0; i--) {
    auto it = Scopes[i].find(name);
    if (it != Scopes[i].end()) {
      info.Ptr = it->second;
      info.Ty = it->second->getAllocatedType();
      return info;
    }
  }
  auto g = GlobalNamedValues.find(name);
  if (g != GlobalNamedValues.end()) {
    info.Ptr = g->second;
    info.Ty = g->second->getValueType();
  }
  return info;
}

// Step 1. Reserves a stack slot for a variable. The alloca always goes at
// the top of the function's entry block, even for a variable declared inside
// a loop. There it runs once per call, so a loop does not grow the stack on
// every iteration, and mem2reg only promotes allocas that are in the entry
// block. A temporary builder is used so the main Builder keeps its place.
static AllocaInst *CreateEntryBlockAlloca(Function *F, const std::string &name,
                                          Type *Ty) {
  IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
  return TmpB.CreateAlloca(Ty, nullptr, name);
}

// Step 1. Declares a local in the current scope. Two declarations of the
// same name in one block is an error, but the same name in an inner block
// is allowed, because that is shadowing. Only the innermost map is checked,
// which is exactly that rule.
static AllocaInst *declareLocal(const TOKEN &tok, const std::string &name,
                                Type *Ty) {
  if (Scopes.back().count(name) > 0)
    SemanticError(tok, "scope",
                  "redeclaration of '" + name + "' in the same scope");
  Function *F = Builder.GetInsertBlock()->getParent();
  AllocaInst *Slot = CreateEntryBlockAlloca(F, name, Ty);
  Scopes.back()[name] = Slot;
  return Slot;
}

//===----------------------------------------------------------------------===//
// Step 2 - literals, functions, blocks, return and the program
//
// After this step the smallest real program, "int main() { return 5; }",
// compiles to IR that runs.
//===----------------------------------------------------------------------===//

// Step 2. Literals need no instructions at all. LLVM treats constants as
// Values we can use directly as operands, so an integer literal is just an
// i32 constant. The last argument marks it as signed, as all MiniC ints are.
Value *IntASTnode::codegen() { return ConstantInt::get(IntTy(), Val, true); }

// Step 2. Same idea for a float literal. The parser kept it as a double, and
// ConstantFP rounds it to the 32-bit float type for us.
Value *FloatASTnode::codegen() { return ConstantFP::get(FloatTy(), Val); }

// Step 2. And for true and false, which become the i1 constants 1 and 0.
Value *BoolASTnode::codegen() { return ConstantInt::get(BoolTy(), Bool); }

// Step 2. Builds a function's signature: its LLVM FunctionType from the
// parameter and return types, and the Function object in the module. It is
// used for an extern on its own, and as the first half of every function
// definition. A function can be declared twice (an extern and then the
// definition, for example), so if one with this name already exists we reuse
// it, but only if the signature is identical. A name already taken by a
// global variable is a scope error.
//
// The function gets external linkage, so the C++ test driver can call it by
// name and an extern with no body is filled in by the linker. A bool is
// passed in a full register, so bool parameters and bool returns are marked
// zeroext. That tells the C++ side on the other end of the call that the
// upper bits are zero, which is what it expects.
Function *FunctionPrototypeAST::codegen() {
  std::vector<llvm::Type *> ParamTys;
  for (const auto &Param : Params)
    ParamTys.push_back(typeFromName(Param->getType()));
  llvm::Type *RetTy = typeFromName(Type);
  FunctionType *FT = FunctionType::get(RetTy, ParamTys, false);

  if (Function *Existing = TheModule->getFunction(Name)) {
    if (Existing->getFunctionType() != FT)
      SemanticError(Tok, "type",
                    "conflicting declarations of function '" + Name + "'");
    return Existing;
  }
  if (GlobalNamedValues.count(Name) > 0)
    SemanticError(Tok, "scope",
                  "'" + Name + "' is already declared as a global variable");

  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

  if (RetTy->isIntegerTy(1))
    F->addRetAttr(Attribute::ZExt);
  unsigned Idx = 0;
  for (auto &Arg : F->args()) {
    Arg.setName(Params[Idx]->getName());
    if (Arg.getType()->isIntegerTy(1))
      Arg.addAttr(Attribute::ZExt);
    Idx++;
  }
  return F;
}

// Step 2. Generates a whole function definition. After getting the signature
// from the prototype, a Function that already has a body means this is the
// second definition of the same name, which is an error.
//
// Code goes into an "entry" block. Parameters arrive as SSA values, which
// cannot be assigned to, but MiniC lets a function change its parameters
// like any other local. So each parameter is copied into its own alloca and
// registered in the function's scope. The parameters and the body's top
// level declarations share that one scope, which is why the body is
// generated with codegenInScope() instead of codegen(): "int f(int x)
// { int x; }" is then correctly reported as a redeclaration.
//
// Every basic block must end in a terminator, and the body might not end
// with a return (a void function often does not). So a return is always
// added at the end: ret void, or ret 0 for a value function, which is our
// choice for a function that falls off its end. If the body did return,
// that extra ret lands in an unreachable block, and EliminateUnreachableBlocks
// removes it along with any other dead code. Finally verifyFunction checks
// the IR. If it fails, that is a bug in this compiler rather than in the
// MiniC program, so it gets its own exit status, 3.
Value *FunctionDeclAST::codegen() {
  Function *F = Proto->codegen();
  if (!F->empty())
    return SemanticError(Proto->getTok(), "scope",
                         "redefinition of function '" + Proto->getName() + "'");

  BasicBlock *EntryBB = BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(EntryBB);

  Scopes.push_back({});
  unsigned Idx = 0;
  for (auto &Arg : F->args()) {
    // An earlier extern may have used different parameter names, so this
    // definition's names are set again.
    const std::string &ParamName = Proto->getParams()[Idx]->getName();
    Arg.setName(ParamName);
    AllocaInst *Slot = declareLocal(Proto->getTok(), ParamName, Arg.getType());
    Builder.CreateStore(&Arg, Slot);
    Idx++;
  }

  static_cast<BlockAST *>(Block.get())->codegenInScope();
  Scopes.pop_back();

  if (F->getReturnType()->isVoidTy())
    Builder.CreateRetVoid();
  else
    Builder.CreateRet(Constant::getNullValue(F->getReturnType()));

  EliminateUnreachableBlocks(*F);
  if (verifyFunction(*F, &errs())) {
    fprintf(stderr, "Internal compiler error: invalid IR for function '%s'\n",
            Proto->getName().c_str());
    exit(3);
  }
  return F;
}

// Step 2. A block used as a statement, like "{ int y; y = 1; }" inside a
// function, opens a new scope for its declarations and closes it at the end,
// so y is not visible after the closing brace. It produces no value.
Value *BlockAST::codegen() {
  Scopes.push_back({});
  codegenInScope();
  Scopes.pop_back();
  return nullptr;
}

// Step 2. The contents of a block without opening a scope: the declarations
// first, then the statements in order. Only a function body calls this
// directly, because its scope was already opened for the parameters. The
// parser gives nullptr for an empty statement ";", and it never adds those
// to Stmts, so every pointer here is a real node.
Value *BlockAST::codegenInScope() {
  for (const auto &Decl : LocalDecls)
    Decl->codegen();
  for (const auto &Stmt : Stmts)
    Stmt->codegen();
  return nullptr;
}

// Step 2. "return;" and "return e;". The function's own return type decides
// what is allowed: a value function needs a value, a void function must not
// have one, and the value may be widened to the return type but not
// narrowed, so returning a float from an int function is an error.
//
// A ret ends its basic block, and LLVM forbids any instruction after a
// terminator. But MiniC code can continue after a return
// ("return 1; x = 2;"). So we open a fresh block, "after.ret", and carry on
// there. Nothing jumps to it, so EliminateUnreachableBlocks deletes it once
// the function is finished.
Value *ReturnAST::codegen() {
  Function *F = Builder.GetInsertBlock()->getParent();
  llvm::Type *RetTy = F->getReturnType();

  if (!Val) {
    if (!RetTy->isVoidTy())
      return SemanticError(Tok, "type",
                           "function '" + F->getName().str() + "' returns " +
                               typeName(RetTy) + ": 'return' needs a value");
    Builder.CreateRetVoid();
  } else {
    if (RetTy->isVoidTy())
      return SemanticError(Tok, "type",
                           "void function '" + F->getName().str() +
                               "' cannot return a value");
    Value *V = requireValue(Val->codegen(), Tok, "the returned expression");
    Value *Widened = widen(V, RetTy);
    if (!Widened)
      return SemanticError(Tok, "type",
                           "cannot return " + typeName(V->getType()) +
                               " from function '" + F->getName().str() +
                               "' returning " + typeName(RetTy) +
                               " (narrowing conversion)");
    Builder.CreateRet(Widened);
  }

  BasicBlock *DeadBB = BasicBlock::Create(TheContext, "after.ret", F);
  Builder.SetInsertPoint(DeadBB);
  return nullptr;
}

// Step 2. The root: externs first, then the top-level declarations in source
// order. Going in order means a function or global can only be used after
// its declaration, as in C, because lookups only see what has already been
// generated.
Value *ProgramAST::codegen() {
  for (const auto &Ext : Externs)
    Ext->codegen();
  for (const auto &Decl : Decls)
    Decl->codegen();
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Step 3 - local variables, reading a variable, and assignment
//
// After this step a function can declare locals, assign to them and read
// them back, including shadowed names in nested blocks.
//===----------------------------------------------------------------------===//

// Step 3. A local declaration such as "int x;" produces no instruction where
// it appears. All the work is in declareLocal from Step 1: it reserves the
// stack slot in the entry block and records the name in the innermost scope,
// and complains if the same block already declared x. MiniC has no
// initialisers, so nothing is stored yet, and a declaration is not an
// expression, so there is no value to return.
Value *VarDeclAST::codegen() {
  declareLocal(Var->getTok(), Var->getName(), typeFromName(Type));
  return nullptr;
}

// Step 3. Using a variable as a value, like the x in "y = x + 1". The name
// is looked up through the scopes (innermost first, then the globals), and
// its current value is loaded from the address we get back. Remember that
// variables live in memory, so reading one is always a load and never just a
// register. The load needs the stored type spelled out, because LLVM pointers
// are untyped, and lookupVariable keeps that type next to the pointer.
//
// If the name is not found, it may still be a function name used without
// brackets ("x = f;"), which deserves a clearer message than "undeclared".
Value *VariableASTnode::codegen() {
  VarInfo Var = lookupVariable(Name);
  if (!Var.Ptr) {
    if (TheModule->getFunction(Name))
      return SemanticError(Tok, "type",
                           "'" + Name + "' is a function, not a variable");
    return SemanticError(Tok, "undeclared name",
                         "use of undeclared variable '" + Name + "'");
  }
  return Builder.CreateLoad(Var.Ty, Var.Ptr, Name);
}

// Step 3. Assignment "x = e". The target is looked up first so that
// assigning to an undeclared name is reported before we generate anything
// for the right-hand side. Then e is generated, and it must have a value
// (a void call cannot be assigned). The value is widened to the variable's
// type, so "float f; f = 1;" stores 1.0, while narrowing such as storing a
// float into an int is rejected, as MiniC only allows widening.
//
// In C an assignment is itself an expression whose value is what was
// stored. Returning that value is what makes "a = b = 3" work: the inner
// assignment hands 3 to the outer one.
Value *AssignAST::codegen() {
  const std::string &Name = Var->getName();
  VarInfo Target = lookupVariable(Name);
  if (!Target.Ptr)
    return SemanticError(Var->getTok(), "undeclared name",
                         "assignment to undeclared variable '" + Name + "'");

  Value *RHS = requireValue(Val->codegen(), Var->getTok(),
                            "the right-hand side of the assignment to '" +
                                Name + "'");
  Value *Widened = widen(RHS, Target.Ty);
  if (!Widened)
    return SemanticError(Var->getTok(), "type",
                         "cannot assign " + typeName(RHS->getType()) + " to " +
                             typeName(Target.Ty) + " variable '" + Name +
                             "' (narrowing conversion)");
  Builder.CreateStore(Widened, Target.Ptr);
  return Widened;
}

//===----------------------------------------------------------------------===//
// Step 4 - unary and binary operators
//
// After this step expressions such as "a / 2 + 0.5" or "(f == 3.5) + 1"
// compile, with the operands converted to a common type first. && and || are
// left for Step 7, because they need branches rather than one instruction.
//===----------------------------------------------------------------------===//

// Step 4. Unary operators "!x" and "-x". The operand is generated first and
// must have a value.
//
// "!" works on truth values, so the operand goes through toBool (Step 1),
// which turns an int or float into an i1 by comparing it with zero, and the
// i1 is then flipped. The result of ! is therefore always a bool, as in
// "!5" giving false.
//
// "-" needs a number. As in C, a bool is first promoted to int, so -true is
// -1. LLVM has separate instructions for integers and floats: fneg for a
// float, and neg (really "0 - x") for an int.
Value *UnaryExprAST::codegen() {
  Value *V = requireValue(Operand->codegen(), Tok,
                          "the operand of '" + Tok.lexeme + "'");
  if (Op == NOT)
    return Builder.CreateNot(toBool(V), "nottmp");

  if (V->getType()->isIntegerTy(1))
    V = widen(V, IntTy());
  if (V->getType()->isFloatTy())
    return Builder.CreateFNeg(V, "negtmp");
  return Builder.CreateNeg(V, "negtmp");
}

// Step 4. Binary arithmetic and comparisons. The AST is walked in post-order:
// both operands are generated first, and then one instruction combines them.
//
// LLVM instructions need both operands to have the same type, but MiniC lets
// you write "a / 2 + 0.5" with an int on one side and a float on the other.
// So before choosing the instruction we pick a common type: the wider of the
// two in the order bool < int < float, and never narrower than int, because
// C does arithmetic and comparisons on bools as ints (true + true is 2).
// Both operands are then widened to that type, which never fails since we
// only ever go upwards.
//
// The common type then decides the instruction family. Floats use the F
// versions (fadd, fcmp). The float comparisons are "ordered", so any
// comparison with NaN is false, except != which is "unordered" so that
// NaN != NaN is true, matching C. Ints are signed in MiniC, so division,
// remainder and the ordering comparisons use the signed forms (sdiv, srem,
// icmp slt). There is no float remainder in MiniC, so % on a float is a type
// error. Every comparison produces an i1, which is a MiniC bool.
Value *ExprAST::codegen() {
  Value *L = requireValue(Node1->codegen(), Tok,
                          "the left operand of '" + Tok.lexeme + "'");
  Value *R = requireValue(Node2->codegen(), Tok,
                          "the right operand of '" + Tok.lexeme + "'");

  llvm::Type *Ty = L->getType();
  if (typeRank(R->getType()) > typeRank(Ty))
    Ty = R->getType();
  if (Ty->isIntegerTy(1))
    Ty = IntTy();
  L = widen(L, Ty);
  R = widen(R, Ty);

  if (Ty->isFloatTy()) {
    switch (Op) {
    case PLUS:
      return Builder.CreateFAdd(L, R, "addtmp");
    case MINUS:
      return Builder.CreateFSub(L, R, "subtmp");
    case ASTERIX:
      return Builder.CreateFMul(L, R, "multmp");
    case DIV:
      return Builder.CreateFDiv(L, R, "divtmp");
    case MOD:
      return SemanticError(Tok, "type",
                           "operands of '%' must be int or bool, not float");
    case LT:
      return Builder.CreateFCmpOLT(L, R, "lttmp");
    case LE:
      return Builder.CreateFCmpOLE(L, R, "letmp");
    case GT:
      return Builder.CreateFCmpOGT(L, R, "gttmp");
    case GE:
      return Builder.CreateFCmpOGE(L, R, "getmp");
    case EQ:
      return Builder.CreateFCmpOEQ(L, R, "eqtmp");
    case NE:
      return Builder.CreateFCmpUNE(L, R, "netmp");
    }
  } else {
    switch (Op) {
    case PLUS:
      return Builder.CreateAdd(L, R, "addtmp");
    case MINUS:
      return Builder.CreateSub(L, R, "subtmp");
    case ASTERIX:
      return Builder.CreateMul(L, R, "multmp");
    case DIV:
      return Builder.CreateSDiv(L, R, "divtmp");
    case MOD:
      return Builder.CreateSRem(L, R, "modtmp");
    case LT:
      return Builder.CreateICmpSLT(L, R, "lttmp");
    case LE:
      return Builder.CreateICmpSLE(L, R, "letmp");
    case GT:
      return Builder.CreateICmpSGT(L, R, "gttmp");
    case GE:
      return Builder.CreateICmpSGE(L, R, "getmp");
    case EQ:
      return Builder.CreateICmpEQ(L, R, "eqtmp");
    case NE:
      return Builder.CreateICmpNE(L, R, "netmp");
    }
  }
  // Only && and || reach this point, until Step 7 handles them.
  return SemanticError(Tok, "type",
                       "operator '" + Tok.lexeme + "' is not supported yet");
}
