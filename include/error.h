#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
	char *file;
	size_t line_num;
	char *line_start;
	char *line_char;
} Location;

typedef enum : uint8_t {
	ERROR_COMPTIME,
	ERROR_RUNTIME,
} ErrorKind;

typedef void (*ErrorFn)(Location loc, ErrorKind ek, char *msg);

typedef struct {
	ErrorFn errf;
	bool got_err;
} ErrorCtx;

#endif
