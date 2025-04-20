// https://spacetoast.dev/tech.html
// = header
#ifndef BREAD_ARR_H
#define BREAD_ARR_H
#include <stddef.h>

/* bread.h dynamic arrays
 * `struct barr` is a dynamic array implementation based on the vlist algorithm.
 * That means it's a reverse upward-growing N2O linked list of geometrically
 * growing chunks.
 *
 * Initialize an array using `barr_new(0)`.
 * If you specify a number > 0, the required amount of chunks to store that many
 * elements will be preallocated (and zeroed out if BARR_MEMSET is set).
 * You can force an existing array to grow in the same way whenever you want
 * using `barr_ensure(v, new_size)`. Note that if `new_size < size`, no actions
 * will be taken.
 * You can shrink an existing array by calling `barr_pop(v)` repeatedly for now.
 * You can also grow an existing array by calling `barr_push(v, val)`.
 * You can get the number of items in the array via `barr_size(v)`, or simply
 * reading `v->size` directly.
 * You can mutate existing elements using `barr_set(v, idx, val)`.
 *
 * If you're curious, this is a variation of the VArray, with these changes:
 * * offset is not per-node, but only on the head, applying to the newest bucket
 * * total size is tracked as a size_t in the head
 * * the items are allocated as flexible array members to save on allocations
 */

/* You can change the typedef to anything, but you'd have to change how
 * error reporting is done if you make it something other than a pointer.
 */
typedef void* barr_item;

struct barr_node {
	struct barr_node *next;
	size_t size;
	barr_item items[];
};

struct barr {
	struct barr_node *base;
	size_t size, offset;
};

// Some implementation notes in case you want to dig into the brains :)
/* Relationship between barr.offset and barr_node.size:
 * offset == size    : empty bucket
 * offset == 0       : full bucket
 * 0 < offset < size : bucket with elements
 */
/* Why does barr.size exist?
 * Higher size reservations in malloc(3p) are more likely to fail.
 * If we just keep geometrically growing, we're being probabilistically optimal,
 * but it's risky. If you're on a system that might refuse large malloc() calls,
 * you will want to set BARR_MAXSIZE. However, if you set BARR_MAXSIZE,
 * it becomes impossible to compute the total size of the array from the size
 * of the newest bucket. This can be solved either by iterating, or by keeping
 * a cache in head, which is what we do: a size_t per array isn't the end
 * of the world.
 */

struct barr *barr_new(size_t size);
barr_item barr_get(struct barr *arr, size_t idx);
barr_item barr_pop(struct barr *arr);
barr_item barr_set(struct barr *arr, size_t idx, barr_item val);
barr_item barr_push(struct barr *arr, barr_item val);
size_t barr_size(struct barr *arr);
size_t barr_ensure(struct barr *arr, size_t size);

#endif // BREAD_ARR_H

#ifdef BREAD_ARR_IMPLEMENTATION

// BARR_MAXSIZE is optional: if set to n, never reserve > n slots in a chunk
// BARR_MEMSET  is optional: if set, it will be called to clear memory on ensure

// Growth Factor: the chunks will be of size GF^n where n is the chunk number
#ifndef BARR_GF
#define BARR_GF 4
#endif

// You can change how memory is allocated. I'd recommend mimalloc :)
#if !defined(BARR_MALLOC) || !defined(BARR_FREE)
#include <stdlib.h>
#endif

#ifndef BARR_MALLOC
#define BARR_MALLOC malloc
#endif

#ifndef BARR_FREE
#define BARR_FREE free
#endif

struct barr *barr_new(size_t size) {
	struct barr *arr = BARR_MALLOC(sizeof(struct barr));
	arr->base   = NULL;
	arr->size   = 0;
	arr->offset = 0;
	if (size) barr_ensure(arr, size); // WARN: no error reporting
	return arr;
}

static size_t barr_grow(struct barr *arr) {
	size_t l1 = arr->base ? arr->base->size : 1u;
	size_t l2;
#ifdef BARR_MAXSIZE
	if (BARR_MAXSIZE / BARR_GF > l1)
		l2 = BARR_MAXSIZE;
	else
#endif
	{ l2 = BARR_GF * l1; } // WARN: the {}s are a footgun safety measure
	struct barr_node *node =
		BARR_MALLOC(sizeof(struct barr) + sizeof(barr_item) * l2);
	if (!node) return 0;
	node->size  = l2;
	node->next  = arr->base;
	arr->base   = node;
	arr->offset = l2;
	return l2;
}

static barr_item *barr_addr(struct barr *arr, size_t idx) {
	if (idx >= arr->size) return NULL;

	// we're seeking from the end, so real idx = size - idx
	// if idx == 0, then we want +(size-1)th element
	// if idx == size-1, we want the final one
	// idx = arr->size - idx - 1;

	// the current bucket has `size - offset` elements
	// let's say size=10 and offset=8: there are 2 elements
	// if idx is 12, then we will want to -= 2 and move to the next bucket
	// the easiest way to handle this generically is to += 8 (20)
	// before falling back on the default case (-= 10)
	// idx += arr->offset;

	// the following takes into account all of the above in a single assignment
	idx = arr->size + arr->offset - idx - 1;

	struct barr_node *node = arr->base;
	while (idx >= node->size) {
		if (!node) return NULL;
		idx -= node->size;
		node = node->next;
	}
	return node->items + idx;
}

barr_item barr_get(struct barr *arr, size_t idx) {
	barr_item  *res = barr_addr(arr, idx);
	if (res) return *res;
	return NULL;
}

barr_item barr_set(struct barr *arr, size_t idx, barr_item val) {
	barr_item *res = barr_addr(arr, idx);
	if (res)  *res = val;
	return NULL;
}

barr_item barr_pop(struct barr *arr) {
	if (!arr->base) return NULL;

	if (arr->offset < arr->base->size) {
		return arr->base->items[arr->offset++];
	}

	// the bucket is empty and offset == size
	struct barr_node *old = arr->base;
	barr_item popped = old->items[old->size-1];
	arr->base = old->next;
	arr->offset = 0;
	arr->size--;
	BARR_FREE(old);
	return popped;
}

barr_item barr_push(struct barr *arr, barr_item val) {
	if (!arr->base || !arr->offset) {
		if (!barr_grow(arr)) return NULL;
		return barr_push(arr, val);
	}
	arr->base->items[--arr->offset] = val;
	arr->size++;
	return val;
}

size_t barr_size(struct barr *arr) {
	return arr->size;
}

size_t barr_ensure(struct barr *arr, size_t size) {
	while (arr->size < size) {
		if (arr->offset) { // fill up current bucket if it isn't already
			arr->size  += arr->offset;
#ifdef BARR_MEMSET
			BARR_MEMSET(arr->base, 0, sizeof(barr_item) * arr->offset);
#endif
			arr->offset = 0;
		} else { // else make a new bucket to fill up
			barr_grow(arr);
		}
	}

	if (arr->size > size) { // roll back and set offset if we went too far
		arr->offset = arr->size - size;
		arr->size = size;
	}
	return arr->size;
}

// TODO: barr_iterate? it's non-trivial to write and I don't need it
// it'd be mostly there as an optimization

#endif // BREAD_ARR_IMPLEMENTATION
