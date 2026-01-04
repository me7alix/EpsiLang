#ifndef ERR_H
#define ERR_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
	char *file;
	size_t line_num;
	char *line_start;
	char *line_char;
} Location;

typedef enum {
	ERROR_COMPTIME,
	ERROR_RUNTIME,
} ErrorKind;

typedef void (*ErrorFn)(Location loc, ErrorKind ek, char *msg);

typedef struct {
	ErrorFn errf;
	bool got_err;
} ErrorCtx;

#endif
