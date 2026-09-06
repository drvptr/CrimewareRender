#include "arena.h"

void
arena_Init(struct arena *a, void *mem, long size)
{
	a->base = (char *)mem;
	a->size = size;
	a->used = 0;
	a->peak = 0;
	a->failed = 0;
}

void *
arena_Alloc(struct arena *a, long bytes, long align)
{
	long start;
	long i;
	char *p;

	if (a == 0 || a->base == 0 || bytes < 0)
		return 0;
	if (align < 1)
		align = 1;

	start = a->used;
	while (start % align != 0)
		start++;

	if (start + bytes > a->size) {
		a->failed = 1;
		return 0;
	}

	p = a->base + start;
	for (i = 0; i < bytes; i++)
		p[i] = 0;

	a->used = start + bytes;
	if (a->used > a->peak)
		a->peak = a->used;
	return p;
}

long
arena_Mark(struct arena *a)
{
	return a->used;
}

void
arena_Reset(struct arena *a, long mark)
{
	if (mark < 0 || mark > a->used)
		return;
	a->used = mark;
}

long
arena_Left(struct arena *a)
{
	return a->size - a->used;
}
