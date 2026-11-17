// Implicit widening bool -> int -> float, and arithmetic on bools.
float widen_assign() { int i; bool b; float f; i = true + 5; b = true; f = i; f = f + b; return f; }
int bool_plus() { return true + true; }
int bool_neg() { return -true; }
int bool_compare() { int r; r = 0; if (true > false) { r = 1; } return r; }
float mixed(int a, float b) { return a / 2 + b * 2; }
float arg_widen() { return mixed(true, 2); }
float chain_types() { float f; int i; f = i = 3; return f / 2; }
int chain_same() { int a; int b; a = b = 7; return a + b; }
float bool_to_float_param(float x) { return x; }
float pass_bool() { return bool_to_float_param(true); }
int not_int(int a) { return !a; }
float return_widened() { return 3; }
