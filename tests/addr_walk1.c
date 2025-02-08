
#include <stdint.h>

#include "display.h"
#include "error.h"
#include "test.h"

#include "test_funcs.h"
#include "test_helper.h"

#include "xhci.h"

extern uintptr_t global_ws;

int test_addr_walk1(int my_cpu)
{
	int ticks = 0;

	do_tick(my_cpu);
	BAILOUT;

	return ticks;
}
