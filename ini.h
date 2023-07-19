// https://spacetoast.dev/tech.html
// = header
#ifndef BREAD_INI_H
#define BREAD_INI_H
#include <stdio.h>

/* bread.h ini parser
 * A lenient and error-correcting INI parser.
 * It will parse a given file handle, recover recoverable errors, and stop on irrecoverable errors.
 * It will call the provided callback (including userdata) for every key-value pair, while mentioning the section.
 * Note that the section may be NULL.
 *
 * The data is loaded into static internal memory and must be copied if it is to be used outside of the callback.
 * The sizes default to 64 bytes for sections, the length of a section for keys, and 16 times the length of a key for the values.
 * You can set any given one of them by defining BINI_{SEC,KEY,VAL}_MAXLEN before the implementation include.
 * You may define BINI_MALLOC and BINI_FREE to a 3p-compatible malloc and free for the working buffers to be heap allocated.
 *
 * The error correction features are:
 * * if a section is not terminated before the end of the line, it is considered terminated
 * * if a key is not terminated before the end of the line, the newline is considered to be an =
 * * if an error is encountered while reading the value, the value is preserved
 * * if any string doesn't fit into the working buffer, we continue parsing, but the value you receive is truncated
 *
 * The limitations include:
 * * keys, values, and sections cannot contain "\n"
 * * keys cannot contain "="
 * * keys and values cannot contain leading or trailing whitespace
 * * comments may not be present on the same line as a key-value pair (but may be present after a section)
 *
 * Both ";" and "#" are recognized as comment characters.
 *
 * The implementation may be modified to achieve different design goals.
 */

// Returns the number of bytes, including truncated data, but not including stripped whitespace.
// If cb returns non-zero, parsing stops.
int bread_parse_ini(FILE *src, void *userdata,
		int (*cb)(const char *section, const char *key, const char *value, void *userdata));

#endif // BREAD_INI_H

#ifdef BREAD_INI_IMPLEMENTATION

// maximum lengths for the static buffers
#ifndef BINI_SEC_MAXLEN
#define BINI_SEC_MAXLEN 64
#endif
#ifndef BINI_KEY_MAXLEN
#define BINI_KEY_MAXLEN BINI_SEC_MAXLEN
#endif
#ifndef BINI_VAL_MAXLEN
#define BINI_VAL_MAXLEN BINI_KEY_MAXLEN * 16
#endif

#include <stdbool.h>
#include <string.h>

typedef int (*callback)(const char*, const char*, const char*, void*);

// == general utilities
// scan c from the right until not in s, set final ptr to 0
// returns new strlen
static inline int stripright(char *c, const char *s) {
	ssize_t len = strlen(c); // strlen doesn't include terminating 0
	if (!len) return len; // it's already empty
	while ((--len) >= 0 && strchr(s, c[len])) {}
	// either strchr failed or len is now -1
	if (len < 0) {
		*c = 0;
		return 0;
	}
	c[++len] = 0;
	return len;
}

// == character classes
const char wss[] = " \t\r\n";

// == parsing utilities
// skip as long as the condition holds
// does not consume the character after
static int parse_skipwhile(FILE *src, const char *s) {
	int c, out = 0;
	while ((c = fgetc(src)) != EOF) {
		if (!strchr(s, c)) {
			ungetc(c, src);
			return out;
		}
		out++;
	}
	// hit error
	return ferror(src) ? -out : out;
}

// skip until hitting a delimiter
// consumes the delimiter
static int parse_skipuntil(FILE *src, const char *s) {
	int c, out = 0;
	while ((c = fgetc(src)) != EOF) {
		if (strchr(s, c)) {
			return out;
		}
		out++;
	}
	// hit error
	return ferror(src) ? -out : out;
}

#define parse_skipws(src) parse_skipwhile(src, wss)

