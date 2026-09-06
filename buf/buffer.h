/*-
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Potr Dervyshev.  All rights reserved.
 *
 *	@(#)buffer.h	5.0 (Potr Dervyshev) 08/29/2026
 */

#ifndef BUF_H_SENTRY
#define BUF_H_SENTRY

typedef char
IF_LONG_LONG_IS_NOT_64_BITS_THEN_YOU_GET_A_COMPILATION_ERROR
[(sizeof(unsigned long long) == 8) ? 1 : -1];

/*********************************************************************
 *  BUFFER:                                                          *
 *   void *buf  //a flat run of bytes that you own                   *
 *                                                                   *
 *  GEOMETRY:                                                        *
 *   unsigned long long geo  // "type" that interprets "buf"         *
 *                                                                   *
 *   +----------+----------+------------+------------+-------+       *
 *   |  cols    |  rows    |   strx     |   stry     | unit  |       *
 *   |   15     |   15     |    15      |    15      |  4    |       *
 *   +----------+----------+------------+------------+-------+       *
 *   63      49 48      34 33        19 18         4 3     0         *
 *                                                                   *
 *   unit  -  bytes in one unit, 1..15                               *
 *   cols  -  units along axis 0 (x), 1..32767                       *
 *   rows  -  units along axis 1 (y), 1..32767.  rows == 1 is 1D     *
 *   strx  -  BYTES from one unit to the next along x, signed        *
 *   stry  -  BYTES from one row to the next, signed, -16384..16383  *
 *********************************************************************/


/************************************
 * DEFINITIONS, GETTERS AND SETTERS *
 ************************************/

#define BUF_COLS(g)  ((long)(((g) >> 49) & 0x7FFF))
#define BUF_ROWS(g)  ((long)(((g) >> 34) & 0x7FFF))
#define BUF_STRX(g)  ((long)(((long long)((g) << 30)) >> 49))
#define BUF_STRY(g)  ((long)(((long long)((g) << 45)) >> 49))
#define BUF_UNIT(g)  ((long)((g) & 0xF))

unsigned long long bufNewGeom(long unit, long cols, long rows);
/* USAGE:
   unsigned long long g = bufNewGeom(4,640,860);
*/

unsigned long long bufNewGeomPitch(long unit, long cols, long rows, long pitch);

unsigned long long bufNewGeomFull(long unit, long cols, long rows, long strx, long stry);

int  bufGeomCheck(unsigned long long geo);
/* USAGE:
   if( bufGeomCheck(g) ){ ...all right... }
*/
long bufGeomGetUnits(unsigned long long geo); /* cols x rows */
long bufGeomGetBytes(unsigned long long geo);
long bufGeomGetStart(unsigned long long geo); /* <= 0, nonzero after a flip */
int  bufGeomIsPacked(unsigned long long geo);/* contiguous, no gaps, no flips */
int  bufGeomIsFits(void *buf, unsigned long long geo, void *mem, long size);
/*  1 if the geometry cannot reach outside the memory you own.  The one
 *  check worth making, because the compiler is not doing it for you.
 */

/************************************
 *          FINDING A UNIT          *
 ************************************/
void *buf_at(void *buf, unsigned long long geo, long x, long y);
void *buf_row(void *buf, unsigned long long geo, long y);
void *buf_clamp(void *buf, unsigned long long geo, long x, long y);
/*  buf_clamp() pins x and y to the edges, which is what blurs want  */

/************************************
 *          CHANGING THE VIEW       *
 ************************************/
unsigned long long buf_swap_xy(unsigned long long geo);	/* transpose	*/
unsigned long long buf_step_x(unsigned long long geo, long n);
unsigned long long buf_step_y(unsigned long long geo, long n);
unsigned long long buf_crop(unsigned long long geo, long cols, long rows);

/*  These three also move the origin, so they take geo by pointer and give
 *  the new base back.  One C function cannot return both.
 */
void *buf_window(void *buf, unsigned long long *geo,
                 long x, long y, long cols, long rows);
void *buf_lane(void *buf, unsigned long long *geo, long at, long bytes);
/*  buf_lane(p, &g, 2, 1) - the red byte of every BGRA unit, in place  */
void *buf_flip_x(void *buf, unsigned long long *geo);
void *buf_flip_y(void *buf, unsigned long long *geo);

/*  BYTES.  There is no libc under this, so these live here.  Build with
 *  -ffreestanding -fno-builtin, or the compiler may call memcpy anyway.
 */
void buf_copy(void *dst, void *src, long n);	/* no overlap	*/
void buf_move(void *dst, void *src, long n);	/* overlap ok	*/
void buf_fill(void *dst, int byte, long n);
int  buf_same(void *a, void *b, long n);

/*  MOVING BYTES BY SHAPE - the only two that do  */

int buf_blit(void *dst, unsigned long long dgeo,
             void *src, unsigned long long sgeo);
/*  copies the overlap of the two rectangles, min(unit) bytes per unit.
 *  Every view above is just numbers, so this one function is also the
 *  crop, the transpose, the flip, the channel split and the subsample.
 *  Overlapping buffers are safe only when dst <= src.
 *  Returns 0, or -1 on a bad argument.
 */

int buf_set(void *buf, unsigned long long geo, void *unit);
/*  writes one unit-sized pattern into every unit  */

/*  WALKING.  The callback returns 0 to go on; anything else stops the walk
 *  and comes back out of buf_each(), so find / check all / stop on error
 *  are one mechanism.  A fold is buf_each() with the total in arg.
 */

int buf_each(void *buf, unsigned long long geo,
             int (*fn)(void *unit, long x, long y, void *arg), void *arg);

int buf_map(void *dst, unsigned long long dgeo,
            void *src, unsigned long long sgeo,
            int (*fn)(void *dst, void *src, long x, long y, void *arg),
            void *arg);
/*  unit sizes may differ: 4 bytes in, 1 byte out is the usual case  */

#endif
