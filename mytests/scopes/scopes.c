// Nested scopes, shadowing, and a local hiding a global.
int x;

int shadow(int a) {
  int r;
  x = 1;
  r = 0;
  {
    int x;
    x = 10;
    {
      float x;
      x = 2.5;
      r = r + 1;
    }
    r = r + x;
  }
  r = r + x;
  return r + a;
}
int get_x() { return x; }
int param_shadow(int a) { { int a; a = 3; } return a; }
int param_assign(int n) { n = n * 2; return n; }
int factorial(int n) {
  int factorial;
  factorial = 1;
  while (n > 1) { factorial = factorial * n; n = n - 1; }
  return factorial;
}
