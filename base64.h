// https://spacetoast.dev/tech.html
#ifndef BREAD_BASE64_IMPLEMENTATION
// header
#ifndef BREAD_BASE64_H
#define BREAD_BASE64_H
#include <stdio.h>
// the "u" prefix designates "base64url" functions, as oppposed to "base64"

// = encoders
// stream -> stream
void be64ss(FILE *dst, FILE *src);
void ube64ss(FILE *dst, FILE *src);
// buffer -> stream
void be64bs(FILE *dst, const char *src, size_t n);
void ube64bs(FILE *dst, const char *src, size_t n);

// = decoders
// stream -> stream
void bd64ss(FILE *dst, FILE *src);
void ubd64ss(FILE *dst, FILE *src);
// buffer -> stream
void bd64bs(FILE *dst, const char *src, size_t n);
void ubd64bs(FILE *dst, const char *src, size_t n);
#endif // BREAD_BASE64_H

#else // BREAD_BASE64_IMPLEMENTATION
// implementation
#include <stdio.h>
#include <stdint.h>

static const char b64a[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};
static const char ub64a[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'
};

// encode a 64-based chunk, this is the workhorse of the encoders
static inline int abe64c(char dst[static 4], uint32_t chunk, size_t n, const char alph[static 64]) {
	switch (n) {
		case 0:
			break;
		case 1:
			chunk <<= 8;
			dst[0] = alph[chunk >> 18 & 63];
			dst[1] = alph[chunk >> 12 & 63];
			dst[2] = '=';
			dst[3] = '=';
			break;
		case 2:
			dst[0] = alph[chunk >> 18 & 63];
			dst[1] = alph[chunk >> 12 & 63];
			dst[2] = alph[chunk >> 6  & 63];
			dst[3] = '=';
			break;
		default:
			dst[0] = alph[chunk >> 18 & 63];
			dst[1] = alph[chunk >> 12 & 63];
			dst[2] = alph[chunk >> 6  & 63];
			dst[3] = alph[chunk       & 63];
			return 3;
	}
	return n;
}

static inline int abe64cs(FILE *dst,
						  char buf[static 4],
						  uint32_t chunk,
						  size_t n,
						  const char alph[static 64]) {
	int o = abe64c(buf, chunk, n, alph);
	fprintf(dst, "%c%c%c%c", buf[0], buf[1], buf[2], buf[3]);
	return o;
}

// encode stream with alphabet
inline static void abe64ss(FILE *dst, FILE *src, const char alph[static 64]) {
	uint32_t buf = 0;
	char obuf[4];
	int c, r = 0;
	for (;;) {
		c = fgetc(src);
		if (c == EOF && feof(src)) break;
		if (r == 3) {
			r -= abe64cs(dst, obuf, buf, r, alph);
			buf &= 0;
		}
		buf |= c; r++;
		if (r != 3) buf <<= 8;
	}
	abe64cs(dst, obuf, buf, r, alph);
}

void be64ss(FILE *dst, FILE *src) {
	return abe64ss(dst, src, b64a);
}

void ube64ss(FILE *dst, FILE *src) {
	return abe64ss(dst, src, ub64a);
}

// encode buffer with alphabet
inline static void abe64bs(FILE *dst, const char *src, size_t n, const char alph[static 64]) {
	uint32_t buf = 0;
	char obuf[4];
	int r = 0;
	while (n) {
		if (r >= n) break;
		if (r == 3) {
			n -= abe64cs(dst, obuf, buf, r, alph);
			r = 0; buf &= 0;
		}
		buf |= *src++; r++;
		if (r != 3) buf <<= 8;
	}
	abe64cs(dst, obuf, buf, r, alph);
}

void be64bs(FILE *dst, const char *src, size_t n) {
	return abe64bs(dst, src, n, b64a);
}

void ube64bs(FILE *dst, const char *src, size_t n) {
	return abe64bs(dst, src, n, ub64a);
}
#endif // BREAD_BASE64_IMPLEMENTATION
