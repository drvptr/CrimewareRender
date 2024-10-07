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
 *	@(#)basics.h	1.0 (Potr Dervyshev) 11/05/2024
 */
/* BASIC DRAWING PRIMITIVES */
#ifndef BASICS_H_SENTRY
#define BASICS_H_SENTRY
#include <math.h>
#include "algebra.h"
#include "../IO/io.h"

#define GET_W(image) ((image)[0])
#define GET_H(image) ((image)[1])
#define GET_COLOR(image,x,y) ((image)[(x)*((image)[1])+(y+2)])
#define ALPHA(color) ((((unsigned int)color) >> 24) < (unsigned int)0x7F)

typedef void(*Plotter)(window *w,int x,int y, int color, void *userdata);

typedef struct font_t font;

/* 1.PRIMITIVES-FUNCS */
void DrawPixel(window *w, int x, int y, int color);
void DrawLine(window *w, int x0, int y0, int x1, int y1, int color);
void DrawTriangle( window *w, int x1,int y1,int x2,int y2,int x3,int y3,
		    Plotter Plot, int color, void *userdata);
void DrawImage(window *w, int x0, int y0, int *image);
void DrawFill(window *w, int x0, int y0, int x1, int y1, int color);
void DrawRectangle(window *w, int x0, int y0, int x1, int y1, int color);
void DrawGradient(window *w,int x0,int y0,int x1,int y1,int c0,int c1);
void DrawText(window* w,int x,int y,int size, font* f, int color,char* text);

/* 2 UTILITY-FUNCS */
inline static int MixColor(int r, int g, int b){
	return 0xFF << 24 | r << 16 | g << 8 | b;
};

inline static void UnmixColor(int color, int *r, int *g, int *b){
	*r = (color >> 16) & 0xFF;
	*g = (color >> 8) & 0xFF;
	*b = color & 0xFF;
};

inline static int AdjustIntensity(int color, float intensity){
	intensity = ABS(intensity);
	int r = (color >> 16) & 0xFF;
	int g = (color >> 8) & 0xFF;
	int b = color & 0xFF;
	r = (int)(r * intensity);
	g = (int)(g * intensity);
	b = (int)(b * intensity);
	return 0xFF000000 | (r << 16) | (g << 8) | b;
}

inline static int ConvertToGrayARGB(int value, int max) {
	int gray = 255 - ABS(((float)value/max) * 255);
	if (value <= 0) gray = 0xFF;
	if (value >= max) gray = 0x00;
	int color = (0xFF << 24) | (gray << 16) | (gray << 8) | gray;
	return color;
}

font* LoadFont(char* path);
void FreeFont(font *f);
void DefaultPlot(window *w,int x,int y,int color,void *userdata);
#define TRIANGLE(w,x1,y1,x2,y2,x3,y3,color)\
       DrawTriangle((w),(x1),(y1),(x2),(y2),(x3),(y3),DefaultPlot,(color),NULL)\

/*Simply draws a filled triangle (if that's the application you need)
USAGE: 

	DrawTriangle(w,300,300,100,100,220,500,DefaultPlot,0xFFAA2020,NULL);
	
or

	TRIANGLE(w,00,300,100,100,220,500,0xFFAA2020);
	
you can create custom plotter-function as "Plotter" profile (see above):

	DrawTriangle(w,300,300,100,100,220,500,RandomColorPlot,NULL,NULL);
	DrawTriangle(w,300,300,100,100,220,500,TexturePlot,intensy,&texture);
	DrawTriangle(w,300,300,100,100,220,500,GradientPlot,NULL,&colors_data);
	
	*/

#endif
