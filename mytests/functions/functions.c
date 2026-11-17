// Mutual recursion through an extern forward declaration (whose parameter
// name differs from the definition), recursion, and bool ABI with the driver.
extern bool is_odd(int n);
bool is_even(int n) { if (n == 0) { return true; } return is_odd(n - 1); }
bool is_odd(int k) { if (k == 0) { return false; } return is_even(k - 1); }
int fib(int n) { if (n < 2) { return n; } return fib(n - 1) + fib(n - 2); }
bool less(int a, int b) { return a < b; }
bool flip(bool b) { return !b; }
int bool_to_int(bool b) { return b + 0; }
