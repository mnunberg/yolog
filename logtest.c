#include <stdio.h>
#include "yolog.h"
YOLOG_STATIC_INIT(__FILE__, YOLOG_DEBUG);
int main(void) {
	yolog_debug("Hello World");
	do_something();
	return 0;
}
