/*-
 * SPDX-License-Identifier: BSD-0-Clause
 *
 * Copyright (c) 2025
 *	Potr Dervyshev.  All rights reserved.
 *	@(#)wavefront.c	1.0 (Potr Dervyshev) 28/09/2025
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "wavefront.h"

/*------------------------------------------------- 
	#        Polygon Stack (static)      #
------------------------------------------------- */

static polygon_t *PushPolyPoint(polygon_t **top, const int v, const int vt, const int vn){
	polygon_t *new_node = malloc(sizeof(polygon_t));
	new_node->v = v;
	new_node->vt = vt;
	new_node->vn = vn;
	new_node->next = *top;
	*top = new_node;
#ifdef DEBUG
	printf("(dbg) wavefront.c: POINT ADDED TO THE POLY: v=%i, vt=%i, vn=%i\n", v, vt, vn);
#endif
	return new_node;
}

static void DelPolyPoint(polygon_t **list, int v){
	while(*list){
		if((*list)->v == v){
			polygon_t *target = *list;
			*list = (*list)->next;
			free(target);
			return;
		}
		else {
			list = &(*list)->next;
		};
	}
}

static polygon_t *PopPolyPoint(polygon_t **top) {
	if (*top == NULL) 
		return NULL;
	polygon_t *res = *top;
	*top = (*top)->next;
	return res;
}

static void RemovePoly(polygon_t *list){
	while(list) {
		polygon_t *tmp = list;
		list = list->next;
		free(tmp);
	}
}

/*------------------------------------------------- 
	#        Utility (static)      #
------------------------------------------------- */

static char *FileToBuffer(const char path[]) {
	FILE *file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	long length = ftell(file);
	if (length <= 0) {
		fclose(file);
		return NULL;
	}
	rewind(file);
	char *result = malloc((size_t)length + 1);
	if (!result) {
		fclose(file);
		return NULL;
	}
	size_t read_bytes = fread(result, 1, (size_t)length, file);
	fclose(file);
	result[read_bytes] = '\0';
	return result;
}

static wf_wavefront_t *InitWavefront(int vc, int vtc, int vnc, int fc){
	if(vc == 0)
		return NULL;
	wf_wavefront_t *result = malloc(sizeof(wf_wavefront_t));
	result->vertex = malloc((sizeof(float *)) * (vc + 1));
	for(int v = 0; v < vc; v++){
		(result->vertex)[v] = (float *)malloc(3 * sizeof(float));
	};
	(result->vertex)[vc] = NULL;
	result->texture = NULL;
	if(vtc != 0){
		result->texture = malloc((sizeof(float *)) * (vtc +1));
		for(int vt = 0; vt < vtc; vt++){
			(result->texture)[vt] = (float *)malloc(3 * sizeof(float));
		};
		(result->texture)[vtc] = NULL;
	}
	result->normal = NULL;
	if(vnc != 0){
		result->normal = malloc((sizeof(float *)) * (vnc +1));
		for(int vn = 0; vn < vnc; vn++){
			(result->normal)[vn] = (float *)malloc(3 * sizeof(float));
		};
		(result->normal)[vnc] = NULL;
	}	
	result->face = malloc(sizeof(polygon_t *) * (fc + 1));
	for(int i = 0; i <= fc; i++){
		result->face[i] = NULL;
	};
	return result;
}

/*------------------------------------------------- 
	#     1. Parser Class (static)      #
------------------------------------------------- */

typedef enum parser_states {
	ST_UNDEFINED,
	ST_V,
	ST_VT,
	ST_VN,
	ST_F,
	ST_COMMENT,
	MAX_STATES
} parser_states_t;

typedef struct parser parser_t;

typedef void (*Handler_t)(const char *line, int line_len, void *userdata, parser_t *this);

struct parser {
	parser_states_t state;
	Handler_t Handle[MAX_STATES];
	void *userdata;
};

static void ParseObj(const char *input, parser_t *p) {
	int start = 0;
	for (int i = 0; input[i] != '\0'; i++) {
		char c = input[i];
		if (c == '\n') {
			int len = i - start;
			if (len > 0) {
				if (p->state < MAX_STATES && p->Handle[p->state]) {
					p->Handle[p->state](input + start, len, p->userdata, p);
				}
			}
			p->state = ST_UNDEFINED;
			start = i + 1;
		}
		else if (p->state == ST_UNDEFINED) {
			if (c == '#') p->state = ST_COMMENT;
			else if (c == 'v') {
				if (input[i+1] == 't') { p->state = ST_VT; i++; }
				else if (input[i+1] == 'n') { p->state = ST_VN; i++; }
				else if (input[i+1] == ' ') { p->state = ST_V; }
			}
			else if (c == 'f' && input[i+1] == ' ') p->state = ST_F;
		}
	}
}

