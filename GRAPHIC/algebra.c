/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024
 *	Potr Dervyshev.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)algebra.h	1.0 (Potr Dervyshev) 11/05/2024
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "algebra.h"

#ifndef _FIXED_POINT
int TrimLineFindX(int x0, int y0, int x1, int y1, int y){
	int dy = y1 - y0;
	if(dy == 0) {
		return x0;
	};
	int dx = x1 - x0;
	int delta_y = y - y0;
	int new_x = x0 + (int)( (float)(dx * delta_y) / dy );
	return new_x;
};

int TrimLineFindY(int x0, int y0, int x1, int y1, int x){
	int dx = x1 - x0;
	if(dx == 0) {
		return y0;
	};
	int dy = y1 - y0;
	int delta_x = x - x0;
	int new_y = y0 + (int)( (float)(dy * delta_x) / dx );
	return new_y;
};
#endif

#ifdef _FIXED_POINT
int TrimLineFindX(int x0, int y0, int x1, int y1, int y){
	int dy = y1 - y0;
	if(dy == 0) {
		return x0;
	};
	int dx = x1 - x0;
	int delta_y = y - y0;
	int new_x = x0 + FIXED_TO_INT( div(INT_TO_FIXED(dx * delta_y), INT_TO_FIXED(dy)) );
	return new_x;
};

int TrimLineFindY(int x0, int y0, int x1, int y1, int x){
	int dx = x1 - x0;
	if(dx == 0) {
		return y0;
	};
	int dy = y1 - y0;
	int delta_x = x - x0;
	int new_y = y0 + FIXED_TO_INT( div(INT_TO_FIXED(dy * delta_x), INT_TO_FIXED(dx)) );
	return new_y;
};
#endif

float TrimPlaneFindZ(vector p0, vector p1, vector p2, float x, float y){
	float a,b,c,d;
	a = (p1[Y] - p0[Y])*(p2[Z] - p0[Z]) - (p2[Y] - p0[Y])*(p1[Z] - p0[Z]);
	b = (p1[X] - p0[X])*(p2[Z] - p0[Z]) - (p2[X] - p0[X])*(p1[Z] - p0[Z]);
	c = (p1[X] - p0[X])*(p2[Y] - p0[Y]) - (p2[X] - p0[X])*(p1[Y] - p0[Y]);
	if(c == 0){
		return 0;
	}
	d = (b * p0[Y]) - (a * p0[X])	- (c * p0[Z]);
	float z = 0;
	z	= (((-1) * d) + (b * y) - (a * x))/c;
	return z;
};
