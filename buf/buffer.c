#include "buffer.h"

#define BUF_MAX_UNIT	15
#define BUF_MAX_COLS	32767
#define BUF_MAX_ROWS	32767
#define BUF_MAX_STR	16383
#define BUF_MIN_STR	(-16384)

/*  One step of a field, for changing geo with plain addition:
 *  geo += BUF_ONE_ROW
 */
#define BUF_ONE_COL	(1ULL << 49)
#define BUF_ONE_ROW	(1ULL << 34)
#define BUF_ONE_STRX	(1ULL << 19)
#define BUF_ONE_STRY	(1ULL << 4)

unsigned long long bufNewGeomFull(long unit, long cols, long rows, long strx, long stry){
	if (unit < 1 || unit > BUF_MAX_UNIT)
		return 0;
	if (cols < 1 || cols > BUF_MAX_COLS)
		return 0;
	if (rows < 1 || rows > BUF_MAX_ROWS)
		return 0;
	if (strx < BUF_MIN_STR || strx > BUF_MAX_STR)
		return 0;
	if (stry < BUF_MIN_STR || stry > BUF_MAX_STR)
		return 0;
	return ((unsigned long long)cols << 49)
	     | ((unsigned long long)rows << 34)
	     | (((unsigned long long)strx & 0x7FFF) << 19)
	     | (((unsigned long long)stry & 0x7FFF) << 4)
	     | (unsigned long long)unit;
}

unsigned long long bufNewGeomPitch(long unit, long cols, long rows, long pitch) {
	if (unit < 1 || cols < 1)
		return 0;
	if (pitch < cols * unit)
		return 0;		/* rows would overlap each other */
	return bufNewGeomFull(unit, cols, rows, unit, pitch);
}

unsigned long long bufNewGeom(long unit, long cols, long rows){
	if (unit < 1 || cols < 1)
		return 0;
	return bufNewGeomPitch(unit, cols, rows, cols * unit);
}

int bufGeomCheck(unsigned long long geo){
	if (BUF_UNIT(geo) < 1)
		return 0;
	if (BUF_COLS(geo) < 1)
		return 0;
	if (BUF_ROWS(geo) < 1)
		return 0;
	return 1;
}

long bufGeomGetUnits(unsigned long long geo){
	if (!bufGeomCheck(geo))
		return 0;
	return BUF_COLS(geo) * BUF_ROWS(geo);
}

long bufGeomGetStart(unsigned long long geo){
	long farx, fary, low;
	if (!bufGeomCheck(geo))
		return 0;
	farx = (BUF_COLS(geo) - 1) * BUF_STRX(geo);
	fary = (BUF_ROWS(geo) - 1) * BUF_STRY(geo);
	low = 0;
	if (farx < 0)
		low += farx;
	if (fary < 0)
		low += fary;
	return low;
}

long bufGeomGetBytes(unsigned long long geo){
	long farx, fary, low, high;

	if (!bufGeomCheck(geo))
		return 0;
	farx = (BUF_COLS(geo) - 1) * BUF_STRX(geo);
	fary = (BUF_ROWS(geo) - 1) * BUF_STRY(geo);

	low = 0;
	high = 0;
	if (farx < 0)
		low += farx;
	else
		high += farx;
	if (fary < 0)
		low += fary;
	else
		high += fary;

	return high - low + BUF_UNIT(geo);
}

int bufGeomIsPacked(unsigned long long geo){
	if (!bufGeomCheck(geo))
		return 0;
	if (BUF_STRX(geo) != BUF_UNIT(geo))
		return 0;
	if (BUF_STRY(geo) != BUF_COLS(geo) * BUF_UNIT(geo))
		return 0;
	return 1;
}

int bufGeomIsFits(void *buf, unsigned long long geo, void *mem, long size){
	char *low, *high;

	if (buf == 0 || mem == 0 || !bufGeomCheck(geo))
		return 0;
	low = (char *)buf + bufGeomGetStart(geo);
	high = low + bufGeomGetBytes(geo);

	if (low < (char *)mem)
		return 0;
	if (high > (char *)mem + size)
		return 0;
	return 1;
}

void *
buf_at(void *buf, unsigned long long geo, long x, long y)
{
	return (char *)buf + x * BUF_STRX(geo) + y * BUF_STRY(geo);
}

void *
buf_row(void *buf, unsigned long long geo, long y)
{
	return (char *)buf + y * BUF_STRY(geo);
}

void *
buf_clamp(void *buf, unsigned long long geo, long x, long y)
{
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x > BUF_COLS(geo) - 1)
		x = BUF_COLS(geo) - 1;
	if (y > BUF_ROWS(geo) - 1)
		y = BUF_ROWS(geo) - 1;
	return buf_at(buf, geo, x, y);
}

/*  CHANGING THE VIEW  */

