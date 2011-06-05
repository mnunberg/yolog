#include "yolog.h"
YOLOG_STATIC_INIT(__FILE__, YOLOG_INFO);

void do_something(void) {
	yolog_debug("This should not display!");
	yolog_info("This should display. OHAI!");
}
