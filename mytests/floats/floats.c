// Float comparisons, including NaN, where != must be true (unordered).
bool feq(float a, float b) { return a == b; }
bool fne_int(float a, int b) { return a != b; }
bool nan_ne() { float z; float n; z = 0.0; n = z / z; return n != n; }
bool nan_eq() { float z; float n; z = 0.0; n = z / z; return n == n; }
int nan_cond() { float z; z = 0.0; if (z / z) { return 1; } return 0; }
float average(int a, int b) { return (a + b) / 2.0; }
