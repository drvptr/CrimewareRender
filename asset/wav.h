/*
 *	@(#)wav.h	1.0
 *
 *  RIFF/WAVE, PCM only, 8 or 16 bit, mono or stereo.  A 16 bit file on a
 *  little endian machine is not copied at all: the samples are already in
 *  the layout the mixer wants, so wav_Parse() just points at them inside
 *  your bytes.  8 bit files are widened into the arena.
 */
#ifndef WAV_H_SENTRY
#define WAV_H_SENTRY

#include "../core/arena.h"

struct wav {
	const short *pcm;	/* interleaved	*/
	long frames;
	int rate;
	int channels;
};

int wav_Parse(struct wav *out, const void *data, long len, struct arena *a);
/* RETURN VALUE: 0 on success, -1 if it is not a PCM WAVE we can use. */

#endif /* WAV_H_SENTRY */