// parses into ptr as long as getc is in s and maxlen holds
// if maxlen is exhausted, continue by skipping
// does not consume the character that follows
// note that we do not actually use this,
// but you may want this in case you're writing a different parser with this as the base
static int parse_while(FILE *src, char *ptr, ssize_t maxlen, const char *s) {
	int out = 0;
	while (out < maxlen) {
		*ptr = fgetc(src);
		// hit eof while scanning
		if (*ptr == EOF) {
			*ptr = 0;
			return ferror(src) ? -out : out;
		} else if (!strchr(s, *ptr)) {
			ungetc(*ptr, src);
			*ptr = 0;
			return out;
		}
		ptr++; out++;
	}
	// we hit maxlen
	(*--ptr) = 0;
	int skipped = parse_skipwhile(src, s);
	if (skipped > 0) {
		return out + skipped;
	}
	return ferror(src) ? (skipped - out) : (out - skipped);
}

// parses into ptr until getc is in s and maxlen holds
// if maxlen is exhausted, continue by skipping
// consumes terminator
static int parse_until(FILE *src, char *ptr, ssize_t maxlen, const char *s) {
	int out = 0;
	while (out < maxlen) {
		*ptr = fgetc(src);
		// hit eof while scanning
		if (*ptr == EOF) {
			*ptr = 0;
			return ferror(src) ? -out : out;
		} else if (strchr(s, *ptr)) {
			*ptr = 0;
			return out;
		}
		ptr++; out++;
	}
	// we hit maxlen
	(*--ptr) = 0;
	int skipped = parse_skipuntil(src, s);
	if (skipped > 0) {
		return out + skipped;
	}
	return ferror(src) ? (skipped - out) : (out - skipped);
}

// == parsers
static inline int parse_section(FILE *src, char *section) {
	// if a section line is not terminated, "helpfully" insert the ] automatically
	return parse_until(src, section, BINI_SEC_MAXLEN, "]\n");
}

static inline int parse_key(FILE *src, char *key) {
	int out = parse_until(src, key, BINI_KEY_MAXLEN, "=\n");
	return    stripright(key, wss);
}

static inline int parse_value(FILE *src, char *value) {
	int out = parse_until(src, value, BINI_VAL_MAXLEN, "\n");
	return    stripright(value, wss);
}

static int parse_kv(FILE *src, void *userdata,
		const char *section, char *key, char *value, callback cb) {
	int len = 0, tmp;

	tmp = parse_key(src, key); // consumes the =, removes trailing ws
	// key must have a value, may not error, may not eof
	if (tmp <= 0 || feof(src)) return 0;
	len += tmp;

	tmp = parse_skipws(src); // skip whitespace after =
	// may not error, may be empty, may not eof
	if (tmp < 0 || feof(src)) return 0;
	len += tmp;

	tmp = parse_value(src, value);
	len += tmp > 0 ? tmp : -tmp; // errors are fine as long as we finished parsing

	if (cb(section, key, value, userdata)) len *= -1; // cb requested error
	return len;
}

static int parse_expr(FILE *src, void *userdata,
		char *section, char *key, char *value, callback cb) {
	int len = parse_skipws(src);
	if (len) return len;

	int c;
	switch ((c = fgetc(src))) {
		case EOF:
			return 0;
		case '[':
			// section, we want to skip over the [
			return parse_section(src, section);
		case '#':
		case ';':
			// comment, we don't care about the comment character
			return parse_skipuntil(src, "\n");
		default:
			// a key-value pair
			ungetc(c, src);
			return parse_kv(src, userdata, section, key, value, cb);
	}
	return 0;
}

int bread_parse_ini(FILE *src, void *userdata, callback cb) {
#if defined(BINI_MALLOC)
	char *section = BINI_MALLOC(BINI_SEC_MAXLEN);
	char *key     = BINI_MALLOC(BINI_KEY_MAXLEN);
	char *value   = BINI_MALLOC(BINI_VAL_MAXLEN);
	*section = 0; *key = 0; *value = 0;
#else
	char section[BINI_SEC_MAXLEN] = {0};
	char key[BINI_KEY_MAXLEN]     = {0};
	char value[BINI_VAL_MAXLEN]   = {0};
#endif

	int status, out = 0;
	// as long as we're consuming output...
	while ((status = parse_expr(src, userdata, section, key, value, cb)) >= 0) {
		out += status;
		if (feof(src) || ferror(src)) break;
	}

#if defined(BINI_MALLOC) && defined(BINI_FREE)
	BINI_FREE(section);
	BINI_FREE(key);
	BINI_FREE(value);
#endif
	return ferror(src) ? -out : out;
}
#endif // BREAD_INI_IMPLEMENTATION