unsigned long long
buf_swap_xy(unsigned long long geo)
{
	if (!bufGeomCheck(geo))
		return 0;
	return bufNewGeomFull(BUF_UNIT(geo), BUF_ROWS(geo), BUF_COLS(geo),
	                    BUF_STRY(geo), BUF_STRX(geo));
}

unsigned long long
buf_step_x(unsigned long long geo, long n)
{
	long cols;

	if (!bufGeomCheck(geo) || n < 1)
		return 0;
	cols = (BUF_COLS(geo) + n - 1) / n;
	return bufNewGeomFull(BUF_UNIT(geo), cols, BUF_ROWS(geo),
	                    BUF_STRX(geo) * n, BUF_STRY(geo));
}

unsigned long long
buf_step_y(unsigned long long geo, long n)
{
	long rows;

	if (!bufGeomCheck(geo) || n < 1)
		return 0;
	rows = (BUF_ROWS(geo) + n - 1) / n;
	return bufNewGeomFull(BUF_UNIT(geo), BUF_COLS(geo), rows,
	                    BUF_STRX(geo), BUF_STRY(geo) * n);
}

unsigned long long
buf_crop(unsigned long long geo, long cols, long rows)
{
	if (!bufGeomCheck(geo))
		return 0;
	if (cols < 1 || cols > BUF_COLS(geo))
		return 0;	/* a view cannot invent units */
	if (rows < 1 || rows > BUF_ROWS(geo))
		return 0;
	return bufNewGeomFull(BUF_UNIT(geo), cols, rows,
	                    BUF_STRX(geo), BUF_STRY(geo));
}

void *
buf_window(void *buf, unsigned long long *geo, long x, long y,
           long cols, long rows)
{
	unsigned long long g;

	if (buf == 0 || geo == 0 || !bufGeomCheck(*geo))
		return 0;
	if (x < 0 || y < 0)
		return 0;
	if (x + cols > BUF_COLS(*geo) || y + rows > BUF_ROWS(*geo))
		return 0;

	g = buf_crop(*geo, cols, rows);
	if (g == 0)
		return 0;
	buf = buf_at(buf, g, x, y);
	*geo = g;
	return buf;
}

void *
buf_lane(void *buf, unsigned long long *geo, long at, long bytes)
{
	unsigned long long g;

	if (buf == 0 || geo == 0 || !bufGeomCheck(*geo))
		return 0;
	if (at < 0 || bytes < 1 || at + bytes > BUF_UNIT(*geo))
		return 0;

	/* the unit shrinks, the step between units stays what it was */
	g = bufNewGeomFull(bytes, BUF_COLS(*geo), BUF_ROWS(*geo),
	                 BUF_STRX(*geo), BUF_STRY(*geo));
	if (g == 0)
		return 0;
	*geo = g;
	return (char *)buf + at;
}

void *
buf_flip_x(void *buf, unsigned long long *geo)
{
	unsigned long long g;
	long strx;

	if (buf == 0 || geo == 0 || !bufGeomCheck(*geo))
		return 0;
	strx = BUF_STRX(*geo);
	g = bufNewGeomFull(BUF_UNIT(*geo), BUF_COLS(*geo), BUF_ROWS(*geo),
	                 -strx, BUF_STRY(*geo));
	if (g == 0)
		return 0;
	buf = (char *)buf + (BUF_COLS(*geo) - 1) * strx;
	*geo = g;
	return buf;
}

void *
buf_flip_y(void *buf, unsigned long long *geo)
{
	unsigned long long g;
	long stry;

	if (buf == 0 || geo == 0 || !bufGeomCheck(*geo))
		return 0;
	stry = BUF_STRY(*geo);
	g = bufNewGeomFull(BUF_UNIT(*geo), BUF_COLS(*geo), BUF_ROWS(*geo),
	                 BUF_STRX(*geo), -stry);
	if (g == 0)
		return 0;
	buf = (char *)buf + (BUF_ROWS(*geo) - 1) * stry;
	*geo = g;
	return buf;
}

void buf_copy(void *dst, void *src, long n){
	char *d;
	char *s;

	d = (char *)dst;
	s = (char *)src;
	while (n > 0) {
		*d = *s;
		d++;
		s++;
		n--;
	}
}

void
buf_move(void *dst, void *src, long n)
{
	char *d;
	char *s;

	d = (char *)dst;
	s = (char *)src;
	if (d == s || n < 1)
		return;
	if (d < s) {
		buf_copy(dst, src, n);
		return;
	}
	d += n;
	s += n;
	while (n > 0) {
		d--;
		s--;
		*d = *s;
		n--;
	}
}

void
buf_fill(void *dst, int byte, long n)
{
	char *d;

	d = (char *)dst;
	while (n > 0) {
		*d = (char)byte;
		d++;
		n--;
	}
}

int
buf_same(void *a, void *b, long n)
{
	char *p;
	char *q;

	p = (char *)a;
	q = (char *)b;
	while (n > 0) {
		if (*p != *q)
			return 0;
		p++;
		q++;
		n--;
	}
	return 1;
}

/*  MOVING BYTES BY SHAPE  */

