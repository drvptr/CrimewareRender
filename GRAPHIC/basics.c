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
 *	@(#)basics.c	1.0 (Potr Dervyshev) 08/11/2024
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basics.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "../IO/io.h"

/*look for statics implementation at the bottom of the file*/
static int ComputeOutCode(int width, int height, int x, int y);
static int CohenSutherland(window *w, int *x0, int *y0, int *x1, int *y1);

typedef void (*Handler)(window *,int,int,int);		
static void DirectPlot(window *w,int x,int y,int color);
static void InversePlot(window *w,int x,int y,int color);
static void Bresenham(Handler Plot, window *w,
			int x0, int y0, int x1, int y1, int color);
static void Rasterize(window *w,
		   int x1,int y1,int x2,int y2,int x3,int y3,
		   Plotter Plot, int color, void *userdata);
static void Intersect(int code, int max, int x0, int y0,
			int x1, int y1, int *nx, int *ny);
static void SutherlandHodgman(window *w,
			int x0, int y0,
			int x1, int y1,
			int x2, int y2,
			Plotter Plot, int color, void *userdata);
static int ClipFill(window *w, int *x0, int *y0, int *x1, int *y1);
static int ClipRectangle(window *w, int *x0, int *y0, int *x1, int *y1);
static int TriangleCheck(window *w,int x1,int y1,int x2,int y2,int x3,int y3);
static int DecodeUTF8(char **text);

struct font_t{
	stbtt_fontinfo *info;
	char *buffer;
};

/*_____________MAIN FUNCS (DRAWERS)_____________*/

void DrawAlphaPixel(window *w, int x, int y, int color){
	int width = io_GetWidth(w); int height = io_GetHeight(w);
	if( (y < height) && (x < width) && (x >= 0) && (y >= 0) ){
		if(ALPHA(color)){
			if(TRANSPARENT(color))
				return;
			BlendAlpha(io_GetPixel(w,x,y), &color);
			io_SetPixel(w, x, y, color);
		}else{
			io_SetPixel(w, x, y, color);
		};
	};
};

void DrawPixel(window *w, int x, int y, int color){
	int width = io_GetWidth(w); int height = io_GetHeight(w);
	if( (y < height) && (x < width) && (x >= 0) && (y >= 0) ){
		io_SetPixel(w, x, y, color);
	};
};

void DrawLine(window *w, int x0, int y0, int x1, int y1, int color){
	if( CohenSutherland(w, &x0, &y0, &x1, &y1) == 0){
			return; /*clipping shown the line out of canvas*/
	};
	int inverse = ((ABS(y1 - y0)) > (ABS(x1 - x0)));
	if(inverse){
		swap_xy(&x0,&y0);
		swap_xy(&x1,&y1);
	};
	if(x1 < x0){
		swap_xy(&x0,&x1);
		swap_xy(&y0,&y1);
	};
	if(inverse){/*more then 45deg line*/
		Bresenham(InversePlot, w, x0, y0, x1, y1, color);
	}
	else{	/*default less then 45deg line*/
		Bresenham(DirectPlot, w, x0, y0, x1, y1, color);
	}
};

void DrawTriangle(window *w, int x1,int y1,int x2,int y2,int x3,int y3,
		    Plotter Plot, int color, void *userdata){
	if( TriangleCheck(w,x1,y1,x2,y2,x3,y3) ) /*100% out of canvas*/
		return;
	int mx = io_GetWidth(w);
	int my = io_GetHeight(w);
	if((ComputeOutCode(mx,my,x1,y1)|ComputeOutCode(mx,my,x2,y2)|
		/*100% on canvas*/	ComputeOutCode(mx,my,x3,y3)) == 0){
		Rasterize(w,x1,y1,x2,y2,x3,y3,Plot,color,userdata);
		return;
	};
	/*split into several triangles and draw*/
	SutherlandHodgman(w,x1,y1,x2,y2,x3,y3,Plot,color,userdata);
};

void DrawImage(window *w, int x0, int y0, int *image){
	int width = io_GetWidth(w); int height = io_GetHeight(w);
	int x1 = GET_W(image); int y1 = GET_H(image);
	for(int x = 0; x < x1; x++){
		int dx = x0 + x;
		if(dx < 0 || dx > width)
			continue;
		for(int y = 0; y < y1; y++){
		 	int dy = y0 + (y1 - y);
			if(dy < 0 || dy > height)
				continue;
		 	if( !ALPHA( GET_COLOR(image,x,y) ) ){
		 		io_SetPixel(w, dx, dy, GET_COLOR(image,x,y));
			};
		};
	};
};

void DrawFill(window *w, int x0, int y0, int x1, int y1, int color){
	if (x0 > x1){
		swap_xy(&x0, &x1);
	}
	if (y0 > y1){
		swap_xy(&y0, &y1);
	}
	if( ClipFill(w, &x0, &y0, &x1, &y1) ){
		return;
	}
	for(int x = x0; x < x0 + x1; x++){
		 for(int y = y0; y < y0 + y1; y++){
		 	io_SetPixel(w, x, y, color);
		 };
	};
};

void DrawRectangle(window *w, int x0, int y0, int x1, int y1, int color){
	int width = io_GetWidth(w); int height = io_GetHeight(w);
	if (x0 > x1)
		swap_xy(&x0, &x1);
	if (y0 > y1)
		swap_xy(&y0, &y1);
	if( ClipRectangle(w, &x0, &y0, &x1, &y1) )
		return;
	for(int x = x0; x <= x1; x++){
		if(x >= width)
			break;
		if (y0 >= 0 && y0 < height)
			io_SetPixel(w, x, y0, color); /* Up Line */
		if (y1 >= 0 && y1 < height)
			io_SetPixel(w, x, y1, color); /* Down Line */
	}
	for(int y = y0; y <= y1; y++) {
		if(y >= height)
			break;
		if (x0 >= 0 && x0 < width)	
			io_SetPixel(w, x0, y, color); /* Left Line */
		if (x1 >= 0 && x1 < width)
			io_SetPixel(w, x1, y, color); /* Right Line */
	}
};

#ifndef _FIXED_POINT
void DrawGradient(window *w, int x0, int y0, int x1, int y1, int c0, int c1) {
	if (x0 > x1) {
		swap_xy(&x0, &x1);
	}
	if (y0 > y1) {
		swap_xy(&y0, &y1);
	}
	if (ClipFill(w, &x0, &y0, &x1, &y1)) {
		return;
	}
	int r0,g0,b0,r1,g1,b1;
	UnmixColor(c0, &r0, &g0, &b0);
	UnmixColor(c1, &r1, &g1, &b1);
	for (int x = x0; x <= x1; x++) {
		for (int y = y0; y <= y1; y++) {
			float factorY = (float)(y - y0)/(y1 - y0);
			int r = r0 + (int)((r1 - r0)*factorY);
			int g = g0 + (int)((g1 - g0)*factorY);
			int b = b0 + (int)((b1 - b0)*factorY);
			int color = MixColor(r,g,b);
			io_SetPixel(w, x, y, color);
		}
	}
}
#endif

#ifdef _FIXED_POINT
void DrawGradient(window *w, int x0, int y0, int x1, int y1, int c0, int c1){
	if (x0 > x1) {
		swap_xy(&x0, &x1);
	}
	if (y0 > y1) {
		swap_xy(&y0, &y1);
	}
	if (ClipFill(w, &x0, &y0, &x1, &y1)) {
		return;
	}
	int r0,g0,b0,r1,g1,b1;
	UnmixColor(c0, &r0, &g0, &b0);
	UnmixColor(c1, &r1, &g1, &b1);
	for (int x = x0; x <= x1; x++) {
		for (int y = y0; y <= y1; y++) {
			fixed factorY = div(INT_TO_FIXED(y - y0),
					    INT_TO_FIXED(y1 - y0));
			int r = r0 + FIXED_TO_INT(mul(
						INT_TO_FIXED(r1 - r0),
						factorY));
			int g = g0 + FIXED_TO_INT(mul(
						INT_TO_FIXED(g1 - g0),
						factorY));
			int b = b0 + FIXED_TO_INT(mul(
						INT_TO_FIXED(b1 - b0),
						factorY));
			int color = MixColor(r,g,b);
			io_SetPixel(w, x, y, color);
		}
	}
}
#endif

void DrawText(window* w,int x,int y,int size,font* f,int color,char* text){
	int winw = io_GetWidth(w); int winh = io_GetHeight(w);
	float scale = stbtt_ScaleForPixelHeight(f->info, size);
	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(f->info, &ascent, &descent, &lineGap);
	int baseline = y + (int)(ascent * scale);
	int xpos = x;
	while (*text) {
		int codepoint = DecodeUTF8(&text);
		int width, height, xoffset, yoffset;
		unsigned char* glyph_bm = stbtt_GetCodepointBitmap(f->info, 0,
			scale, codepoint, &width, &height, &xoffset, &yoffset);
		for (int row = 0; row < height; row++) {
			for (int col = 0; col < width; col++) {
				unsigned char pixel_value=
							glyph_bm[row*width+col];
				if (pixel_value > 0) {
					int image_x = xpos + col + xoffset;
					int image_y = baseline + row + yoffset;
					if (image_x >= 0 &&
						   image_x < winw &&
						   image_y >= 0 &&
						   image_y < winh) {
						io_SetPixel(w, image_x,
						image_y, color);
					}
				}
			}
		}
		stbtt_FreeBitmap(glyph_bm, NULL);
		int advance, lsb;
		stbtt_GetCodepointHMetrics(f->info, codepoint, &advance, &lsb);
		xpos += (int)(advance * scale);
	}
};

/*_____________UTILITY_____________*/

void BlendAlpha(int bg, int *color){
	int r,g,b, r_bg, g_bg, b_bg;
	UnmixColor(*color, &r, &g, &b);
	UnmixColor(bg, &r_bg, &g_bg, &b_bg);
	float alpha = (float)(((*color) >> 24) & 0xFF) / 255.0f;
	r = r*alpha + r_bg*(1-alpha);
	g = g*alpha + g_bg*(1-alpha);
	b = b*alpha + b_bg*(1-alpha);
	*color = MixColor(r, g, b);
}

void DefaultPlot(window *w,int x,int y,int color,void *userdata){
	io_SetPixel(w,x,y,color);
};

font* LoadFont(char* path){
	FILE* f_file = fopen(path, "rb");
	if (!f_file) {
		printf("ERROR: Invalid font\n");;
		return NULL;
	};
	fseek(f_file, 0, SEEK_END);
	size_t font_size = ftell(f_file);
	fseek(f_file, 0, SEEK_SET);
	font* f = malloc(sizeof(font));
	if (!f) {
		printf("ERROR: No memory\n");
		return NULL;
	};
	f->buffer = malloc(font_size*sizeof(char));
	fread(f->buffer, 1, font_size, f_file);
	f->info = malloc(sizeof(stbtt_fontinfo));
	fclose(f_file);
	if (!stbtt_InitFont(f->info, f->buffer,
	     stbtt_GetFontOffsetForIndex(f->buffer, 0))) {
		printf("ERROR:TruetypeinitError\n");
		free(f);
		return NULL;
	}
	return f;
};

void FreeFont(font *f){
	free(f->buffer);
	free(f->info);
	free(f);
}

/*_____________STATICS_____________*/

#define INSIDE 0   // 0000
#define LEFT   1   // 0001
#define RIGHT  2   // 0010
#define BOTTOM 4   // 0100
#define TOP    8   // 1000

static int ComputeOutCode(int width, int height, int x, int y){
	int code = INSIDE;
	if (x < 0)
		code |= LEFT;
	else if (x >= width)
		code |= RIGHT;
	if (y < 0)
		code |= BOTTOM;
	else if (y >= height)
		code |= TOP;
	return code;
}
/* https://en.wikipedia.org/wiki/Cohen-Sutherland_algorithm */
static int CohenSutherland(window *w, int *x0, int *y0, int *x1, int *y1) {
	int max_x = io_GetWidth(w); int max_y = io_GetHeight(w);
	int outcode0 = ComputeOutCode(max_x, max_y, *x0, *y0);
	int outcode1 = ComputeOutCode(max_x, max_y, *x1, *y1);
	int accept = 0;
	while (1) {
		if (!(outcode0 | outcode1)) {  /* 100% on canvas */
			accept = 1;
			break;
		} else if (outcode0 & outcode1) { /* 100% out of border */
			break;
		} else {	/* need trim */
			int outcodeOut = (outcode1>outcode0)?(outcode1):(outcode0);
			int x, y;
			if (outcodeOut & TOP) {	
				x = TrimLineFindX(*x0,*y0,*x1,*y1,max_y-1);
				y = max_y - 1;
			} else if (outcodeOut & BOTTOM) { 
				x = TrimLineFindX(*x0,*y0,*x1,*y1,0);
				y = 0;
			} else if (outcodeOut & RIGHT) { 
				y = TrimLineFindY(*x0, *y0, *x1, *y1, max_x-1);
				x = max_x - 1;
			} else if (outcodeOut & LEFT) {
				y = TrimLineFindY(*x0, *y0, *x1, *y1, 0);
				x = 0;
			}
			if (outcodeOut == outcode0) {
				*x0 = x;
				*y0 = y;
				outcode0 = ComputeOutCode(max_x, max_y, *x0, *y0);
			} else {
				*x1 = x;
				*y1 = y;
				outcode1 = ComputeOutCode(max_x, max_y, *x1, *y1);
			}
		}
	}
	return accept;
};

static void DirectPlot(window *w,int x,int y,int color){
	io_SetPixel(w,x,y,color);
};

static void InversePlot(window *w,int x,int y, int color){
	io_SetPixel(w,y,x,color);
};

/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
static void Bresenham(Handler Plot, window *w,
			int x0, int y0, int x1, int y1, int color){
	int dx = ABS(x1 - x0);
	int dy = ABS(y1 - y0);
	int e = 0;
	int de = (dy + 1);
	int y = y0;
	int dir = (y1 - y0 > 0)?(1):(-1);
	for(int x = x0; x <= x1; x++){
		(Plot)(w,x,y,color);
		e = e + de;
		if(e >= (dx + 1)){
			y = y + dir;
			e = e - (dx + 1);
		};
	};
};

#ifndef _FIXED_POINT
static void Rasterize(window *w, /*draws triangle*/
		   int x1,int y1,int x2,int y2,int x3,int y3,
		   Plotter Plot, int color, void *userdata) {
	if (y1 > y2) { swap_xy(&x1, &x2); swap_xy(&y1, &y2); }
	if (y1 > y3) { swap_xy(&x1, &x3); swap_xy(&y1, &y3); }
	if (y2 > y3) { swap_xy(&x2, &x3); swap_xy(&y2, &y3); }
	int max = y3 - y1;
	for (int i = 0; i < max; i++) {
		int half = i > y2 - y1 || y2 == y1;
		int seg = half ? y3 - y2 : y2 - y1;
		float alpha = (float)i / max;
		float beta  = (float)(i - (half ? y2 - y1 : 0)) / seg;
		int a = x1 + (x3 - x1) * alpha;
		int b = (half)?(x2 + (x3 - x2) * beta):(x1+(x2 - x1)*beta);
		int h = y1 + i; /* draw horizontal line a,b,h */
		if (a > b) {
			swap_xy(&a, &b);
		}
		for (int x = a; x <= b; x++) {
			(Plot)(w, x, h, color,userdata);
		}
	}
};
#endif

#ifdef _FIXED_POINT
static void Rasterize(window *w, /*draws triangle*/
		   int x1,int y1,int x2,int y2,int x3,int y3,
		   Plotter Plot, int color, void *userdata) {
	if (y1 > y2) { swap_xy(&x1, &x2); swap_xy(&y1, &y2); }
	if (y1 > y3) { swap_xy(&x1, &x3); swap_xy(&y1, &y3); }
	if (y2 > y3) { swap_xy(&x2, &x3); swap_xy(&y2, &y3); }
	int max = y3 - y1;
	for (int i = 0; i < max; i++) {
		int half = i > y2 - y1 || y2 == y1;
		int seg = half ? y3 - y2 : y2 - y1;
		fixed alpha = div(INT_TO_FIXED(i),INT_TO_FIXED(max));
		fixed delta = INT_TO_FIXED( (i - ((half)?(y2 - y1):(0)) ));
		fixed beta  = div(delta, INT_TO_FIXED(seg));
		int a = x1 + FIXED_TO_INT( mul(INT_TO_FIXED(x3-x1),alpha) );
		int b = half ? 
			(x2 +  FIXED_TO_INT(mul(INT_TO_FIXED(x3-x2),beta)) ) 
			:
			(x1 +  FIXED_TO_INT(mul(INT_TO_FIXED(x2-x1),beta )));
		int h = y1 + i; /*draw horizontal line a,b,h*/
		if (a > b) {
			swap_xy(&a, &b);
		}
		for (int x = a; x <= b; x++) {
			(Plot)(w, x, h, color,userdata);
		}
	}
};
#endif

static int TriangleCheck(window *w,int x1,int y1,int x2,int y2,int x3,int y3){
	int width = io_GetWidth(w);
	int height = io_GetHeight(w);
	if (x1 < 0 && x2 < 0 && x3 < 0)
		return 1; /*Left Border*/
	if (x1 >= width && x2 >= width && x3 >= width)
		return 1; /*Right Border*/
	if (y1 < 0 && y2 < 0 && y3 < 0)
		return 1; /*Up Border*/
	if (y1 >= height && y2 >= height && y3 >= height)
		return 1; /*Down Border*/
	return 0;
};

/* https://en.wikipedia.org/wiki/Sutherland-Hodgman_algorithm */
enum edge {left,right,bottom,top};

static void Trim(enum edge e, int max, int x0, int y0,
			int x1, int y1, int *nx, int *ny) {
	if (e == left || e == right) {
		*ny = TrimLineFindY(x0, y0, x1, y1, max);
		*nx = max;
	}
	if (e == bottom || e == top) {
		*nx = TrimLineFindX(x0, y0, x1, y1, max);
		*ny = max;
	}
};

static int InsideEdge(int x, int y, enum edge e, int b){
	switch(e){
		case left: return x >= b;
		case right: return x <= b;
		case bottom: return y >= b;
		case top: return y <= b;
	}
	return 0;
};

#define NEW_POLYGON(name) \
		int name##_c = 3;\
		int name[7][2]
#define ASSIGMENT_POLYGON(a,b) \
		a##_c = b##_c;\
		memcpy(a, b, sizeof(a))
#define NEW_POINT(name) int name##_p[2]
#define X(point) ((point##_p)[0])
#define Y(point) ((point##_p)[1])
#define GET_X(poly,point) ((poly)[(point)][0])
#define GET_Y(poly,point) ((poly)[(point)][1])
#define COUNT(poly) (poly##_c)
#define ADD_POINT(poly,point)	poly[poly##_c][0] = point##_p[0];\
				poly[poly##_c][1] = point##_p[1];\
				poly##_c++
				
/* https://en.wikipedia.org/wiki/Sutherland-Hodgman_algorithm */
static void SutherlandHodgman(window *w,
			int x0, int y0,
			int x1, int y1,
			int x2, int y2,
			Plotter Plot, int color, void *userdata){
	int max_x = io_GetWidth(w) ; int max_y = io_GetHeight(w) ;
	NEW_POLYGON(out) = {{x0,y0},{x1,y1},{x2,y2},{0,0},{0,0},{0,0},{0,0}};
	int boards[4] = {0, max_x - 1, 0, max_y};
	for (enum edge e = left; e <= top; e++) {
		NEW_POLYGON(inp); 
		ASSIGMENT_POLYGON(inp,out);
		COUNT(out) = 0;
		for(int p = 0; p < COUNT(inp); p++){
			NEW_POINT(cur) = {GET_X(inp,p), GET_Y(inp,p)};
			NEW_POINT(prv) = {
				GET_X(inp, (p-1+COUNT(inp))%COUNT(inp) ),
				GET_Y(inp, (p-1+COUNT(inp))%COUNT(inp) ) };
			NEW_POINT(trm);
			Trim(e,boards[e],X(prv),Y(prv),X(cur),Y(cur),
						&(X(trm)),&(Y(trm)));
			if(InsideEdge(X(cur),Y(cur), e, boards[e])){
				if(!InsideEdge(X(prv),Y(prv), e, boards[e])){
					ADD_POINT(out,trm);
				}
			ADD_POINT(out,cur);
			} else if(InsideEdge(X(prv),Y(prv), e, boards[e])){
				ADD_POINT(out,trm);
			};
		};
	};/*End. Next draw output triangels*/
	for (int p = 1; p < COUNT(out) - 1 ; p++) {
		Rasterize(w,
			  GET_X(out,0),GET_Y(out,0),
			  GET_X(out,p),GET_Y(out,p),
			  GET_X(out,p+1),GET_Y(out,p+1),
			  Plot,color,userdata);
	}
}

static int ClipFill(window *w, int *x0, int *y0, int *x1, int *y1){
	int max_x = io_GetWidth(w); int max_y = io_GetHeight(w);
	if(*x1 < 0 || *y1 < 0){
		return 1;
	};
	if(*x0 >= max_x || *y0 >= max_y){
		return 1;
	}
	if(*x1 >= max_x){
		*x1 = max_x - 1;
	};
	if(*y1 >= max_y){
		*y1 = max_y - 1;
	};
	if(*x0 < 0){
		*x0 = 0;
	};
	if(*y0 < 0){
		*y0 = 0;
	};
	return 0;
}

static int ClipRectangle(window *w, int *x0, int *y0, int *x1, int *y1){
	int max_x = io_GetWidth(w); int max_y = io_GetHeight(w);
	if(*x1 < 0 || *y1 < 0){
		return 1;
	};
	if(*x0 >= max_x || *y1 >= max_y){
		return 1;
	}
	if(((*x0 < 0)&&(*y0 < 0))&&((*x1 >= max_x)&&(*y1 >= max_y))){
		return 1;
	}
	if(*x1 > max_x){
		*x1 = max_x;
	};
	if(*y1 > max_y){
		*y1 = max_y;
	};
	if(*x0 < 0){
		*x0 = -1;
	};
	if(*y0 < 0){
		*y0 = -1;
	};
	return 0;
};

static int DecodeUTF8(char **text){
	const unsigned char *s = (const unsigned char *)*text;
	int codepoint = 0;
	int bytes = 0;
	if (s[0] < 0x80) {
		codepoint = s[0];
		bytes = 1;
	} else if ((s[0] & 0xE0) == 0xC0) {
		codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		bytes = 2;
	} else if ((s[0] & 0xF0) == 0xE0) {
		codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) |
			(s[2] & 0x3F);
		bytes = 3;
	} else if ((s[0] & 0xF8) == 0xF0) {
		codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
			((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
		bytes = 4;
	}
	*text += bytes;
	return codepoint;
};
