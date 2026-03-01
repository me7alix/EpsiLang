#ifndef UTF8_H_
#define UTF8_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define UTF8_INVALID 0xFFFFFFFFu

typedef uint32_t UTF8_Rune;

static UTF8_Rune utf8_decode(char *s, size_t *bytes_read) {
	unsigned char *p = (unsigned char *)s;

	if (p[0] < 0x80) {
		*bytes_read = 1;
		return p[0];
	}

	if ((p[0] & 0xE0) == 0xC0) {
		*bytes_read = 2;
		return ((p[0] & 0x1F) << 6) |
		       (p[1] & 0x3F);
	}

	if ((p[0] & 0xF0) == 0xE0) {
		*bytes_read = 3;
		return ((p[0] & 0x0F) << 12) |
		       ((p[1] & 0x3F) << 6) |
		       (p[2] & 0x3F);
	}

	if ((p[0] & 0xF8) == 0xF0) {
		*bytes_read = 4;
		return ((p[0] & 0x07) << 18) |
		       ((p[1] & 0x3F) << 12) |
		       ((p[2] & 0x3F) << 6) |
		       (p[3] & 0x3F);
	}

	*bytes_read = 1;
	return 0xFFFD;
}

static int utf8_encode(UTF8_Rune cp, char out[4]) {
	if (cp > 0x10FFFF)
		return -1;

	if (cp >= 0xD800 && cp <= 0xDFFF)
		return -1;

	if (cp <= 0x7F) {
		out[0] = cp;
		return 1;
	}

	if (cp <= 0x7FF) {
		out[0] = 0xC0 | (cp >> 6);
		out[1] = 0x80 | (cp & 0x3F);
		return 2;
	}

	if (cp <= 0xFFFF) {
		out[0] = 0xE0 | (cp >> 12);
		out[1] = 0x80 | ((cp >> 6) & 0x3F);
		out[2] = 0x80 | (cp & 0x3F);
		return 3;
	}

	out[0] = 0xF0 | (cp >> 18);
	out[1] = 0x80 | ((cp >> 12) & 0x3F);
	out[2] = 0x80 | ((cp >> 6) & 0x3F);
	out[3] = 0x80 | (cp & 0x3F);

	return 4;
}

static size_t utf8_len(char *s) {
	size_t len = 0;

	while (*s) {
		size_t bytes;
		utf8_decode(s, &bytes);
		s += bytes;
		len++;
	}

	return len;
}

static UTF8_Rune utf8_get_nth(char *s, size_t n) {
	size_t index = 0;

	while (*s) {
		size_t bytes;
		UTF8_Rune cp = utf8_decode(s, &bytes);

		if (index == n)
			return cp;

		s += bytes;
		index++;
	}

	return 0;
}

// You must make sure there is enough space in the string yourself
static int utf8_set_nth(char *s, size_t n, UTF8_Rune new_cp) {
	size_t count = 0;
	char *p = s;

	while (*p) {
		size_t old_len = 0;
		UTF8_Rune cp = utf8_decode(p, &old_len);

		if (cp == UTF8_INVALID)
			return -1;

		if (count == n) {
			char new_bytes[4];
			int new_len = utf8_encode(new_cp, new_bytes);

			if (new_len < 0)
				return -1;

			if (new_len != old_len) {
				size_t tail_len = strlen(p + old_len);
				memmove(p + new_len,
				        p + old_len,
				        tail_len + 1);
			}

			memcpy(p, new_bytes, new_len);
			return 0;
		}

		p += old_len;
		count++;
	}

	return -1;
}

#endif // UTF8_H_