void Count(const char *line, int line_len, void *out,parser_t *this){
	int *count_res = (int *)out;
	count_res[this->state]++;
}

void AddVertex(const char *line, int len, void *data, parser_t *this) {
	void **tmp = (void **)data;
	wf_wavefront_t *obj = (wf_wavefront_t*)tmp[0];
	int *counters = (int*)tmp[1];
	int *idx = &counters[ST_V];
	float *v = obj->vertex[*idx];
	char buf[len+1];
	memcpy(buf, line+1, len-1);
	buf[len-1] = '\0';
	char *token = strtok(buf, " \t");
	int i = 0;
	while (token && i < 4) {
		v[i++] = strtof(token, NULL);
		token = strtok(NULL, " \t");
	}
	if (i == 3) v[3] = 1.0f;
	(*idx)++;
}

void AddNormal(const char *line, int len, void *data, parser_t *this) {
	void **tmp = (void **)data;
	wf_wavefront_t *obj = (wf_wavefront_t*)tmp[0];
	int *counters = (int*)tmp[1];
	int *idx = &counters[ST_VN];
	float *n = obj->normal[*idx];
	char buf[len+1];
	memcpy(buf, line+2, len-2); // "vn " уже скипаем
	buf[len-2] = '\0';
	char *token = strtok(buf, " \t");
	int i = 0;
	while (token && i < 3) {
		n[i++] = strtof(token, NULL);
		token = strtok(NULL, " \t");
	}
	(*idx)++;
}

void AddTexCoord(const char *line, int len, void *data, parser_t *this) {
	void **tmp = (void **)data;
	wf_wavefront_t *obj = (wf_wavefront_t*)tmp[0];
	int *counters = (int*)tmp[1];
	int *idx = &counters[ST_VT];
	float *t = obj->texture[*idx];
	char buf[len+1];
	memcpy(buf, line+2, len-2); // "vt "
	buf[len-2] = '\0';
	char *token = strtok(buf, " \t");
	int i = 0;
	while (token && i < 3) {
		t[i++] = strtof(token, NULL);
		token = strtok(NULL, " \t");
	}
	if (i == 2) t[2] = 0.0f;
	(*idx)++;
}

void AddFace(const char *line, int len, void *data, parser_t *this) {
	void **tmp = (void **)data;
	wf_wavefront_t *obj = (wf_wavefront_t*)tmp[0];
	int *counters = (int*)tmp[1];
	int *fidx = &counters[ST_F];
	char buf[len+1];
	memcpy(buf, line+1, len-1);
	buf[len-1] = '\0';
	char *token = strtok(buf, " \t");
	while (token) {
		int v=0, vt=0, vn=0;
		char *s1 = token;
		char *s2 = strchr(s1, '/');
		if (s2) { *s2 = '\0'; s2++; }
		char *s3 = s2 ? strchr(s2, '/') : NULL;
		if (s3) { *s3 = '\0'; s3++; }
		if (*s1) v  = atoi(s1);
		if (s2 && *s2) vt = atoi(s2);
		if (s3 && *s3) vn = atoi(s3);
		if (v  < 0) v  = counters[ST_V]  + v + 1;
		if (vt < 0) vt = counters[ST_VT] + vt + 1;
		if (vn < 0) vn = counters[ST_VN] + vn + 1;
		if (v != 0)
			PushPolyPoint(&obj->face[*fidx], v, vt, vn);
		token = strtok(NULL, " \t");
	}
	(*fidx)++;
}

/*------------------------------------------------- 
	#        1. Main Publuc      #
------------------------------------------------- */

wf_wavefront_t *wf_LoadWavefront(char *filename){
	char *buffer = FileToBuffer(filename);
	if(buffer == NULL){
		fprintf(stderr," (err) wavefront.c: Failed to open %s\n",filename);
		return NULL;
	};
	int count_res[MAX_STATES] = {0,0,0,0,0,0};
	parser_t FirstRun = {ST_UNDEFINED,{NULL,Count,Count,Count,Count,NULL},count_res};
	ParseObj(buffer, &FirstRun);
	wf_wavefront_t *newobj = InitWavefront(count_res[ST_V], count_res[ST_VT], count_res[ST_VN], count_res[ST_F]);
	int idx[MAX_STATES] = {0,0,0,0,0,0};
	void *data[] = {newobj, idx};
	parser_t SecondRun = {ST_UNDEFINED,{NULL,AddVertex,AddTexCoord,AddNormal,AddFace, NULL},data};
	ParseObj(buffer, &SecondRun);
	return newobj;
}

