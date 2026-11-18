// 1D arrays: zero initialisation, expression indices, elements in
// expressions and as call arguments, float arrays, nested indexing.
extern int print_int(int X);

int twice(int x) { return x * 2; }

int zero_init() { int a[5]; return a[0] + a[1] + a[2] + a[3] + a[4]; }

int fill_and_sum(int n) {
  int a[10]; int i; int s;
  i = 0;
  while (i < n) { a[i] = i * i; i = i + 1; }
  s = 0; i = 0;
  while (i < n) { s = s + a[i]; i = i + 1; }
  return s;
}

int expr_index() {
  int arr[10]; int i; int j;
  i = 2; j = 4;
  arr[i + 1] = 5;
  arr[(i + 1) * (j - 2) + twice(1)] = 9;
  return arr[3] * 10 + arr[8];
}

int element_as_arg() { int a[3]; a[2] = 21; return twice(a[2]); }

float float_array() { float f[4]; f[0] = 1.5; f[1] = 2; f[3] = f[0] + f[1]; return f[3] + f[2]; }

int nested_index() { int a[5]; a[0] = 3; a[3] = 4; a[4] = 7; return a[a[a[0]]]; }

int bool_index() { int a[2]; a[true] = 6; return a[1]; }

int chain_assign() { int a[3]; int x; x = a[0] = a[1] = 8; return x + a[0] + a[1]; }
