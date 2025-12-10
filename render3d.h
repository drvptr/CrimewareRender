/*-
 * SPDX-License-Identifier: BSD-0-Clause
 *
 * Copyright (c) 2025
 *	Potr Dervyshev.  All rights reserved.
 *
 *	@(#)render3d.h	1.0 (Potr Dervyshev) 28/09/2025
 */
 
#ifndef RENDER3D_H_SENTRY
#define RENDER3D_H_SENTRY

#include "io.h"
#include "wavefront.h"

typedef struct r3_camera_inc_t r3_camera_t;

typedef void* r3_texture_t;

r3_camera_t *r3_InitCamera(io_window_t *w,int x0,int y0,int z0,int x1,int y1,int z1);
void r3_MoveCamera(r3_camera_t *cam, float dx, float dy, float dz);
void r3_RotateCameraY(r3_camera_t *cam, float angleRad);
void r3_RemoveCamera(r3_camera_t *cam);
void r3_RenderMesh(io_window_t *w, wf_wavefront_t *obj, r3_texture_t *tex);


#endif
