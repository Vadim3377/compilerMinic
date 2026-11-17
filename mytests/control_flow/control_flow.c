// if/while edge cases: non-bool conditions, returns in branches, dead code
// after return, functions that fall off the end, and empty loop bodies.
int cnt;

int cond_int(int n) { int c; c = 0; while (n) { n = n - 1; c = c + 1; } if (2.5) { c = c + 100; } return c; }
int both_return(int n) { if (n > 0) { return 1; } else { return 2; } }
int after_return(int n) { return n; n = n + 1; return 99; }
int return_in_loop(int n) { while (true) { if (n > 5) { return n; } n = n + 1; } }
int falls_off(int n) { n = n + 1; }
int empty_int() { }
void empty_void() { }
void early_void(int n) { if (n > 0) { return; } cnt = 77; }
bool step() { cnt = cnt + 1; return cnt < 5; }
int empty_while() { cnt = 0; while (step()) ; return cnt; }
int if_no_else(int n) { int r; r = 0; if (n == 3) { r = 1; } return r; }
int nested_if(int a, int b) {
  if (a > 0) {
    if (b > 0) { return 1; } else { return 2; }
  } else {
    if (b > 0) { return 3; }
  }
  return 4;
}
int nested_loops(int n) {
  int i; int j; int s;
  i = 0; s = 0;
  while (i < n) {
    j = 0;
    while (j < i) { s = s + 1; j = j + 1; }
    i = i + 1;
  }
  return s;
}
int call_void() { empty_void(); cnt = 0; early_void(1); early_void(0); return cnt; }
