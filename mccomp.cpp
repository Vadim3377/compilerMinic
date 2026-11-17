// mccomp.cpp - the MiniC compiler driver.
//
// Usage: ./mccomp input.c
// On success it prints the AST, writes LLVM IR to output.ll in the current
// directory and exits with 0.
//
// Exit codes:
//   0  success
//   1  usage error, or a file could not be opened
//   2  an error in the MiniC program (lexical, syntax or semantic)
//   3  internal compiler error: we generated IR that LLVM rejects
//
// Source files:
//   lexer.h / lexer.cpp      tokens and the lexer (starter code)
//   ast.h / ast.cpp          AST node classes and AST printing (Part 1)
//   parser.h / parser.cpp    recursive descent parser (Part 1)
//   codegen.h / codegen.cpp  LLVM IR generation and semantic checks (Part 2)
//
// Generative AI (Claude, Anthropic) was used while writing parts of this
// compiler, as declared in the report.

#include "codegen.h"
#include "lexer.h"
#include "parser.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdio>
#include <iostream>

using namespace llvm;

int main(int argc, char **argv) {
  if (argc == 2) {
    pFile = fopen(argv[1], "r");
    if (pFile == NULL) {
      perror("Error opening file");
      // Part 2 Step 0: the starter carried on after this message, and the
      // lexer then read from a null FILE* and crashed with a segfault.
      return 1;
    }
  } else {
    std::cout << "Usage: ./code InputFile\n";
    return 1;
  }

  lineNo = 1;
  columnNo = 1;

  TheModule = std::make_unique<Module>("mini-c", TheContext);

  // Part 1. Any lexical or syntax error exits with status 2 inside the
  // parser, so getting past this line means the program parsed.
  auto RootASTnode = ParseProgram();
  fprintf(stderr, "Parsing Finished: program is syntactically valid\n");

  // Part 1, Task 3: print the AST.
  llvm::outs() << "********************* AST (begin) *********************\n";
  llvm::outs() << *RootASTnode << "\n";
  llvm::outs() << "********************* AST (end) ***********************\n";
  llvm::outs().flush();

  // Part 2: generate IR for the whole program. Semantic errors exit with
  // status 2 from inside codegen(). Each function was already verified on
  // its own; this last check also covers globals and the module as a whole.
  RootASTnode->codegen();
  if (verifyModule(*TheModule, &errs())) {
    fprintf(stderr, "Internal compiler error: the generated module is invalid\n");
    return 3;
  }

  printf(
      "********************* FINAL IR (begin) ****************************\n");
  auto Filename = "output.ll";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message();
    return 1;
  }
  TheModule->print(dest, nullptr);
  printf(
      "********************* FINAL IR (end) ******************************\n");

  fclose(pFile);
  return 0;
}
