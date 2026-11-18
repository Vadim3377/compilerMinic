#include <cstdio>
#include <cmath>

static int failures = 0;

// Prints one line per check, in the same style as the coursework drivers.
static void check(const char *what, bool ok) {
  if (ok) {
    printf("PASSED %s\n", what);
  } else {
    printf("FAILED %s\n", what);
    failures++;
  }
}

static bool close_to(float a, float b) { return std::fabs(a - b) < 1e-5f; }

extern "C" {
void bubble_sort(int *, int); int find(int *, int, int); int sum_rec(int *, int);
int sort_global(); int local_cube(); float local_average(); int row_of_3d();
extern int data[8];
}

int main() {
  int v[6] = {9, 4, 7, 1, 8, 2};
  bubble_sort(v, 6);
  check("bubble sort in place on a C++ array", v[0] == 1 && v[1] == 2 && v[5] == 9);
  check("linear search finds 7 at index 3", find(v, 6, 7) == 3 && find(v, 6, 5) == -1);
  check("recursive sum over an array parameter", sum_rec(v, 6) == 31);
  check("global array sorted through a parameter: 1 and 9", sort_global() == 19);
  check("driver sees the sorted global", data[0] == 1 && data[7] == 9);
  check("3D array parameter", local_cube() == 15);
  check("float array parameter", close_to(local_average(), 3.0f));
  check("row of a 3D array passed as a 1D array", row_of_3d() == 1);
  return failures;
}
