// Precedence, associativity and signed integer semantics.
int precedence() { return 2 + 3 * 4 - 10 / 2 % 3; }
int left_assoc() { return 20 - 5 - 3; }
int div_assoc() { return 64 / 4 / 2; }
int signed_div() { return -7 / 2; }
int signed_mod() { return -7 % 3; }
float float_neg(float f) { return -f; }
int double_neg(int x) { return - -x; }
int compare_all(int a, int b) {
  return (a < b) + (a <= b) * 2 + (a > b) * 4 + (a >= b) * 8 + (a == b) * 16 + (a != b) * 32;
}
int parens() { return (2 + 3) * (4 - 1); }
int unary_prec() { return -2 * 3 + !0; }
