#ifndef TYPE_H
#define TYPE_H

#include <stdlib.h>
#include <stdbool.h>

typedef enum {
	TYPE_INT,
	TYPE_FLOAT,
} TypeKind;

typedef struct {
	TypeKind kind;

	union {
		struct {
			bool is_signed;
			size_t size;
		} tint;
		struct {
			size_t size;
		} tfloat;
	} as;
} Type;

#endif
