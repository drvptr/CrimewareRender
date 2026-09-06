/*
 *	@(#)plat_null.c	1.0
 *
 *  No window, no sound, no input.  Time still runs, so the fixed step loop
 *  behaves exactly as it does on a real machine.  This is what the tests
 *  link against, and what a dedicated server would use.
 */
#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include "plat.h"

static double time_base;
static int fake_w = 640;
static int fake_h = 480;

static double
now_seconds(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int
plat_Init(void)
{
	time_base = now_seconds();
	return 1;
}

void
plat_Shutdown(void)
{
}

int
plat_OpenWindow(const char *title, int width, int height)
{
	(void)title;
	fake_w = width;
	fake_h = height;
	return 1;
}

void
plat_CloseWindow(void)
{
}

void
plat_Poll(struct plat_input *in)
{
	int i;

	for (i = 0; i < PLAT_KEYS; i++) {
		in->hit[i] = 0;
		in->rel[i] = 0;
	}
	in->mouse_dx = 0;
	in->mouse_dy = 0;
	in->width = fake_w;
	in->height = fake_h;
	in->resized = 0;
}

void
plat_Swap(void)
{
}

void
plat_GrabMouse(int on)
{
	(void)on;
}

double
plat_Time(void)
{
	return now_seconds() - time_base;
}

void
plat_Sleep(double seconds)
{
	struct timespec ts;

	if (seconds <= 0.0)
		return;
	ts.tv_sec = (long)seconds;
	ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
	nanosleep(&ts, 0);
}

void *
plat_GlProc(const char *name)
{
	(void)name;
	return 0;
}

int
plat_AudioOpen(int rate, int channels)
{
	(void)rate;
	(void)channels;
	return 0;
}

int
plat_AudioSpace(void)
{
	return 0;
}

int
plat_AudioWrite(const short *pcm, int frames)
{
	(void)pcm;
	(void)frames;
	return frames;
}

void
plat_AudioClose(void)
{
}
