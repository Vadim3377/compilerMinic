// Matrix multiplication with 2D arrays, a typical program for this part.
float a[3][3];
float b[3][3];
float c[3][3];

void init() {
  int i; int j;
  i = 0;
  while (i < 3) {
    j = 0;
    while (j < 3) { a[i][j] = i + j; b[i][j] = i - j; j = j + 1; }
    i = i + 1;
  }
}

void multiply() {
  int i; int j; int k; float s;
  i = 0;
  while (i < 3) {
    j = 0;
    while (j < 3) {
      s = 0; k = 0;
      while (k < 3) { s = s + a[i][k] * b[k][j]; k = k + 1; }
      c[i][j] = s;
      j = j + 1;
    }
    i = i + 1;
  }
}

float get_c(int i, int j) { return c[i][j]; }
