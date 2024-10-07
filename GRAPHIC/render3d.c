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
 *	@(#)render3d.c	1.0 (Potr Dervyshev) 11/05/2024
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "render3d.h"

#define SUN (vector){0,0,1}
#define MISSED_TEXTURE_COLOR 0xFF000000
#define SHADOW 0.4
#define REFLEX 0.2

/*plotters (call-back funcs for DrawTriangle)*/
static void DepthFilter(window *w, int x, int y, int color, void *data);
static void DepthPlot(window *w, int x, int y, int color, void *user_data);
static void TexturePlot(window *w, int x, int y, int color, void *user_data);
/*ZBufer utilities*/
static fixed **ZBufferInit(int width, int height);
static void FillZBuffer(window *w, camera *cam, wavefront_obj *obj);
static void CleanZBuffer(camera *cam);
static void ZBufferFree(fixed **ZBuffer);

camera *InitCamera(window *w,int x0,int y0,int z0,int x1,int y1,int z1,int fov){
	camera *res = malloc(sizeof(camera));
	res->pos[X] = (float)(x0);
	res->pos[Y] = (float)(y0);
	res->pos[Z] = (float)(z0);
	res->target[X] = (float)(x1);
	res->target[Y] = (float)(y1);
	res->target[Z] = (float)(z1);
	res->fov = (float)(fov);
	res->w = io_GetWidth(w);
	res->h = io_GetHeight(w);
	res->hw = res->w/2;
	res->hh = res->h/2;
	vec_sub(res->target, res->pos, res->dir);
	vec_normalize(res->dir);
	vec_cross(SUN, res->dir, res->y_aix);
	vec_normalize(res->y_aix);
	vec_cross(res->dir, res->y_aix, res->z_aix);
	vec_normalize(res->z_aix);
	res->zbuffer = ZBufferInit(res->w, res->h);
	res->buf_refill_required = TRUE;
	return res;
};

void FreeCamera(camera *cam){
	ZBufferFree(cam->zbuffer);
	free(cam);
}

void MoveCamera(camera *cam, vector new_pos, vector new_target){
	cam->buf_refill_required = TRUE;
	CleanZBuffer(cam);
	if (new_pos != NULL) {
		cam->pos[X] = new_pos[X];
		cam->pos[Y] = new_pos[Y];
		cam->pos[Z] = new_pos[Z];
	};
	if (new_target != NULL) {
		cam->target[X] = new_target[X];
		cam->target[Y] = new_target[Y];
		cam->target[Z] = new_target[Z];
	};
	vec_sub(cam->target, cam->pos, cam->dir);
	vec_normalize(cam->dir);
	vec_cross(SUN,  cam->dir, cam->y_aix );
	vec_normalize(cam->y_aix);
	vec_cross(cam->dir, cam->y_aix, cam->z_aix);
	vec_normalize(cam->z_aix);
}

int PerspectiveProjection(vector p, camera *cam, int *x, int *y, fixed *z){
	vector p_c = {  p[X] - cam->pos[X],
			p[Y] - cam->pos[Y],
			p[Z] - cam->pos[Z]	};
	float x_cam = vec_dot(p_c, cam->y_aix);
	float y_cam = vec_dot(p_c, cam->z_aix);
	float z_cam = vec_dot(p_c, cam->dir);
	if(z_cam <= 0) {
		return 1;
	}
	*x = (int)((x_cam * cam->fov) / z_cam) + cam->hw;
	*y = cam->hh - (int)((y_cam * cam->fov) / z_cam);
	*z = FLOAT_TO_FIXED(z_cam);
	return 0;
}

void RenderWireframe(window *w, camera *cam, wavefront_obj *obj, int color){
	int i = 0;
	while(FACE(obj,i) != NULL){
		vector p0, p1;
		fixed z0,z1;
		int x0,y0,x1,y1;
		polygon *prv = FACE(obj,i);
		polygon *cur = FACE(obj,i)->next;
		while(cur != NULL){
			COPY_POINT(obj,prv->v,p0);
			COPY_POINT(obj,cur->v,p1);
			if(PerspectiveProjection(p0, cam, &x0, &y0, &z0) ||
			   PerspectiveProjection(p1, cam, &x1, &y1, &z1)){
			   	prv = cur;
				cur = cur->next;
				continue;
			};
			DrawLine(w,x0,y0,x1,y1,color);
			prv = cur;
			cur = cur->next;
			if(cur == NULL){
				cur = FACE(obj,i);
				COPY_POINT(obj,prv->v,p0);
				COPY_POINT(obj,cur->v,p1);
				if(PerspectiveProjection(p0, cam, &x0, &y0, &z0) ||
				   PerspectiveProjection(p1, cam, &x1, &y1, &z1))
					break;
				DrawLine(w,x0,y0,x1,y1,color);
				break;
			};
		};
		i++;
	};
};

/*plotter-func*/
static void DepthPlot(window *w, int x, int y, int color, void *user_data){
	void **data = (void **)user_data;
	fixed z0 = *((int *)(data[0]));
	fixed z1 = *((int *)(data[1]));
	fixed z2 = *((int *)(data[2]));
	fixed z = div((z0+z1+z2),3);
	fixed **zbuffer = ((fixed **)(data[3]));
	if(z <= zbuffer[x][y]){
		io_SetPixel(w,x,y,color);
	}
};

void RenderShaded(window *w, camera *cam, wavefront_obj *obj, int color){
	if(cam->buf_refill_required){
		FillZBuffer(w, cam, obj);
		cam->buf_refill_required = FALSE;
	};
	int i = 0; vector u,v,n; float intensy = 1;
	vector p0, p1, p2;
	fixed z0,z1,z2; int x0,y0,x1,y1,x2,y2;
	void *data[4] = {&z0, &z1, &z2, cam->zbuffer};
	while(FACE(obj,i) != NULL){
		polygon *fst = FACE(obj,i);
		polygon *prv = FACE(obj,i)->next;
		polygon *cur = (FACE(obj,i)->next)->next;
		COPY_POINT(obj,fst->v,p0);
		if(PerspectiveProjection(p0, cam, &x0, &y0, &z0))
			continue;
		do {
			COPY_POINT(obj,prv->v,p1);
			COPY_POINT(obj,cur->v,p2);
			if(PerspectiveProjection(p1, cam, &x1, &y1, &z1) ||
			   PerspectiveProjection(p2, cam, &x2, &y2, &z2)){
			   	prv = cur;
				cur = cur->next;
				continue;
			};
			vec_sub(p1,p0,u); vec_sub(p2,p0,v);
			vec_cross(v,u,n); vec_normalize(n);
			intensy = vec_dot(SUN,n);
			intensy = (1-intensy)*SHADOW + intensy;
			if(intensy <= 0){
				intensy = -intensy *REFLEX;
			}
			int newcol = AdjustIntensity(color,intensy);
			DrawTriangle(w,x0,y0,x1,y1,x2,y2,
					DepthPlot,newcol,data);
			prv = cur;
			cur = cur->next;
		}while(cur != NULL);
		i++;
	};
};

/*plotter-func*/
static void TexturePlot(window *w, int x, int y, int color, void *user_data){
	void **data = (void **)user_data;
	fixed z0 = *((int *)(data[0]));
	fixed z1 = *((int *)(data[1]));
	fixed z2 = *((int *)(data[2]));
	fixed z = div((z0+z1+z2),3);
	fixed **zbuffer = ((fixed **)(data[3]));
	TGAimage *texture = ((TGAimage *)(data[4]));
	vector t0,t1,t2;
	t0[X] = ((float *)(data[5]))[Y]; t0[Y] = ((float *)(data[5]))[X];
	t1[X] = ((float *)(data[6]))[Y]; t1[Y] = ((float *)(data[6]))[X];
	t2[X] = ((float *)(data[7]))[Y]; t2[Y] = ((float *)(data[7]))[X];
	vector p0 = {(float)(*((int *)(data[8]))),(float)(*((int *)(data[9]))),FIXED_TO_FLOAT(z0)};
	vector p1 = {(float)(*((int *)(data[10]))),(float)(*((int *)(data[11]))),FIXED_TO_FLOAT(z1)};
	vector p2 = {(float)(*((int *)(data[12]))),(float)(*((int *)(data[13]))),FIXED_TO_FLOAT(z2)};
	float lambda0, lambda1, lambda2;
	Barycentric(x, y, p0, p1, p2, &lambda0, &lambda1, &lambda2);
	float u = lambda0 * t0[X] + lambda1 * t1[X] + lambda2 * t2[X];
	float v = lambda0 * t0[Y] + lambda1 * t1[Y] + lambda2 * t2[Y];
	int tw = get_width(texture);
	int th = get_height(texture);
	float i = (*((float *)(data[14])));
	if (u >= 0 && u < 1 && v >= 0 && v < 1) {
        	color = get_pixel(texture, (int)(v * th), (int)(u * tw));
    	}
	if(z <= zbuffer[x][y]){
		int newcol = AdjustIntensity(color,i);
		io_SetPixel(w,x,y,newcol);
	}
};

void RenderTextured(window *w, camera *cam, wavefront_obj *obj, TGAimage *texture){
	if(obj->texture == NULL || texture == NULL){
		RenderShaded(w,cam,obj, MISSED_TEXTURE_COLOR);
		return;
	};
	if(cam->buf_refill_required){
		FillZBuffer(w, cam, obj);
		cam->buf_refill_required = FALSE;
	};
	int i = 0; vector u,v,n; float intensy = 1;
	vector p0, p1, p2;
	vector t0,t1,t2;
	fixed z0,z1,z2; int x0,y0,x1,y1,x2,y2;
	void *data[15] = {&z0, &z1,&z2,cam->zbuffer,texture,t0,t1,t2,
			   &x0, &y0, &x1, &y1, &x2, &y2,&intensy};
	while(FACE(obj,i) != NULL){
		polygon *fst = FACE(obj,i);
		polygon *prv = FACE(obj,i)->next;
		polygon *cur = (FACE(obj,i)->next)->next;
		COPY_POINT(obj,fst->v,p0);
		COPY_TEXTURE(obj,fst->vt,t0);
		if(PerspectiveProjection(p0, cam, &x0, &y0, &z0))
			continue;
		do {
			COPY_POINT(obj,prv->v,p1);
			COPY_TEXTURE(obj,prv->vt,t1);
			COPY_POINT(obj,cur->v,p2);
			COPY_TEXTURE(obj,cur->vt,t2);
			if(PerspectiveProjection(p1, cam, &x1, &y1, &z1) ||
			   PerspectiveProjection(p2, cam, &x2, &y2, &z2)){
			   	prv = cur;
				cur = cur->next;
				continue;
			};
			vec_sub(p1,p0,u); vec_sub(p2,p0,v);
			vec_cross(v,u,n); vec_normalize(n);
			intensy = vec_dot(SUN,n);
			intensy = (1-intensy)*SHADOW + intensy;
			if(intensy <= 0){
				intensy = -intensy *REFLEX;
			}
			DrawTriangle(w,x0,y0,x1,y1,x2,y2,
					TexturePlot,MISSED_TEXTURE_COLOR,data);
			prv = cur;
			cur = cur->next;
		}while(cur != NULL);
		i++;
	};
};

static void GouraudPlot(window *w, int x, int y, int color, void *user_data){
	void **data = (void **)user_data;
	fixed z0 = *((int *)(data[0]));
	fixed z1 = *((int *)(data[1]));
	fixed z2 = *((int *)(data[2]));
	fixed z = div((z0+z1+z2),3);
	int textured = *((int *)(data[17]));
	fixed **zbuffer = ((fixed **)(data[3]));
	TGAimage *texture = ((TGAimage *)(data[4]));
	vector t0,t1,t2;
	if(textured){
		t0[X] = ((float *)(data[5]))[Y]; t0[Y] = ((float *)(data[5]))[X];
		t1[X] = ((float *)(data[6]))[Y]; t1[Y] = ((float *)(data[6]))[X];
		t2[X] = ((float *)(data[7]))[Y]; t2[Y] = ((float *)(data[7]))[X];
	};
	vector p0 = {(float)(*((int *)(data[8]))),(float)(*((int *)(data[9]))),FIXED_TO_FLOAT(z0)};
	vector p1 = {(float)(*((int *)(data[10]))),(float)(*((int *)(data[11]))),FIXED_TO_FLOAT(z1)};
	vector p2 = {(float)(*((int *)(data[12]))),(float)(*((int *)(data[13]))),FIXED_TO_FLOAT(z2)};
	float i0 = *((float *)(data[14]));
	float i1 = *((float *)(data[15]));
	float i2 = *((float *)(data[16]));
	float lambda0, lambda1, lambda2;
	Barycentric(x, y, p0, p1, p2, &lambda0, &lambda1, &lambda2);
	float i = lambda0 * i0 + lambda1 * i1 + lambda2 * i2;
	if(textured){
		float u = lambda0 * t0[X] + lambda1 * t1[X] + lambda2 * t2[X];
		float v = lambda0 * t0[Y] + lambda1 * t1[Y] + lambda2 * t2[Y];
		int tw = get_width(texture);
		int th = get_height(texture);
		if (u >= 0 && u < 1 && v >= 0 && v < 1) {
        		color = get_pixel(texture, (int)(v * th), (int)(u * tw));
    		}
	}
	if(z <= zbuffer[x][y]){
		int newcol = AdjustIntensity(color,i);
		io_SetPixel(w,x,y,newcol);
	}
};

void RenderGouraud(window *w, camera *cam, wavefront_obj *obj,
				TGAimage *texture, int default_color){
	if(obj->normal == NULL){
		WavefrontCalculateNormals(obj);
	};
	if(cam->buf_refill_required){
		FillZBuffer(w, cam, obj);
		cam->buf_refill_required = FALSE;
	};
	int i = 0;
	int textured = (obj->texture != NULL)&&(texture != NULL);
	float i0,i1,i2;
	vector p0, p1, p2;
	vector n0, n1, n2;
	vector t0,t1,t2;
	fixed z0,z1,z2; int x0,y0,x1,y1,x2,y2;
	void *data[18] = {&z0, &z1,&z2,cam->zbuffer,texture,t0,t1,t2,
			   &x0, &y0, &x1, &y1, &x2, &y2,&i0,&i1,&i2,
			   &textured};
	while(FACE(obj,i) != NULL){
		polygon *fst = FACE(obj,i);
		polygon *prv = FACE(obj,i)->next;
		polygon *cur = (FACE(obj,i)->next)->next;
		COPY_POINT(obj,fst->v,p0);
		COPY_TEXTURE(obj,fst->vt,t0);
		COPY_NORMAL(obj,fst->vn,n0);
		if(PerspectiveProjection(p0, cam, &x0, &y0, &z0))
			continue;
		do {
			COPY_POINT(obj,prv->v,p1);
			COPY_TEXTURE(obj,prv->vt,t1);
			COPY_NORMAL(obj,prv->vn,n1);
			COPY_POINT(obj,cur->v,p2);
			COPY_TEXTURE(obj,cur->vt,t2);
			COPY_NORMAL(obj,cur->vn,n2);
			if(PerspectiveProjection(p1, cam, &x1, &y1, &z1) ||
			   PerspectiveProjection(p2, cam, &x2, &y2, &z2)){
			   	prv = cur;
				cur = cur->next;
				continue;
			};
			i0 = vec_dot(SUN,n0); i0 = (1-i0)*SHADOW + i0;
			if(i0 <= 0){ i0 = -i0 *REFLEX; }
			i1 = vec_dot(SUN,n1); i1 = (1-i1)*SHADOW + i1;
			if(i1 <= 0){ i1 = -i1 *REFLEX; }
			i2 = vec_dot(SUN,n2); i2 = (1-i2)*SHADOW + i2;
			if(i2 <= 0){ i2 = -i2 *REFLEX; }
			DrawTriangle(w,x0,y0,x1,y1,x2,y2,
					GouraudPlot,default_color,data);
			prv = cur;
			cur = cur->next;
		}while(cur != NULL);
		i++;
	};
};

static fixed **ZBufferInit(int width, int height){
	fixed **empty_buffer =  malloc((sizeof(fixed *)) *  (width + 1));
	for(int x = 0; x < width; x++){
		empty_buffer[x] = (fixed *)malloc(height * sizeof(fixed));
	};
	empty_buffer[width] = NULL;
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			empty_buffer[x][y] = INF;
		};
	};
	return empty_buffer;
};

