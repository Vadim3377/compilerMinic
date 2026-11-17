// ast.cpp - helpers used by the to_string() methods in ast.h (Part 1, Task 3).

#include "ast.h"

// Glue a prefix onto every line of a multi-line string. The first line gets
// `first` (the connector pointing at this child) and the rest get `rest`
// (a vertical bar or spaces, so the lines of a deeper subtree stay lined up
// under their parent). This is the only piece of real indentation logic;
// everything else is built on top of it.
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

// A node's header followed by its children. The last child gets "`-" and no
// bar below it, every other child gets "|-" with a bar continuing down to
// the next sibling, which is how the clang AST dump draws its trees.
std::string treeNode(const std::string &header,
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

// An extra label level such as "Condition" or "Then", so an if statement
// shows which subtree is which instead of three anonymous children.
std::string labelled(const std::string &label, const std::string &child) {
  return treeNode(label, {child});
}

// Operators are stored as token types, which are partly negative numbers
// (EQ is -19, for example), so they need translating back into text.
std::string opToString(int op) {
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

// Some children are legitimately missing, like the body of "while (c) ;" or
// the empty statement ";", so printing must not dereference a null pointer.
std::string nodeToString(const ASTnode *node) {
  if (node == nullptr)
    return "<empty>";
  return node->to_string();
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const ASTnode &ast) {
  os << ast.to_string();
  return os;
}
