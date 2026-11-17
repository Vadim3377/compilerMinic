// codegen.h - the LLVM objects shared between code generation and main().

#ifndef MCCOMP_CODEGEN_H
#define MCCOMP_CODEGEN_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <memory>

// The context owns every LLVM type and constant, and the module is the
// container all generated functions and globals go into. main() creates the
// module before code generation and prints it to output.ll afterwards.
extern llvm::LLVMContext TheContext;
extern std::unique_ptr<llvm::Module> TheModule;

#endif