wf_wavefront_t *wf_LoadMemoryWavefront(const char *buffer){
	if(buffer == NULL){
		fprintf(stderr," (err) wavefront.c: Failed to open nullptr \n");
		return NULL;
	};
	int count_res[MAX_STATES] = {0,0,0,0,0,0};
	parser_t FirstRun = {ST_UNDEFINED,{NULL,Count,Count,Count,Count,NULL},count_res};
	ParseObj(buffer, &FirstRun);
	wf_wavefront_t *newobj = InitWavefront(count_res[ST_V], count_res[ST_VT], count_res[ST_VN], count_res[ST_F]);
	int idx[MAX_STATES] = {0,0,0,0,0,0};
	void *data[] = {newobj, idx};
	parser_t SecondRun = {ST_UNDEFINED,{NULL,AddVertex,AddTexCoord,AddNormal,AddFace, NULL},data};
	ParseObj(buffer, &SecondRun);
	return newobj;
}

void wf_RemoveWavefront(wf_wavefront_t *obj){ 
	int i = 0;
	while(obj->face[i] != NULL){
		RemovePoly(obj->face[i]);
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
}

void wf_WavefrontPrintLog(wf_wavefront_t *obj){
	int v = 0; int vt = 0; int vn = 0;
	while ((obj->vertex)[v] != NULL) {
		v++;
	}
	int i = 0; 
	while(FACE(obj,i) != NULL){
		printf("FACE #%i\n",i);
		for(polygon_t *cur = FACE(obj,i); cur != NULL; cur = cur->next){
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
}

/*------------------------------------------------- 
	#       2. Geometry      #
------------------------------------------------- */

static inline void ComputeNormal(vector v1, vector v2, vector v3, vector n){
	vector u, v;
	vec_sub(v2,v1,u); //u = v2 - v1
	vec_sub(v3,v1,v); //v = v3 - v1
	vec_cross(u,v,n); //n = u x v
}

void wf_WavefrontCalculateNormals(wf_wavefront_t *obj){
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
		polygon_t *fst = FACE(obj, i);
		fst->vn = fst->v;
		COPY_POINT(obj,fst->v,p0);
		polygon_t *prv = FACE(obj, i)->next;
		polygon_t *cur = FACE(obj, i)->next->next;
		do{
			prv->vn = prv->v;
			cur->vn = cur->v;
			COPY_POINT(obj,prv->v,p1);
			COPY_POINT(obj,cur->v,p2);
			ComputeNormal(p0,p1,p2, n); //n = comp_normal()
			NORMAL(obj,fst->vn - 1,X) += n[X];
			NORMAL(obj,fst->vn - 1,Y) += n[Y];
			NORMAL(obj,fst->vn - 1,Z) += n[Z];
			NORMAL(obj,prv->vn - 1,X) += n[X];
			NORMAL(obj,prv->vn - 1,Y) += n[Y];
			NORMAL(obj,prv->vn - 1,Z) += n[Z];
			NORMAL(obj,cur->vn - 1,X) += n[X];
			NORMAL(obj,cur->vn - 1,Y) += n[Y];
			NORMAL(obj,cur->vn - 1,Z) += n[Z];
			prv = cur;
			cur = cur->next;
		} while(cur != NULL);
		i++;
	}
	for(int vn = 0; vn < v; vn++) {
		vec_normalize(obj->normal[vn]);
	}
}

void wf_TurnWavefront(wf_wavefront_t *obj, float alpha, float beta, float gamma){
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
}

void wf_MoveWavefront(wf_wavefront_t *obj, float dx, float dy, float dz){
	int n = 0;
	while((obj->vertex)[n] != NULL){
		VERTEX(obj,n,X) += dx;
		VERTEX(obj,n,Y) += dy;
		VERTEX(obj,n,Z) += dz;
		n++;
	};
}

void wf_ScaleWavefront(wf_wavefront_t *obj, float multipler){
	int n = 0;
	while((obj->vertex)[n] != NULL){
		VERTEX(obj,n,X) *= multipler;
		VERTEX(obj,n,Y) *= multipler;
		VERTEX(obj,n,Z) *= multipler;
		n++;
	};
}

void wf_MoveVertex(wf_wavefront_t *obj, int id, float dx, float dy, float dz){
	VERTEX(obj,id,X) += dx;
	VERTEX(obj,id,Y) += dy;
	VERTEX(obj,id,Z) += dz;
}

void wf_SetVertex(wf_wavefront_t *obj, int id, float x, float y, float z){
	VERTEX(obj,id,X) = x;
	VERTEX(obj,id,Y) = y;
	VERTEX(obj,id,Z) = z;
}
