/*
 *	@(#)tga.h	1.0
 *
 *  TGA in, RGBA out.  The engine has exactly one pixel format above the
 *  loader, so nothing downstream has to ask what a picture used to be.
 *
 *  There is no open() in here.  You pass bytes: from read(), from mmap(),
 *  or from a header made by "xxd -i wall.tga > wall.h".
 *
 *	unsigned long long g = tga_Geo(wall_tga);
 *	void *pix = arena_Alloc(&a, tga_CanvasBytes(wall_tga), 4);
 *	tga_Decode(pix, tga_CanvasBytes(wall_tga), wall_tga, wall_tga_len);
 *	unsigned int tex = gfx_MakeTexture(pix, g, 1, 1);
 *
 *  Supported: type 2 and 10 (truecolour, raw and RLE) at 24 or 32 bpp,
 *  type 3 and 11 (greyscale) at 8 bpp, both origins.  Palettes are not
 *  supported and are reported as an error rather than guessed at.
 */
#ifndef TGA_H_SENTRY
#define TGA_H_SENTRY

int tga_Probe(const void *data, long len);
/* RETURN VALUE: 1 if the header is one we can decode, 0 otherwise. */

unsigned long long tga_Geo(const void *data);
/* RETURN VALUE: a packed buf geometry, unit 4 (RGBA), rows top down.
 *		 0 if the size does not fit a buf geometry - which happens
 *		 past 4095 pixels wide, because a row stride is 15 bits.
 */

long tga_CanvasBytes(const void *data);
int tga_Decode(void *dst, long dstcap, const void *data, long len);
/* RETURN VALUE: 0 on success, -1 on a malformed or unsupported file. */

int tga_Width(const void *data);
int tga_Height(const void *data);

#endif /* TGA_H_SENTRY */
