// Global variables: zero-initialised, shared between functions, visible to
// the driver, and widened on assignment like locals.
int counter;
float gf;
bool gb;

int read_untouched() { return counter; }
void increment() { counter = counter + 1; }
int read_counter() { return counter; }
float use_globals() { gf = 2; gb = true; if (gb) { gf = gf * 2.5; } return gf; }
