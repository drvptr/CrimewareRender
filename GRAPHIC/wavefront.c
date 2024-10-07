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
 *	@(#)wavefront.c	1.0 (Potr Dervyshev) 11/05/2024
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "wavefront.h"

enum parser_states {undefined = 0, comment, vertex, textures, normals, face};

enum {vc = 0, vtc, vnc, fc, counters_total};

static void count(FILE *input, int count[counters_total]){
	count[vc] = 0; count[vtc] = 0; count[vnc] = 0; count[fc] = 0;
	int current,past;
	enum parser_states mode = undefined;
	while((current = fgetc(input)) != EOF){
		if(mode != undefined){
			if(current == '\n'){
				mode = undefined;
			}
			continue;
		};
		if(current == '#'){
			mode = comment;
		};
		if( (past == 'v')&&(current == ' ') ){
			count[vc]++;
			mode = vertex;
		};
		if( (past == 'v')&&(current == 't') ){
			count[vtc]++;
			mode = textures;
		};
		if( (past == 'v')&&(current == 'n') ){
			count[vnc]++;
			mode = normals;
		};
		if( (past == 'f')&&(current == ' ') ){
			count[fc]++;
			mode = face;
		};
		past = current;
	};
	/*printf(" (dbg) WFOBJ_VERTX_CNT:%i\n",count[vc]);
	printf(" (dbg) WFOBJ_NRMLS_CNT:%i\n",count[vnc]);
	printf(" (dbg) WFOBJ_TEXRS_CNT:%i\n",count[vtc]);
	printf(" (dbg) WFOBJ_FACES_CNT:%i\n",count[fc]);*/
};

static void add_to_list(polygon **list, int v, int vt, int vn){
	polygon *new_node = malloc(sizeof(polygon));
	new_node->v = v;
	new_node->vt = vt;
	new_node->vn = vn;
	new_node->next = NULL;
	if(*list == NULL){
		*list = new_node;
	}
	else {
		new_node->next = *list;
		*list = new_node;
	};
	//printf(" (dbg) WFOBJ_ADDLST:%i added\n",vn);
};

static void remove_list(polygon *list){
	if(list){
		remove_list(list->next);
		free(list);
	};
};

static wavefront_obj *init_object(int v_size, int vt_size, int vn_size, int f_size){
	wavefront_obj *result = malloc(sizeof(wavefront_obj));
	result->vertex = malloc((sizeof(float *)) * (v_size +1));
	for(int v = 0; v < v_size; v++){
		(result->vertex)[v] = (float *)malloc(3 * sizeof(float));
	};
	(result->vertex)[v_size] = NULL;
	if(vt_size != 0){
		result->texture = malloc((sizeof(float *)) * (vt_size +1));
		for(int vt = 0; vt < vt_size; vt++){
			(result->texture)[vt] = (float *)malloc(3 * sizeof(float));
		};
		(result->texture)[vt_size] = NULL;
	}
	else {
		result->texture = NULL;
	}
	if(vn_size != 0){
		result->normal = malloc((sizeof(float *)) * (vn_size +1));
		for(int vn = 0; vn < vn_size; vn++){
			(result->normal)[vn] = (float *)malloc(3 * sizeof(float));
		};
		(result->normal)[vn_size] = NULL;
	}	
	else {
		result->normal = NULL;
	}
	result->face = malloc(sizeof(polygon *) * (f_size + 1));
	for(int i = 0; i <= f_size; i++){
		result->face[i] = NULL;
	};
	return result;
};

typedef struct {
	enum parser_states mode;
	int vt_enable;
	int vn_enable;
	int crnt;
	int past;
	int v_count; int vt_count; int vn_count;
	int f_count;
	int max_v; int max_vt; int max_vn;
} parser_session ;

static int define_state(parser_session *state){
	if(state->crnt == '#'){
		state->mode = comment;
	};
	if( (state->past == 'v')&&(state->crnt == ' ') ){
		state->mode = vertex;
	};
	if( (state->past == 'v')&&(state->crnt == 't') ){
		state->mode = textures;
		if(!(state->vt_enable)){
			state->vt_enable = 1;
		};
	};
	if( (state->past == 'v')&&(state->crnt == 'n') ){
		state->mode = normals;
		if(!(state->vn_enable)){
			state->vn_enable = 1;
		};
	};
	if( (state->past == 'f')&&(state->crnt == ' ') ){
		state->mode = face;
	};
	return state->mode;
};

static void pick_vertex(FILE *in, parser_session data, wavefront_obj *out){
	float x,y,z;
	if( 3 == fscanf(in, "%f %f %f", &x, &y, &z) ){
		VERTEX(out, (data.v_count), X) = x;
		VERTEX(out, (data.v_count), Y) = y;
		VERTEX(out, (data.v_count), Z) = z;
	};
};

static void pick_texture(FILE *in, parser_session data, wavefront_obj *out){
	float x,y,z;
	if( 3 == fscanf(in, "%f %f %f", &x, &y, &z) ){
		TEXTURE(out, data.vt_count, X) = x;
		TEXTURE(out, data.vt_count, Y) = y;
		TEXTURE(out, data.vt_count, Z) = z;
	}
};

static void pick_normal(FILE *in, parser_session data, wavefront_obj *out){
	float x,y,z;
	if( 3 == fscanf(in, "%f %f %f", &x, &y, &z)){
		NORMAL(out, data.vn_count, X) = x;
		NORMAL(out, data.vn_count, Y) = y;
		NORMAL(out, data.vn_count, Z) = z;
	};
};

static void pick_face(FILE *in, parser_session data, wavefront_obj *out){
	if( data.vt_enable == 0 && data.vn_enable == 0 ){
		int v;
		while(fscanf(in,"%i ",&v) == 1){
			if(v < 0){
				v = data.max_v + 1 + v;
			};
			add_to_list(&FACE(out, data.f_count), v, 0, 0);
		};
	};
	if( data.vt_enable && !(data.vn_enable) ){
		int v,vt;
		while(fscanf(in,"%i/%i",&v,&vt) == 2){
			if(v < 0){
				v = data.max_v + 1 + v;
			};
			if(vt < 0){
				vt = data.max_vt + 1 + vt;
			};				
			add_to_list(&FACE(out, data.f_count), v, vt, 0);
		};
	};
	if( !(data.vt_enable) && data.vn_enable ){
		int v,vn;
		while(fscanf(in,"%i//%i",&v,&vn) == 2){
			if(v < 0){
				v = data.max_v + 1 + v;
			};
			if(vn < 0){
				vn = data.max_vn + 1 + vn;
			};				
			add_to_list(&FACE(out, data.f_count), v, 0, vn);
		};
	};
	if( data.vt_enable && data.vn_enable ){
		int v,vt,vn;
		while(fscanf(in,"%i/%i/%i",&v,&vt,&vn) == 3){
			if(v < 0){
				v = data.max_v + 1 + v;
			};
			if(vt < 0){
				vt = data.max_vt + 1 + vt;
			};
			if(vn < 0){
				vn = data.max_vn + 1 + vn;
			};				
			add_to_list(&FACE(out, data.f_count), v, vt, vn);
		};
	};
};

static void parse_objfile(FILE *in, wavefront_obj *out, int max_v, int max_vt, int max_vn){
	parser_session state = {undefined,0,0,0,0,0,0,0,0, max_v, max_vt, max_vn};
	state.past = fgetc(in);
	while((state.crnt = fgetc(in)) != EOF){
		switch(state.mode){
			case undefined:
				if( define_state(&state) ){
					ungetc(state.crnt, in);
				};
				break;
			case comment:
				if(state.crnt == '\n'){
					state.mode = undefined;
				}
				break;
			case vertex:
				pick_vertex(in,state,out);
				state.v_count++;
				state.mode = undefined;
				break;
			case textures:
				pick_texture(in,state,out);
				state.vt_count++;
				state.mode = undefined;
				break;
			case normals:
				pick_normal(in,state,out);
				state.vn_count++;
				state.mode = undefined;
				break;
			case face:
				pick_face(in,state,out);
				state.f_count++;
				state.crnt = '\n';
				state.mode = undefined;
				break;
		};
		state.past = state.crnt;
	}
}

wavefront_obj *ImportObj(char *filename){
	if(filename[0] == '\0'){
		fprintf(stderr," (err) Empty filename\n");
		return NULL;
	};
	FILE *input = fopen(filename,"r");
	if(input == NULL){
		fprintf(stderr," (err) No such file named %s\n",filename);
		return NULL;
	};
	int max[counters_total];
	count(input,max);
	wavefront_obj *result = init_object(max[vc], max[vtc], max[vnc], max[fc]);
	rewind(input);
	parse_objfile(input, result, max[vc], max[vtc], max[vnc]);
	fclose(input);
	return result;
};

#ifdef _WIN32
FILE *fmemopen(unsigned char arr[], unsigned int len, const char *mode);
#endif

wavefront_obj *ImportEmbedObj(unsigned char obj[], unsigned int len){
	FILE *input = fmemopen(obj,len,"r");
	if(input == NULL){
		fprintf(stderr," (err) Cant open embeded object\n");
		return NULL;
	};
	int max[counters_total];
	count(input,max);
	wavefront_obj *result = init_object(max[vc], max[vtc], max[vnc], max[fc]);
	rewind(input);
	parse_objfile(input, result, max[vc], max[vtc], max[vnc]);
	fclose(input);
	return result;
};

void FreeObj(wavefront_obj *obj){ 
	int i = 0;
	while(obj->face[i] != NULL){
		remove_list(obj->face[i]);
		i++;
	};
	i = 0;
	while((obj->vertex)[i] != NULL){
		free((obj->vertex)[i]);
		i++;
	};
	i = 0;
	if(obj->texture != NULL){
		while((obj->texture)[i] != NULL){
			free((obj->texture)[i]);
			i++;
		};
		i = 0;
	}
	if(obj->normal != NULL){
		while((obj->normal)[i] != NULL){
			free((obj->normal)[i]);
			i++;
		};
	}
	free(obj->vertex);
	free(obj->texture);
	free(obj->normal);
	free(obj->face);
	free(obj);
};

void WavefrontPrintLog(wavefront_obj *obj){
	int v = 0; int vt = 0; int vn = 0;
	while ((obj->vertex)[v] != NULL) {
		v++;
	}
	int i = 0; 
	while(FACE(obj,i) != NULL){
		printf("FACE #%i\n",i);
		for(polygon *cur = FACE(obj,i); cur != NULL; cur = cur->next){
			printf(" vertex\t(%i):\t",cur->v);
			printf("%f,\t%f,\t%f\n",
					VERTEX(obj,cur->v - 1,X),
					VERTEX(obj,cur->v - 1,Y),
					VERTEX(obj,cur->v - 1,Z));
			if(obj->texture != NULL){
				printf(" textur\t(%i):\t",cur->vt);
				printf("%f,\t%f,\t%f\n",
						TEXTURE(obj,cur->vt - 1,X),
						TEXTURE(obj,cur->vt - 1,Y),
						TEXTURE(obj,cur->vt - 1,Z));
			};
			if(obj->normal != NULL){
				printf(" normal\t(%i):\t",cur->vn);
				printf("%f,\t%f,\t%f\n",
						NORMAL(obj,cur->vn - 1,X),
						NORMAL(obj,cur->vn - 1,Y),
						NORMAL(obj,cur->vn - 1,Z));
			};
		};
		printf("\n\n");
		i++;
	};
};

static inline void ComputeNormal(vector v1, vector v2, vector v3, vector n){
	vector u, v;
	vec_sub(v2,v1,u); //u = v2 - v1
	vec_sub(v3,v1,v); //v = v3 - v1
	vec_cross(u,v,n); //n = u x v
}

void WavefrontCalculateNormals(wavefront_obj *obj){
	int v = 0;
	if(obj->normal != NULL) {
		free(obj->normal);
	};
	while ((obj->vertex)[v] != NULL) {
		v++;
	}
	obj->normal = malloc((sizeof(float *)) * (v + 1));
	for (int vn = 0; vn < v; vn++) {
		obj->normal[vn] = (float *)malloc(3 * sizeof(float));
		obj->normal[vn][X] = 0;
		obj->normal[vn][Y] = 0;
		obj->normal[vn][Z] = 0;
	}
	obj->normal[v] = NULL;
	int i = 0;
	vector p0,p1,p2, n;
	while(FACE(obj, i) != NULL) {
		polygon *fst = FACE(obj, i);
		fst->vn = fst->v;
		COPY_POINT(obj,fst->v,p0);
		polygon *prv = FACE(obj, i)->next;
		polygon *cur = FACE(obj, i)->next->next;
		do{
			prv->vn = prv->v;
			cur->vn = cur->v;
			COPY_POINT(obj,prv->v,p1);
			COPY_POINT(obj,cur->v,p2);
			ComputeNormal(p0,p1,p2, n); //n = comp_normal()
			NORMAL(obj,fst->vn - 1,X) -= n[X];
			NORMAL(obj,fst->vn - 1,Y) -= n[Y];
			NORMAL(obj,fst->vn - 1,Z) -= n[Z];
			NORMAL(obj,prv->vn - 1,X) -= n[X];
			NORMAL(obj,prv->vn - 1,Y) -= n[Y];
			NORMAL(obj,prv->vn - 1,Z) -= n[Z];
			NORMAL(obj,cur->vn - 1,X) -= n[X];
			NORMAL(obj,cur->vn - 1,Y) -= n[Y];
			NORMAL(obj,cur->vn - 1,Z) -= n[Z];
			prv = cur;
			cur = cur->next;
		} while(cur != NULL);
		i++;
	}
	for(int vn = 0; vn < v; vn++) {
		vec_normalize(obj->normal[vn]);
	}
}

void TurnObj(wavefront_obj *obj, float alpha, float beta, float gamma){
	float a,b,c,d,e,f,g,h,i;
	int n = 0;
	a = cosf(beta)*cosf(gamma);
	b = -sinf(gamma)*cosf(beta);
	c = sinf(beta);
	d = sinf(alpha)*sinf(beta)*cosf(gamma) + sinf(gamma)*cos(alpha);
	e = -sinf(alpha)*sinf(beta)*sinf(gamma) + cosf(alpha)*cosf(gamma);
	f = - sinf(alpha)*cosf(beta);
	g = sinf(alpha)*sinf(gamma) - sinf(beta)*cosf(alpha)*cosf(gamma);
	h = sinf(alpha)*cosf(gamma) + sinf(beta)*sinf(gamma)*cosf(alpha);
	i = cosf(alpha)*cosf(beta);
	while((obj->vertex)[n] != NULL){
		float x = VERTEX(obj,n,X);
		float y = VERTEX(obj,n,Y);
		float z = VERTEX(obj,n,Z);
		VERTEX(obj,n,X) = x*a + y*b + z*c;
		VERTEX(obj,n,Y) = x*d + y*e + z*f;
		VERTEX(obj,n,Z) = x*g + y*h + z*i;
		n++;
	};
};

void MoveObj(wavefront_obj *obj, float dx, float dy, float dz){
	int n = 0;
	while((obj->vertex)[n] != NULL){
		VERTEX(obj,n,X) += dx;
		VERTEX(obj,n,Y) += dy;
		VERTEX(obj,n,Z) += dz;
		n++;
	};
};

void ScaleObj(wavefront_obj *obj, float multipler){
	int n = 0;
	while((obj->vertex)[n] != NULL){
		VERTEX(obj,n,X) *= multipler;
		VERTEX(obj,n,Y) *= multipler;
		VERTEX(obj,n,Z) *= multipler;
		n++;
	};
};
