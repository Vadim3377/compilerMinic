// parser.h - the recursive descent parser's single entry point.

#ifndef MCCOMP_PARSER_H
#define MCCOMP_PARSER_H

#include "ast.h"

#include <memory>

// Parse the whole of pFile and return the root of its AST. Any syntax error
// prints a message and exits with status 2, so a returned tree is always a
// syntactically valid program.
std::unique_ptr<ProgramAST> ParseProgram();

#endif
