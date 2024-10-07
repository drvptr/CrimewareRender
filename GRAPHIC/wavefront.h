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
 *	@(#)wavefront.h	1.0 (Potr Dervyshev) 11/05/2024
 */
#ifndef WAVEFRONT_H_SENTRY
#define WAVEFRONT_H_SENTRY

#include "algebra.h" /*Takes from enum {X = 0, Y = 1, Z = 2}; VERTEX(obj,45,X)*/

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

typedef struct list {
	int v;	//0 - error; v > 0 <=> VERTEX(obj, obj->face->v - 1, X)
	int vt; //can be zero
	int vn; //can be zero
	struct list *next;
} polygon;

typedef struct {
	float **vertex;
	float **texture; //(optional)
	float **normal; //(optional)
	polygon **face;
} wavefront_obj;

//1. BASIC FUNCTIONS
wavefront_obj *ImportObj(char *filename);
wavefront_obj *ImportEmbedObj(unsigned char obj[], unsigned int len);
void FreeObj(wavefront_obj *obj);
void WavefrontPrintLog(wavefront_obj *obj);
void WavefrontCalculateNormals(wavefront_obj *obj);

//2. TRANSFORMATION PROCEDURES
void TurnObj(wavefront_obj *obj, float alpha, float beta, float gamma);
void MoveObj(wavefront_obj *obj, float dx, float dy, float dz);
void ScaleObj(wavefront_obj *obj, float multipler);

/*PICTURE 1.1: wavefront obj scheme
                                  +-------------+
                                _,.|wavefront_obj|-,,
                           ,.-'   +-------------+   '.,
                       -'``          |         |         `'-
                    +------+   +--------+   +-------+   +-----+
                    |vertex|   |textures|   |normals|   |faces|-----\
                    +------+   +--------+   +-------+   +-----+     |
                       |             |         |                    |
                       |          /--------------\                  |
                +--+   |          [same as vertex]                  |
                |*r|   |          \______________/                  |
  +--------+-+  | a|   |                                            V
  |rational|X|<-| t|   |               +-------------+             +--+
  +--------+-+  | i|   |             _.|int v = 1    |             |*p|
  |rational|Y|  | o|<--/          _-`  +-------------+             | o|
  +--------+-+  | n|           _-`     |polygon *next|<------------| l|
  |rational|Z|  | a|        _-`        +-------------+             | y|
  +--------+-+  | l|     _-`      +-------------+  |               | g|
                +--+  _-`         |int v = 2    |  |               | o|
                | 1|<`            +-------------+  |               | n|
                +--+              |polygon *next|<-/               +--+
                      .           +-------------+                  | 1|
                +--+  .      +-------------+  |                    +--+  .
                |*r| [v]     |int v = 3    |  |                          .
  +--------+-+  | a|         +-------------+  |  +-------------+   +--+  .
  |rational|X|<-| t|         |polygon *next|<-/  |int v  = 5   |   |*p|  [n]
  +--------+-+  | i|         +-------------+     +-------------+   | o|
  |rational|Y|  | o|    +-------------+  |       |polygon *next|<--|-l|
  +--------+-+  | n|    |int v ...    |  |       +-------------+   | y|
  |rational|Z|  | a|    +-------------+  |  +-------------+  |     | g|
  +--------+-+  | l|    |polygon *next|<-/  |int v = 2    |  |     | o|
                +--+    +-------------+     +-------------+  |     | n|
                | 2|                        |polygon *next|<-/     +--+
                +--+                        +-------------+        | 2|
                                                                   +--+
*/

#endif
