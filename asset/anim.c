#include "anim.h"

static long
i32at(const unsigned char *p)
{
	return (long)p[0] | ((long)p[1] << 8) | ((long)p[2] << 16) |
	    ((long)p[3] << 24);
}

int
anim_Parse(struct anim *out, const void *data, long len)
{
	const unsigned char *p;
	long need;

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

	if (out->nverts <= 0 || out->nframes <= 0 || out->fps <= 0)
		return -1;

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
	int i;
	long off_a;
	long off_b;

	if (a == 0 || dst == 0 || a->frames == 0)
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
