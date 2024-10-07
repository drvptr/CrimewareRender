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
 *	@(#)render3d.h	1.0 (Potr Dervyshev) 11/05/2024
 */
#ifndef RENDER3D_H_SENTRY
#define RENDER3D_H_SENTRY

#include "../IO/io.h"
#include "algebra.h"
#include "wavefront.h"
#include "basics.h"
#include "tgatool.h"

typedef struct camera_t{
	vector pos;
	vector target;
	vector dir; //x_aix
	vector y_aix;
	vector z_aix;
	fixed **zbuffer;
	int buf_refill_required;
	float fov;
	int w;
	int h;
	int hw;
	int hh;
} camera;

/* 1. CAMERA METHODS */
camera *InitCamera(window *w,int x0,int y0,int z0,int x1,int y1,int z1,int fov);
void MoveCamera(camera *cam, vector new_pos, vector new_target);
int PerspectiveProjection(vector p, camera *cam, int *x, int *y, fixed *z);
void FreeCamera(camera *cam);

/*2. RENDERERS */
void RenderZBuffer(window *w, camera *cam,wavefront_obj *obj, int max_depth);
void RenderWireframe(window *w, camera * c, wavefront_obj *obj, int color);
void RenderShaded(window *w, camera *cam, wavefront_obj *obj, int color);
void RenderTextured(window *w, camera *cam, wavefront_obj *obj, TGAimage *texture);
void RenderGouraud(window *w, camera *cam, wavefront_obj *obj,
				TGAimage *texture, int default_color);
#endif
