// Arrays as function parameters: passed by reference as a pointer, sized and
// unsized first dimension, 2D parameters, passing a row, passing a
// parameter on, and receiving an array from the C++ driver.
int sum(int a[10], int n) {
  int i; int s;
  i = 0; s = 0;
  while (i < n) { s = s + a[i]; i = i + 1; }
  return s;
}

void set_all(int a[], int n, int v) { int i; i = 0; while (i < n) { a[i] = v; i = i + 1; } }

int pass_on(int a[], int n) { return sum(a, n); }

float trace(float m[3][3]) { return m[0][0] + m[1][1] + m[2][2]; }

int row_sum(int r[4]) { return r[0] + r[1] + r[2] + r[3]; }

float local_calls() {
  int a[10]; int m[3][4]; float f[3][3];
  set_all(a, 10, 2);
  m[2][0] = 1; m[2][3] = 9;
  f[0][0] = 1.5; f[1][1] = 2.5; f[2][2] = 3;
  return sum(a, 10) + pass_on(a, 5) + row_sum(m[2]) + trace(f);
}

int modify(int a[5]) { a[0] = 42; return 0; }
int by_reference() { int a[5]; modify(a); return a[0]; }
