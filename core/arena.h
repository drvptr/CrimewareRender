/*
 *	@(#)arena.h	1.0
 *
 *  The engine never calls malloc().  It also never calls open() or read().
 *  You hand it one block of memory that you own - from malloc, from mmap,
 *  from a static array, from xxd -i - and every loader carves what it needs
 *  out of that block, in order, never freeing individual pieces.
 *
 *  This is not a limitation dressed up as a virtue.  A level loads once and
 *  dies once; a per-object free list buys nothing and costs fragmentation.
 *  When the level ends you reset the whole arena in one instruction.
 *
 *	static char pool[64 * 1024 * 1024];
 *	struct arena a;
 *	arena_Init(&a, pool, sizeof pool);
 *	...
 *	long mark = arena_Mark(&a);	// remember where we are
 *	load_level(&a);			// allocates freely
 *	arena_Reset(&a, mark);		// the level is gone
 */
#ifndef ARENA_H_SENTRY
#define ARENA_H_SENTRY

struct arena {
	char *base;
	long size;
	long used;
	long peak;
	int failed;	/* set once an allocation did not fit */
};

void arena_Init(struct arena *a, void *mem, long size);
void *arena_Alloc(struct arena *a, long bytes, long align);
/* USAGE:
	float *v = arena_Alloc(&a, 3 * n * sizeof(float), 4);
   RETURN VALUE:
	the block, zeroed, or 0 if it does not fit.  A failed arena stays
	failed, so you can do a whole load and check once at the end.
*/
long arena_Mark(struct arena *a);
void arena_Reset(struct arena *a, long mark);
long arena_Left(struct arena *a);

#endif /* ARENA_H_SENTRY */