/*plotter-func*/
static void DepthFilter(window *w, int x, int y, int color, void *user_data){
	void **data = (void **)user_data;
	fixed z0 = *((int *)(data[0]));
	fixed z1 = *((int *)(data[1]));
	fixed z2 = *((int *)(data[2]));
	fixed z = div((z0+z1+z2),3);
	fixed **zbuffer = ((fixed **)(data[3]));
	if(z <= zbuffer[x][y]){
		zbuffer[x][y] = z;
	};
};

static void FillZBuffer(window *w, camera *cam, wavefront_obj *obj){
	int i = 0;
	vector p0, p1, p2;
	fixed z0,z1,z2;
	int x0,y0,x1,y1,x2,y2;
	void *data[4] = {&z0, &z1, &z2, cam->zbuffer};
	while(FACE(obj,i) != NULL){
		polygon *fst = FACE(obj,i);
		polygon *prv = FACE(obj,i)->next;
		polygon *cur = (FACE(obj,i)->next)->next;
		COPY_POINT(obj,fst->v,p0);
		if(PerspectiveProjection(p0, cam, &x0, &y0, &z0))
			continue;
		do {
			COPY_POINT(obj,prv->v,p1);
			COPY_POINT(obj,cur->v,p2);
			if(PerspectiveProjection(p1, cam, &x1, &y1, &z1) ||
			   PerspectiveProjection(p2, cam, &x2, &y2, &z2)){
			   	prv = cur;
				cur = cur->next;
				continue;
			};
			DrawTriangle(w,x0,y0,x1,y1,x2,y2,DepthFilter,0,data);
			prv = cur;
			cur = cur->next;
		} while(cur != NULL);
		i++;
	};
};

void RenderZBuffer(window *w, camera *cam,wavefront_obj *obj, int max_depth){
	if(cam->buf_refill_required){
		FillZBuffer(w, cam, obj);
		cam->buf_refill_required = FALSE;
	};
	for(int x = 0; x < cam->w; x++){
		for(int y = 0; y < cam->h; y++){
			if(cam->zbuffer[x][y] != INF){
				int color = ConvertToGrayARGB(
					FIXED_TO_INT(cam->zbuffer[x][y]),
					max_depth);
				DrawPixel(w,x,y,color);
			}else{
				DrawPixel(w,x,y,0xFF000000);
			};
		};
	};
};

static void CleanZBuffer(camera *cam){
	if(cam->zbuffer == NULL)
		return;
	for(int x = 0; x < cam->w; x++){
		for(int y = 0; y < cam->h; y++){
			cam->zbuffer[x][y] = INF;
		};
	};
};

static void ZBufferFree(fixed **ZBuffer){
	int x = 0;
	while(ZBuffer[x] != NULL){
		free(ZBuffer[x]);
		x++;
	};
	free(ZBuffer);
};
