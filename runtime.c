#include <stdio.h>

extern void print_string(const char *s)
{
	printf("%s", s);
}

extern void print_int(int i)
{
	printf("%d", i);
}

extern void print_boolean(int b)
{
	printf(b ? "true" : "false");
}

extern void print_char(char c)
{
	printf("%c", c);
}

extern int power(int x, int y)
{
	int result = 1;
	while (y > 0) {
		result *= x;
		--y;
	}
	return result;
}
