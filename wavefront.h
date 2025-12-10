/*-
 * SPDX-License-Identifier: BSD-0-Clause
 *
 * Copyright (c) 2024
 *	Potr Dervyshev.  All rights reserved.
 *	@(#)wavefront.h	1.0 (Potr Dervyshev) 11/05/2024
 */
 
#ifndef WAVEFRONT_H_SENTRY
#define WAVEFRONT_H_SENTRY

/*------------------------------------------------- 
	#        1.MATH HELPERS    #
------------------------------------------------- */

enum {X = 0, Y = 1, Z = 2};

typedef float vector[3];

#define VEC_ABS(v) ( sqrtf((v)[X]*(v)[X] + (v)[Y]*(v)[Y] + (v)[Z]*(v)[Z]) )

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

/*------------------------------------------------- 
	#        2.WAVEFRONT     #
------------------------------------------------- */

#define VERTEX(objptr,n,coord) ((objptr)->vertex[(n)][(coord)])
#define TEXTURE(objptr,n,coord) ((objptr)->texture[(n)][(coord)])
#define NORMAL(objptr,n,coord) ((objptr)->normal[(n)][(coord)])
#define FACE(objptr,n) (((objptr)->face)[(n)])
/*	VERTEX - get coordinate of "n" vertex in "objptr" obj
	TEXTURE - get texture coordinate
	NORMAL - get normal-vector coordinates
	FACE - get face (polygon) by his number n	*/

#define COPY_POINT(obj,v,vector) do {\
		(vector)[X] = VERTEX((obj), (v) - 1, X);\
		(vector)[Y] = VERTEX((obj), (v) - 1, Y);\
		(vector)[Z] = VERTEX((obj), (v) - 1, Z);\
	}while(0)
	
#define COPY_TEXTURE(obj,vt,vector) do {\
		(vector)[X] = TEXTURE((obj), (vt) - 1, X);\
		(vector)[Y] = TEXTURE((obj), (vt) - 1, Y);\
		(vector)[Z] = TEXTURE((obj), (vt) - 1, Z);\
	}while(0)

#define COPY_NORMAL(obj,vn,vector) do {\
		(vector)[X] = NORMAL((obj), (vn) - 1, X);\
		(vector)[Y] = NORMAL((obj), (vn) - 1, Y);\
		(vector)[Z] = NORMAL((obj), (vn) - 1, Z);\
	}while(0)

typedef struct polygon {
	int v;	//0 - error; v > 0 <=> VERTEX(obj, obj->face->v - 1, X)
	int vt; //can be zero
	int vn; //can be zero
	struct polygon *next;
} polygon_t;

typedef struct {
	float **vertex;
	float **texture; //(optional)
	float **normal; //(optional)
	polygon_t **face;
} wf_wavefront_t;

//FUNCTIONS
wf_wavefront_t *wf_LoadWavefront(char *filename);
wf_wavefront_t *wf_LoadMemoryWavefront(const char *buffer);
void wf_RemoveWavefront(wf_wavefront_t *obj);
void wf_WavefrontPrintLog(wf_wavefront_t *obj);
void wf_WavefrontCalculateNormals(wf_wavefront_t *obj);
void wf_TurnWavefront(wf_wavefront_t *obj, float alpha, float beta, float gamma);
void wf_MoveWavefront(wf_wavefront_t *obj, float dx, float dy, float dz);
void wf_ScaleWavefront(wf_wavefront_t *obj, float multipler);
void wf_MoveVertex(wf_wavefront_t *obj, int id, float dx, float dy, float dz);
void wf_SetVertex(wf_wavefront_t *obj, int id, float x, float y, float z);

#endif
