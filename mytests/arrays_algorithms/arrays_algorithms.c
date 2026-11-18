// Typical array programs: sorting in place through a parameter, searching,
// a global array passed to a function, 3D and float array parameters, and
// recursion over an array.
int data[8];

void bubble_sort(int a[], int n) {
  int i; int j; int t;
  i = 0;
  while (i < n - 1) {
    j = 0;
    while (j < n - 1 - i) {
      if (a[j] > a[j + 1]) { t = a[j]; a[j] = a[j + 1]; a[j + 1] = t; }
      j = j + 1;
    }
    i = i + 1;
  }
}

int find(int a[], int n, int key) {
  int i;
  i = 0;
  while (i < n) {
    if (a[i] == key) { return i; }
    i = i + 1;
  }
  return -1;
}

int sum_rec(int a[], int n) {
  if (n == 0) { return 0; }
  return a[n - 1] + sum_rec(a, n - 1);
}

void fill_data() {
  data[0] = 5; data[1] = 3; data[2] = 8; data[3] = 1;
  data[4] = 9; data[5] = 2; data[6] = 7; data[7] = 4;
}

int sort_global() { fill_data(); bubble_sort(data, 8); return data[0] * 10 + data[7]; }

int cube_sum(int c[2][2][2]) {
  int i; int j; int k; int s;
  s = 0; i = 0;
  while (i < 2) {
    j = 0;
    while (j < 2) {
      k = 0;
      while (k < 2) { s = s + c[i][j][k]; k = k + 1; }
      j = j + 1;
    }
    i = i + 1;
  }
  return s;
}

int local_cube() {
  int c[2][2][2];
  c[0][0][0] = 1; c[1][1][1] = 8; c[1][0][1] = 6;
  return cube_sum(c);
}

float average(float v[4]) { return (v[0] + v[1] + v[2] + v[3]) / 4; }

float local_average() { float v[4]; v[0] = 1; v[1] = 2.5; v[2] = 3; v[3] = 5.5; return average(v); }

int row_of_3d() { int c[2][2][2]; c[1][0][0] = 4; c[1][0][1] = 5; return find(c[1][0], 2, 5); }
