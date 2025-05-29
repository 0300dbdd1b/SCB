
// SCB: @output(testout)
// SCB: @global-cc(clang)
// SCB: @ld(clang, platform=unix)
// SCB: @cc(clang, platform=unix)
// SCB: @cc(cl, platform=windows)
// SCB: @global-cflags(-std=c11 -pedantic-errors, platform=unix)
// SCB: @sources( src/tests/printing-functions.c src/tests/math-functions.c)


void print_hello_world(void);
int main(void)
{
	print_hello_world();
}
