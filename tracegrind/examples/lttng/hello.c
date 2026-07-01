#include <stdio.h>
#include <unistd.h>

/* The tracepoint provider is built/instantiated in hello-tp.c. Here we just
 * need the tracepoint() macro, so include the header normally. */
#include "hello-tp.h"

int main(void)
{
	int i;

	puts("Hello, World! Tracing 10 events, then exiting.");

	for (i = 0; i < 10; i++) {
		/* Fire the tracepoint. */
		tracepoint(hello_world, my_first_tracepoint, i, "hello from lttng-ust");

		/* Slow things down a bit so it's easy to follow. */
		usleep(100 * 1000);
	}

	puts("Done.");
	return 0;
}