int
buf_blit(void *dst, unsigned long long dgeo, void *src,
         unsigned long long sgeo)
{
	long cols, rows, unit, dstrx, sstrx, x, y;
	char *d;
	char *s;

	if (dst == 0 || src == 0)
		return -1;
	if (!bufGeomCheck(dgeo) || !bufGeomCheck(sgeo))
		return -1;

	cols = BUF_COLS(dgeo);
	if (BUF_COLS(sgeo) < cols)
		cols = BUF_COLS(sgeo);
	rows = BUF_ROWS(dgeo);
	if (BUF_ROWS(sgeo) < rows)
		rows = BUF_ROWS(sgeo);
	unit = BUF_UNIT(dgeo);
	if (BUF_UNIT(sgeo) < unit)
		unit = BUF_UNIT(sgeo);

	dstrx = BUF_STRX(dgeo);
	sstrx = BUF_STRX(sgeo);

	for (y = 0; y < rows; y++) {
		d = (char *)buf_row(dst, dgeo, y);
		s = (char *)buf_row(src, sgeo, y);

		/* both rows lie flat: one run instead of cols small ones */
		if (dstrx == unit && sstrx == unit) {
			buf_move(d, s, cols * unit);
			continue;
		}
		for (x = 0; x < cols; x++) {
			buf_move(d, s, unit);
			d += dstrx;
			s += sstrx;
		}
	}
	return 0;
}

int
buf_set(void *buf, unsigned long long geo, void *unit)
{
	long cols, rows, size, strx, x, y;
	char *d;

	if (buf == 0 || unit == 0 || !bufGeomCheck(geo))
		return -1;

	cols = BUF_COLS(geo);
	rows = BUF_ROWS(geo);
	size = BUF_UNIT(geo);
	strx = BUF_STRX(geo);

	for (y = 0; y < rows; y++) {
		d = (char *)buf_row(buf, geo, y);
		for (x = 0; x < cols; x++) {
			buf_copy(d, unit, size);
			d += strx;
		}
	}
	return 0;
}

/*  WALKING  */

int
buf_each(void *buf, unsigned long long geo,
         int (*fn)(void *unit, long x, long y, void *arg), void *arg)
{
	long cols, rows, strx, x, y;
	char *p;
	int rc;

	if (buf == 0 || fn == 0 || !bufGeomCheck(geo))
		return -1;

	cols = BUF_COLS(geo);
	rows = BUF_ROWS(geo);
	strx = BUF_STRX(geo);

	for (y = 0; y < rows; y++) {
		p = (char *)buf_row(buf, geo, y);
		for (x = 0; x < cols; x++) {
			rc = fn(p, x, y, arg);
			if (rc != 0)
				return rc;
			p += strx;
		}
	}
	return 0;
}

int
buf_map(void *dst, unsigned long long dgeo, void *src,
        unsigned long long sgeo,
        int (*fn)(void *dst, void *src, long x, long y, void *arg),
        void *arg)
{
	long cols, rows, dstrx, sstrx, x, y;
	char *d;
	char *s;
	int rc;

	if (dst == 0 || src == 0 || fn == 0)
		return -1;
	if (!bufGeomCheck(dgeo) || !bufGeomCheck(sgeo))
		return -1;

	cols = BUF_COLS(dgeo);
	if (BUF_COLS(sgeo) < cols)
		cols = BUF_COLS(sgeo);
	rows = BUF_ROWS(dgeo);
	if (BUF_ROWS(sgeo) < rows)
		rows = BUF_ROWS(sgeo);

	dstrx = BUF_STRX(dgeo);
	sstrx = BUF_STRX(sgeo);

	for (y = 0; y < rows; y++) {
		d = (char *)buf_row(dst, dgeo, y);
		s = (char *)buf_row(src, sgeo, y);
		for (x = 0; x < cols; x++) {
			rc = fn(d, s, x, y, arg);
			if (rc != 0)
				return rc;
			d += dstrx;
			s += sstrx;
		}
	}
	return 0;
}

/*  If you link with -nostdlib the compiler may still want these four.
 *  Build with -DBUF_LIBC_MEM and it gets them.
 */
#ifdef BUF_LIBC_MEM
void *
memcpy(void *dst, const void *src, unsigned long n)
{
	buf_copy(dst, (void *)src, (long)n);
	return dst;
}

void *
memmove(void *dst, const void *src, unsigned long n)
{
	buf_move(dst, (void *)src, (long)n);
	return dst;
}

void *
memset(void *dst, int c, unsigned long n)
{
	buf_fill(dst, c, (long)n);
	return dst;
}

int
memcmp(const void *a, const void *b, unsigned long n)
{
	unsigned char *p;
	unsigned char *q;

	p = (unsigned char *)a;
	q = (unsigned char *)b;
	while (n > 0) {
		if (*p != *q)
			return (int)*p - (int)*q;
		p++;
		q++;
		n--;
	}
	return 0;
}
#endif
