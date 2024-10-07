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
 *	@(#)tgatool.c	1.0 (Potr Dervyshev) 11/05/2024
 */
 
#ifndef TGATOOL_H_SENTRY
#define TGATOOL_H_SENTRY


typedef enum tag_image_types{ 
	no_image = 0,
	uncompressed_colormapped = 1,
	uncompressed_truecolor = 2,
	uncompressed_grayscale = 3,
	RLE_colormapped = 9,
	RLE_truecolor = 10,
	RLE_grayscale = 11 
} image_types;
//NOTE: saving colormaped pictures is not developed yet,
//	but you can open colormaped pictures and save is
//	as truecolor.

typedef struct tag_tga_image{
	struct headers *header;
	char *id;		//(OPTIONAL)
	int *color_map;		//(OPTIONAL)
	int **canvas;
	struct footers *footer;//(OPTIONAL)
} TGAimage;
//USAGE:  TGAimage *newimg1;

TGAimage *create_image(short width, short height, image_types type);
//USAGE:  TGAimage *newimg1 = create_image(100, 150, RLE_truecolor);
//NOTE: Creates a new empty image for the pointer.

TGAimage *open_image(char *filename);
//USAGE: TGAimage *newimg1 = open_image("./myfatcock.tga");
//NOTE: Opening existing tga-image.

int set_pixel(TGAimage *dest, unsigned short x, unsigned short y, int clr);
//USAGE:  set_pixel(newimg1,50,75, 0x00AA00FF);
//NOTE: Drawing a pixel in x,y coordinate.
//RETURN VALUE: 0 Sucsess, -1  No image, -2 Pixel is out of canvas

int get_pixel(TGAimage *dest, unsigned int x, unsigned int y);

void set_mode(TGAimage *image, image_types new_mode);
//USAGE:  set_mode(newimg1,uncompressed_truecolor);
//NOTE: Change header of image for new mode.

short get_width(TGAimage *image);
//USAGE:  printf("width:%hi\n",get_width(newimg));
//NOTE: Returning the width of image;

short get_height(TGAimage *image);
//USAGE:  printf("height:%hi\n",get_height(newimg));

int save_image(TGAimage *image, char *filename);
//USAGE:  save_image(newimg, "untitled.tga");
//NOTE:	  Writing the picure into file system (but image not remove form RAM)

void eject_image(TGAimage *existing_image);
//USAGE:  save_image(newimg);
//NOTE:	  Remove image from RAM. Like function free(), but for images.

int RGBA_to_ARGB(int color);

TGAimage *open_embed_image(unsigned char arr[], unsigned int len);

#endif
