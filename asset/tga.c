#include "tga.h"
#include "../buf/buffer.h"

static int
u16at(const unsigned char *p, int off)
{
	return (int)p[off] | ((int)p[off + 1] << 8);
}

int
tga_Width(const void *data)
{
	if (data == 0)
		return 0;
	return u16at((const unsigned char *)data, 12);
}

int
tga_Height(const void *data)
{
	if (data == 0)
		return 0;
	return u16at((const unsigned char *)data, 14);
}

int
tga_Probe(const void *data, long len)
{
	const unsigned char *h;
	int type;
	int depth;

	if (data == 0 || len < 18)
		return 0;
	h = (const unsigned char *)data;
	type = h[2];
	depth = h[16];

	if (h[1] != 0)			/* a palette we will not unpack	*/
		return 0;
	if (type != 2 && type != 10 && type != 3 && type != 11)
		return 0;
	if (type == 2 || type == 10) {
		if (depth != 24 && depth != 32)
			return 0;
	} else {
		if (depth != 8)
			return 0;
	}
	if (tga_Width(data) <= 0 || tga_Height(data) <= 0)
		return 0;
	return 1;
}

unsigned long long
tga_Geo(const void *data)
{
	if (!tga_Probe(data, 18))
		return 0;
	return bufNewGeom(4, tga_Width(data), tga_Height(data));
}

long
tga_CanvasBytes(const void *data)
{
	return (long)tga_Width(data) * (long)tga_Height(data) * 4;
}

/*  One pixel, written where it belongs after the origin bit is honoured.
 *  Everything above this function sees a top-down RGBA image.
 */
static void
put(unsigned char *dst, int w, int h, int top_down, long i,
    int r, int g, int b, int a)
{
	long x;
	long y;
	unsigned char *p;

	x = i % w;
	y = i / w;
	if (!top_down)
		y = h - 1 - y;
	p = dst + (y * (long)w + x) * 4;
	p[0] = (unsigned char)r;
	p[1] = (unsigned char)g;
	p[2] = (unsigned char)b;
	p[3] = (unsigned char)a;
}

int
tga_Decode(void *dstv, long dstcap, const void *datav, long len)
{
	const unsigned char *h;
	const unsigned char *p;
	const unsigned char *end;
	unsigned char *dst;
	int w;
	int h_pix;
	int type;
	int depth;
	int bytes;
	int top_down;
	int grey;
	long pixels;
	long i;
	int count;
	int j;
	int header;

	if (dstv == 0 || datav == 0)
		return -1;
	if (!tga_Probe(datav, len))
		return -1;

	h = (const unsigned char *)datav;
	dst = (unsigned char *)dstv;
	w = tga_Width(datav);
	h_pix = tga_Height(datav);
	type = h[2];
	depth = h[16];
	bytes = depth / 8;
	top_down = (h[17] & 0x20) != 0;
	grey = (type == 3 || type == 11);
	pixels = (long)w * (long)h_pix;

	if (dstcap < pixels * 4)
		return -1;

	p = h + 18 + h[0];
	end = h + len;
	if (p > end)
		return -1;

	i = 0;
	if (type == 2 || type == 3) {
		if (end - p < pixels * bytes)
			return -1;
		while (i < pixels) {
			if (grey)
				put(dst, w, h_pix, top_down, i, p[0], p[0],
				    p[0], 255);
			else
				put(dst, w, h_pix, top_down, i, p[2], p[1],
				    p[0], bytes == 4 ? p[3] : 255);
			p += bytes;
			i++;
		}
		return 0;
	}

	/*  RLE: one header byte, then either one element to repeat or a run
	 *  of literals.  The same packet format bops.c already knows about;
	 *  it is spelled out here because the destination is scattered by
	 *  the origin flip and a flat decode would need a second pass.
	 */
	while (i < pixels) {
		if (p >= end)
			return -1;
		header = *p++;
		count = (header & 0x7F) + 1;
		if (i + count > pixels)
			return -1;

		if (header & 0x80) {
			if (end - p < bytes)
				return -1;
			for (j = 0; j < count; j++) {
				if (grey)
					put(dst, w, h_pix, top_down, i,
					    p[0], p[0], p[0], 255);
				else
					put(dst, w, h_pix, top_down, i,
					    p[2], p[1], p[0],
					    bytes == 4 ? p[3] : 255);
				i++;
			}
			p += bytes;
		} else {
			if (end - p < (long)count * bytes)
				return -1;
			for (j = 0; j < count; j++) {
				if (grey)
					put(dst, w, h_pix, top_down, i,
					    p[0], p[0], p[0], 255);
				else
					put(dst, w, h_pix, top_down, i,
					    p[2], p[1], p[0],
					    bytes == 4 ? p[3] : 255);
				p += bytes;
				i++;
			}
		}
	}
	return 0;
}
