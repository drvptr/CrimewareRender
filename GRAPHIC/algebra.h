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
 *	@(#)algebra.c	1.0 (Potr Dervyshev) 11/05/2024
 */
#ifndef ALGEBRA_H_SENTRY
#define ALGEBRA_H_SENTRY

/*	1. FIXED-POINT IMPLEMENTATION	*/
typedef int fixed;
#define N 4
#define INF 2147483647
//INF = 2^(8*sizeof(fixed) - 1 - N)

#define mul(a,b) ( ((a)*(b)) >> N )
#define div(a,b) (((a) << N)/(b))
//#define FIXED_TO_FLOAT(f) ( (float)(f) * pow(2,(-N)) )
//#define FLOAT_TO_FIXED(f) ( (fixed)((f) * pow(2,(N))) ) 
/*pre-calculated*/
#define FIXED_TO_FLOAT(f) ( (fixed)((f) * 0.0625f) )
#define FLOAT_TO_FIXED(f) ( (fixed)((f) * 16) )
#define FIXED_TO_INT(f) ( (f) >> N )
#define INT_TO_FIXED(f) ( (f) << N )
#define ABS(number) ( ((number) >= 0)?(number):(-(number)) )
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
/*	2.a VECTOR IMPLEMENTATION (float)	*/
enum {X = 0, Y = 1, Z = 2}; //coordinate names. Example: vec[X]

typedef float vector[3];

#define VEC_ABS(v) ( sqrtf((v)[X]*(v)[X] + (v)[Y]*(v)[Y] + (v)[Z]*(v)[Z]) )

#define VEC_ASSIGMENT(v,u) do{\
				(u)[X] = (v)[X];\
				(u)[Y] = (v)[Y];\
				(u)[Z] = (v)[Z]; } while(0)

static inline void vec_add(vector a, vector b, vector c){
	c[X] = a[X] + b[X];
	c[Y] = a[Y] + b[Y];
	c[Z] = a[Z] + b[Z];
};

static inline void vec_sub(vector a, vector b, vector c){
	c[X] = a[X] - b[X];
	c[Y] = a[Y] - b[Y];
	c[Z] = a[Z] - b[Z];
};

static inline void vec_scalar_mul(vector v, float k, vector u){
	u[X] = v[X] * k;
	u[Y] = v[Y] * k;
	u[Z] = v[Z] * k;
};

static inline void vec_cross(vector a, vector b, vector c){
	c[X] = a[Y]*b[Z] - a[Z]*b[Y];
	c[Y] = a[Z]*b[X] - a[X]*b[Z];
	c[Z] = a[X]*b[Y] - a[Y]*b[X];
};

static inline float vec_dot(vector a, vector b){
	return a[X]*b[X] + a[Y]*b[Y] + a[Z]*b[Z];
};

static inline void vec_normalize(vector v){
	float l  = VEC_ABS(v);
	if(l != 0){
		v[X] = v[X] / l;
		v[Y] = v[Y] / l;
		v[Z] = v[Z] / l;
	};
};

/*	3. SOLVERS AND UTILITIES	*/

int TrimLineFindX(int x0, int y0, int x1, int y1, int y);
int TrimLineFindY(int x0, int y0, int x1, int y1, int x);
float TrimPlaneFindZ(vector p0, vector p1, vector p2, float x, float y);

static inline void swap_xy(int *x, int *y){
	int old_x = *x;
	*x = *y;
	*y = old_x;
};

/*https://en.wikipedia.org/wiki/Barycentric_coordinate_system*/
static inline void Barycentric(int x, int y, vector p0, vector p1, vector p2,
			 float *lambda0, float *lambda1, float *lambda2){
	float denominator = (p1[Y] - p2[Y]) * (p0[X] - p2[X]) +
					 (p2[X] - p1[X]) * (p0[Y] - p2[Y]);
	if(denominator == 0)
		return;
	*lambda0 = ((p1[Y] - p2[Y]) * (x - p2[X]) +
				 (p2[X] - p1[X]) * (y - p2[Y])) / denominator;
	*lambda1 = ((p2[Y] - p0[Y]) * (x - p2[X]) + 
				(p0[X] - p2[X]) * (y - p2[Y])) / denominator;
	*lambda2 = 1.0f - *lambda0 - *lambda1;
}

#endif //SENTRY
