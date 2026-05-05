#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

char *read_file(const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (!file) return NULL;

	fseek(file, 0, SEEK_END);
	long filesize = ftell(file);
	rewind(file);

	char *buffer = malloc(filesize + 1);
	size_t read_size = fread(buffer, 1, filesize, file);
	if (read_size != filesize) {
		free(buffer);
		fclose(file);
		return NULL;
	}

	buffer[filesize] = '\0';
	fclose(file);

	return buffer;
}

bool write_to_file(const char *filename, const char *text) {
	FILE *file = fopen(filename, "w");
	if (file == NULL) {
		return false;
	}

	if (fputs(text, file) == EOF) {
		fclose(file);
		return false;
	}

	if (fclose(file) == EOF) {
		return false;
	}

	return true;
}
