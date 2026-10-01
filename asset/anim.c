#include <string.h>

#include "anim.h"

static long
i32at(const unsigned char *p)
{
	return (long)p[0] | ((long)p[1] << 8) | ((long)p[2] << 16) |
	    ((long)p[3] << 24);
}

static float
f32at(const unsigned char *p)
{
	unsigned long u;
	float f;

	u = (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
	    ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
	memcpy(&f, &u, sizeof f);
	return f;
}

/*  Positions of vertex k in frame f of a 16 bit clip.  */
static void
packed_at(const struct anim *a, long f, int k, float out[3])
{
	const short *s;

	s = a->packed + (f * (long)a->nverts + (long)k) * 3;
	out[0] = a->offset[0] + a->scale[0] * (float)s[0];
	out[1] = a->offset[1] + a->scale[1] * (float)s[1];
	out[2] = a->offset[2] + a->scale[2] * (float)s[2];
}

int
anim_Parse(struct anim *out, const void *data, long len)
{
	const unsigned char *p;
	long need;
	int i;

	if (out == 0 || data == 0 || len < 24)
		return -1;
	p = (const unsigned char *)data;
	if (p[0] != 'V' || p[1] != 'A' || p[2] != 'N' || p[3] != '1')
		return -1;

	out->nverts = (int)i32at(p + 4);
	out->nframes = (int)i32at(p + 8);
	out->fps = (int)i32at(p + 12);
	out->has_normals = (int)(i32at(p + 16) & 1);
	out->stride = out->has_normals ? 6 : 3;
	out->frames = 0;
	out->packed = 0;

	if (out->nverts <= 0 || out->nframes <= 0 || out->fps <= 0)
		return -1;

	if (i32at(p + 16) & 2) {
		if (out->has_normals || len < 48)
			return -1;
		for (i = 0; i < 3; i++) {
			out->scale[i] = f32at(p + 24 + i * 4);
			out->offset[i] = f32at(p + 36 + i * 4);
		}
		need = 48 + (long)out->nframes * (long)out->nverts * 3 * 2;
		if (need > len)
			return -1;
		out->packed = (const short *)(const void *)(p + 48);
		return 0;
	}

	need = 24 + (long)out->nframes * (long)out->nverts *
	    (long)out->stride * 4;
	if (need > len)
		return -1;

	out->frames = (const float *)(const void *)(p + 24);
	return 0;
}

float
anim_Length(const struct anim *a)
{
	if (a == 0 || a->fps <= 0)
		return 0.0f;
	return (float)a->nframes / (float)a->fps;
}

void
anim_Sample(const struct anim *a, float seconds, int loop,
    struct gfx_vertex *dst, int nverts, const int *source)
{
	int from;
	float position;
	int frame_a;
	int frame_b;
	float t;
	const float *pa;
	const float *pb;
	float qa[3];
	float qb[3];
	int i;
	long off_a;
	long off_b;

	if (a == 0 || dst == 0 || (a->frames == 0 && a->packed == 0))
		return;
	if (source == 0 && nverts > a->nverts)
		nverts = a->nverts;

	position = seconds * (float)a->fps;
	frame_a = (int)position;
	t = position - (float)frame_a;

	if (loop) {
		frame_a = frame_a % a->nframes;
		if (frame_a < 0)
			frame_a += a->nframes;
		frame_b = (frame_a + 1) % a->nframes;
	} else {
		if (frame_a >= a->nframes - 1) {
			frame_a = a->nframes - 1;
			frame_b = frame_a;
			t = 0.0f;
		} else {
			frame_b = frame_a + 1;
		}
		if (frame_a < 0) {
			frame_a = 0;
			frame_b = 0;
			t = 0.0f;
		}
	}

	if (a->packed != 0) {
		for (i = 0; i < nverts; i++) {
			from = source != 0 ? source[i] : i;
			if (from < 0 || from >= a->nverts)
				continue;
			packed_at(a, frame_a, from, qa);
			packed_at(a, frame_b, from, qb);
			dst[i].x = qa[0] + (qb[0] - qa[0]) * t;
			dst[i].y = qa[1] + (qb[1] - qa[1]) * t;
			dst[i].z = qa[2] + (qb[2] - qa[2]) * t;
		}
		return;
	}

	off_a = (long)frame_a * (long)a->nverts * (long)a->stride;
	off_b = (long)frame_b * (long)a->nverts * (long)a->stride;

	for (i = 0; i < nverts; i++) {
		from = source != 0 ? source[i] : i;
		if (from < 0 || from >= a->nverts)
			continue;
		pa = a->frames + off_a + (long)from * a->stride;
		pb = a->frames + off_b + (long)from * a->stride;

		dst[i].x = pa[0] + (pb[0] - pa[0]) * t;
		dst[i].y = pa[1] + (pb[1] - pa[1]) * t;
		dst[i].z = pa[2] + (pb[2] - pa[2]) * t;

		if (a->has_normals) {
			dst[i].nx = pa[3] + (pb[3] - pa[3]) * t;
			dst[i].ny = pa[4] + (pb[4] - pa[4]) * t;
			dst[i].nz = pa[5] + (pb[5] - pa[5]) * t;
		}
	}
}
