// https://spacetoast.dev/tech.html
#ifndef READALL_BUFSIZE
#define READALL_BUFSIZE 1024
#endif

#ifndef BREAD_STDIOX_IMPLEMENTATION
// header
#ifndef BREAD_STDIOX_H
#define BREAD_STDIOX_H
#include <stdio.h>
// read from src into dst until hitting EOF
// dst will be allocated using malloc and realloc, READALL_BUFSIZE at a time
// does not call fseek, so you can use this with pipes/sockets/etc
// note that as size increases, this will slow down significantly
size_t readall(char **dst, FILE *src);
#endif // BREAD_STDIOX_H

#else // BREAD_STDIOX_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>

size_t readall(char **dst, FILE *src) {
	size_t size = READALL_BUFSIZE, read = 0;
	*dst = malloc(size);
	while(!feof(src)) {
		if (size - read < READALL_BUFSIZE) {
			size = (size - read) + READALL_BUFSIZE;
			*dst = realloc(*dst, size);
		}
		read += fread((*dst) + read, 1, READALL_BUFSIZE, src);
	}
	return read;
}
#endif
