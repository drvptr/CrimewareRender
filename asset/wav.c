#include "wav.h"

static long
u32at(const unsigned char *p)
{
	return (long)p[0] | ((long)p[1] << 8) | ((long)p[2] << 16) |
	    ((long)p[3] << 24);
}

static int
u16at(const unsigned char *p)
{
	return (int)p[0] | ((int)p[1] << 8);
}

static int
tag_is(const unsigned char *p, const char *tag)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (p[i] != (unsigned char)tag[i])
			return 0;
	}
	return 1;
}

int
wav_Parse(struct wav *out, const void *data, long len, struct arena *a)
{
	const unsigned char *p;
	const unsigned char *end;
	long chunk;
	int format;
	int bits;
	int channels;
	int rate;
	const unsigned char *pcm;
	long pcm_bytes;
	short *wide;
	long i;
	long samples;

	if (out == 0 || data == 0 || len < 44)
		return -1;

	p = (const unsigned char *)data;
	end = p + len;
	if (!tag_is(p, "RIFF") || !tag_is(p + 8, "WAVE"))
		return -1;

	format = 0;
	bits = 0;
	channels = 0;
	rate = 0;
	pcm = 0;
	pcm_bytes = 0;

	p += 12;
	while (p + 8 <= end) {
		chunk = u32at(p + 4);
		if (chunk < 0 || p + 8 + chunk > end)
			chunk = end - (p + 8);

		if (tag_is(p, "fmt ") && chunk >= 16) {
			format = u16at(p + 8);
			channels = u16at(p + 10);
			rate = (int)u32at(p + 12);
			bits = u16at(p + 22);
		} else if (tag_is(p, "data")) {
			pcm = p + 8;
			pcm_bytes = chunk;
		}
		p += 8 + chunk;
		if (chunk % 2 != 0)
			p++;	/* chunks are word aligned */
	}

	if (format != 1 || pcm == 0 || rate <= 0)
		return -1;
	if (channels != 1 && channels != 2)
		return -1;

	out->rate = rate;
	out->channels = channels;

	if (bits == 16) {
		out->pcm = (const short *)(const void *)pcm;
		out->frames = pcm_bytes / (2 * channels);
		return 0;
	}
	if (bits == 8) {
		samples = pcm_bytes;
		wide = arena_Alloc(a, samples * (long)sizeof(short), 2);
		if (wide == 0)
			return -1;
		for (i = 0; i < samples; i++)
			wide[i] = (short)(((int)pcm[i] - 128) * 256);
		out->pcm = wide;
		out->frames = samples / channels;
		return 0;
	}
	return -1;
}
