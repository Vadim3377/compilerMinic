// EXPECT: invalid array use
int f(int a[3]) { return a[0]; } int main() { int m[3][3]; return f(m); }
