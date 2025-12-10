/*-
 * SPDX-License-Identifier: BSD-0-Clause
 *
 * Copyright (c) 2025
 *	Potr Dervyshev.  All rights reserved.
 *
 *	@(#)render3d_glx.c	1.0 (Potr Dervyshev) 28/09/2025
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include "render3d.h"

/*------------------------------------------------- 
	# Camera implemetntation #
------------------------------------------------- */

#define DEFAULT_FOV 45
#define DEFAULT_NEAR 1.0f
#define DEFAULT_FAR 5000

GLfloat DEFAULT_LIGHT_POS[] = { 0.0f, 0.0f, 1.0f, 0.0f };

struct r3_camera_inc_t {
    vector pos;
    vector target;
    vector up;
};

static void UpdateCamera(r3_camera_t *cam) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(
		cam->pos[X], cam->pos[Y], cam->pos[Z],
		cam->target[X], cam->target[Y], cam->target[Z],
		cam->up[X], cam->up[Y], cam->up[Z]
	);
	glLightfv(GL_LIGHT0, GL_POSITION, DEFAULT_LIGHT_POS);
}

r3_camera_t *r3_InitCamera(io_window_t *w,int x0,int y0,int z0,int x1,int y1,int z1){
	r3_camera_t *res = malloc(sizeof(r3_camera_t));
	res->pos[X] = x0; res->pos[Y] = y0; res->pos[Z] = z0;
	res->target[X] = x1; res->target[Y] = y1; res->target[Z] = z1;
	res->up[X] = 0; res->up[Y] = 1; res->up[Z] = 0;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(DEFAULT_FOV, (float)io_GetWidth(w)/io_GetHeight(w), DEFAULT_NEAR, DEFAULT_FAR);
	UpdateCamera(res);
	return res;
}

void r3_MoveCamera(r3_camera_t *cam, float dx, float dy, float dz) {
	cam->pos[X] += dx;
	cam->pos[Y] += dy;
	cam->pos[Z] += dz;
	cam->target[X] += dx;
	cam->target[Y] += dy;
	cam->target[Z] += dz;
	UpdateCamera(cam);
}

void r3_RotateCameraY(r3_camera_t *cam, float angleRad) {
	float dx = cam->pos[X] - cam->target[X];
	float dz = cam->pos[Z] - cam->target[Z];
	float newX = dx * cos(angleRad) - dz * sin(angleRad);
	float newZ = dx * sin(angleRad) + dz * cos(angleRad);
	cam->pos[X] = cam->target[X] + newX;
	cam->pos[Z] = cam->target[Z] + newZ;
	UpdateCamera(cam);
}

void r3_RemoveCamera(r3_camera_t *cam){
	if(cam != NULL)
		free(cam);
}

/*------------------------------------------------- 
	# Renders #
------------------------------------------------- */


GLfloat NO_TEXTURE[]  = {0.5f, 0.5f, 0.5f, 1.0f};

void r3_RenderMesh(io_window_t *w, wf_wavefront_t *obj, r3_texture_t *tex){
	glShadeModel(GL_SMOOTH);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	if(tex == NULL)
		glMaterialfv(GL_FRONT, GL_DIFFUSE, NO_TEXTURE);
	glEnable(GL_DEPTH_TEST);
	vector v0, v1, v2;
	vector vn0, vn1, vn2;
	vector vt0, vt1, vt2;
	int i;
	glBegin(GL_TRIANGLES);
	while(FACE(obj,i) != NULL){
		polygon_t *p0 = FACE(obj,i);
		polygon_t *pm = FACE(obj,i)->next;
		polygon_t *pn = (FACE(obj,i)->next)->next;
		COPY_POINT(obj,p0->v,v0);
		COPY_NORMAL(obj,p0->vn,vn0);
		if(tex != NULL) COPY_TEXTURE(obj,p0->vt,vt0);
		do {
			COPY_POINT(obj,pm->v,v1);
			COPY_POINT(obj,pn->v,v2);
			COPY_NORMAL(obj,pm->vn,vn1);
			COPY_NORMAL(obj,pn->vn,vn2);
			if(tex != NULL){	COPY_TEXTURE(obj,pm->vt,vt1);
					COPY_TEXTURE(obj,pn->vt,vt2); };
			glNormal3f(vn0[X], vn0[Y], vn0[Z]); glVertex3f(v0[X], v0[Y], v0[Z]);
			glNormal3f(vn1[X], vn1[Y], vn1[Z]); glVertex3f(v1[X], v1[Y], v1[Z]);
			glNormal3f(vn2[X], vn2[Y], vn2[Z]); glVertex3f(v2[X], v2[Y], v2[Z]);
			if(tex != NULL){ 
				glTexCoord2f(vt0[X], vt0[Y]);
				glTexCoord2f(vt1[X], vt1[Y]);
				glTexCoord2f(vt2[X], vt2[Y]);
			}
			pm = pn;
			pn = pn->next;
		}while(pn != NULL);
		i++;
	};
	glEnd();
}
