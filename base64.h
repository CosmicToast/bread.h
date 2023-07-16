// https://spacetoast.dev/tech.html
// = header
#ifndef BREAD_BASE64_H
#define BREAD_BASE64_H
#include <stdio.h>
// the "u" prefix designates "base64url" functions, as oppposed to "base64"

#define EBB64AL 129 // input is not in the alphabet
#define EBB64DE 130 // input contains a padding error

// = encoders
// stream -> stream
size_t be64ss(FILE *dst, FILE *src);
size_t ube64ss(FILE *dst, FILE *src);
// buffer -> stream
size_t be64bs(FILE *dst, const char *src, size_t n);
size_t ube64bs(FILE *dst, const char *src, size_t n);

// = decoders
// stream -> stream
size_t bd64ss(FILE *dst, FILE *src);
size_t ubd64ss(FILE *dst, FILE *src);
// buffer -> stream
size_t bd64bs(FILE *dst, const char *src, size_t n);
size_t ubd64bs(FILE *dst, const char *src, size_t n);
#endif // BREAD_BASE64_H

// = implementation
#ifdef BREAD_BASE64_IMPLEMENTATION
#define EBB64AL 129
#define EBB64DE 130

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const char b64a[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', 0
};
static const char ub64a[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_', 0
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

// this allows the library to be used with arbitrary alphabets, but is slower
// essentially, we iterate over the alphabet looking for v
// we presume "=" is always the padding character
static inline char idxof(char v, const char alph[static 64]) {
	if (v == '=') return -1;
	const char *ptr = alph;
	while (*ptr++ != v) {
		if (!*ptr) return -2;
	}
	return --ptr - alph;
}

// decode a 64-based chunk, this is the workhorse of the encoders
// returns how many bytes you can read (starting from 0)
static inline size_t abd64c(char dst[static 3], const char _src[static 4], const char alph[static 64]) {
	// if you wanted to make the ss family faster, you could remove this and the const qualifier
	char src[4]; memcpy(src, _src, 4);
	src[0] = idxof(src[0], alph);
	src[1] = idxof(src[1], alph);
	src[2] = idxof(src[2], alph);
	src[3] = idxof(src[3], alph);
	if (src[0] == -2 || src[1] == -2 || src[2] == -2 || src[3] == -2) {
		errno = EBB64AL;
		return 0;
	}
	if (src[0] == -1 || src[1] == -1) {
		errno = EBB64DE;
		return 0;
	}
	if (src[2] == -1 && src[3] != -1) {
		errno = EBB64DE;
		return 0;
	}

	// this chunk now holds all 3 of our potential bytes from bit -24 to -0
	uint32_t chunk = ((src[0] & 63) << 18) |
					 ((src[1] & 63) << 12) |
					 ((src[2] & 63) <<  6) |
					 ((src[3] & 63)      );
	dst[0] = chunk >> 16 & 0xff;
	if (src[2] == -1) {
		return 1;
	}
	dst[1] = chunk >> 8 & 0xff;
	if (src[3] == -1) {
		return 2;
	}
	dst[2] = chunk & 0xff;
	return 3;
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
inline static size_t abe64ss(FILE *dst, FILE *src, const char alph[static 64]) {
	size_t proc = 0;
	uint32_t buf = 0;
	char obuf[4];
	int c, r = 0;
	for (;;) {
		c = fgetc(src);
		if (c == EOF && feof(src)) break;
		proc++;
		if (r == 3) {
			r -= abe64cs(dst, obuf, buf, r, alph);
			buf &= 0;
		}
		buf |= c; r++;
		if (r != 3) buf <<= 8;
	}
	abe64cs(dst, obuf, buf, r, alph);
	return proc;
}

size_t be64ss(FILE *dst, FILE *src) {
	return abe64ss(dst, src, b64a);
}

size_t ube64ss(FILE *dst, FILE *src) {
	return abe64ss(dst, src, ub64a);
}

// encode buffer with alphabet
inline static size_t abe64bs(FILE *dst, const char *src, size_t n, const char alph[static 64]) {
	size_t proc = 0;
	uint32_t buf = 0;
	char obuf[4];
	int r = 0;
	while (n) {
		if (r >= n) break;
		if (r == 3) {
			n -= abe64cs(dst, obuf, buf, r, alph);
			r = 0; buf &= 0;
		}
		buf |= *src++; r++; proc++;
		if (r != 3) buf <<= 8;
	}
	abe64cs(dst, obuf, buf, r, alph);
	return proc;
}

size_t be64bs(FILE *dst, const char *src, size_t n) {
	return abe64bs(dst, src, n, b64a);
}

size_t ube64bs(FILE *dst, const char *src, size_t n) {
	return abe64bs(dst, src, n, ub64a);
}

// decode stream with alphabet
inline static size_t abd64ss(FILE *dst, FILE *src, const char alph[static 64]) {
	size_t proc = 0;
	char buf[4];
	char out[3];
	int r;
	while ((r = fread(buf, 4, 1, src))) {
		r = abd64c(out, buf, alph);
		if (!r) {
			// note that this is not portable, ISO C99 only guarantees
			// one byte of ungetc
			// if this is a concern, use the buffered flavor
			ungetc(buf[3], src); ungetc(buf[2], src);
			ungetc(buf[1], src); ungetc(buf[0], src);
			break;
		}
		proc += 4;
		switch (r) {
		case 1: // final byte with double = padding
			fprintf(dst, "%c", out[0]);
			return proc;
		case 2: // final byte with single = padding
			fprintf(dst, "%c%c", out[0], out[1]);
			return proc;
		case 3: // normal case
			fprintf(dst, "%c%c%c", out[0], out[1], out[2]);
			break;
		}
	}
	return proc;
}

// decode buffer with alphabet
inline static size_t abd64bs(FILE *dst, const char *src, size_t n, const char alph[static 64]) {
	size_t proc = 0;
	size_t ptr = 0;
	char out[3];
	int r;
	// note that there may be extra data in the buffer
	while ((n - ptr) / 4) {
		r = abd64c(out, src + ptr, alph);
		if (!r) return proc;
		proc += 4;
		ptr += 4;
		switch (r) {
			case 1: // final byte with double = padding
				fprintf(dst, "%c", out[0]);
				return proc;
			case 2: // final byte with single = padding
				fprintf(dst, "%c%c", out[0], out[1]);
				return proc;
			case 3: // normal case
				fprintf(dst, "%c%c%c", out[0], out[1], out[2]);
				break;
		}
	}
	return proc;
}

size_t bd64ss(FILE *dst, FILE *src) {
	return abd64ss(dst, src, b64a);
}
size_t ubd64ss(FILE *dst, FILE *src) {
	return abd64ss(dst, src, ub64a);
}

size_t bd64bs(FILE *dst, const char *src, size_t n) {
	return abd64bs(dst, src, n, b64a);
}

size_t ubd64bs(FILE *dst, const char *src, size_t n) {
	return abd64bs(dst, src, n, ub64a);
}
#endif // BREAD_BASE64_IMPLEMENTATION
