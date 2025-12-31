#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>

typedef enum {
	TYPE_NULL,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_BOOL,
	TYPE_ARR,
} TypeKind;

typedef struct {
	TypeKind kind;
} Type;

#endif
