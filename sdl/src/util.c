#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////

[[noreturn]] void printUsage(bool fail);

////////////////////////////////////////////////////////////////////////////////

#define shift() _shift(&argc, &argv)
const char* _shift(int* argc, char*** argv);

////////////////////////////////////////////////////////////////////////////////

#define _die(mode, fmt, ...)                      \
	err##mode(                                    \
		1, "%s:%d: %s: " fmt, __FILE__, __LINE__, \
		__func__ __VA_OPT__(, ) __VA_ARGS__       \
	)

#define die(...) _die(, __VA_ARGS__)

#define dieSDL(fmt, ...) \
	_die(x, fmt ": %s", __VA_ARGS__ __VA_OPT__(, ) SDL_GetError())

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

[[noreturn]] void printUsage(bool fail) {
	extern char* program_invocation_short_name;

	const char usage[] = {
#embed "usage.txt"
		, 0
	};

	FILE* out = fail ? stderr : stdout;
	fprintf(out, usage, program_invocation_short_name);
	exit(fail ? 1 : 0);
}

const char* _shift(int* argc, char*** argv) {
	if((*argc)-- <= 1) {
		fprintf(stderr, "out of arguments!\n");
		printUsage(true);
	}

	return ((*argv)++)[1];
}

#endif
