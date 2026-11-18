// Global arrays: zero initialised, shared between functions, shared with
// the driver, and shadowed by a local array of the same name.
int table[10];
float matrix[2][3];

int global_zero() { return table[0] + table[9]; }
void fill_table() { int i; i = 0; while (i < 10) { table[i] = i + 1; i = i + 1; } }
int table_sum() { int i; int s; i = 0; s = 0; while (i < 10) { s = s + table[i]; i = i + 1; } return s; }
float set_matrix() { matrix[1][2] = 2.5; matrix[0][1] = matrix[1][2] * 2; return matrix[0][1]; }
int shadow_global() { int table[2]; table[0] = 99; return table[0]; }
int global_after_shadow() { return table[0]; }
