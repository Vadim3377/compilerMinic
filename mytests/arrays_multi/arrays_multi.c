// 2D and 3D arrays: row-major layout, expression indices, a local array
// declared in a loop body starts at zero on every iteration.
int grid() {
  int g[3][4]; int i; int j; int s;
  i = 0;
  while (i < 3) {
    j = 0;
    while (j < 4) { g[i][j] = i * 10 + j; j = j + 1; }
    i = i + 1;
  }
  s = g[0][0] + g[1][2] + g[2][3];
  return s;
}

float cube() {
  float arr3D[3][3][4]; int i; int j;
  i = 1; j = 2;
  arr3D[i][j][i + j] = 1.5;
  arr3D[2][2][3] = arr3D[1][2][3] * 2;
  return arr3D[2][2][3] + arr3D[0][0][0];
}

int index_expr() { int m[4][4]; int i; int j; i = 1; j = 1; m[i + 2][j * 2] = 7; return m[3][2]; }

int fresh_each_iteration() {
  int k; int total;
  k = 0; total = 0;
  while (k < 3) {
    int t[2][2];
    total = total + t[1][1];
    t[1][1] = 100;
    k = k + 1;
  }
  return total;
}

int diag(int n) {
  int m[3][3]; int i; int s;
  i = 0;
  while (i < 3) { m[i][i] = n; i = i + 1; }
  s = 0; i = 0;
  while (i < 9) { s = s + m[i / 3][i % 3]; i = i + 1; }
  return s;
}
