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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "tgatool.h"

//---------------------------------[SOME_MATH]----------------------------------

#define ABS(number) (((number) > 0)?(number):(-(number)))

static int expon(int base, int power){
	if(power == 0){
		return 1;
	};
	int multiper = base;
	for(int i = 1; i < power; i++){
		base = base * multiper;
	};
	return base;
};

//---------------------------[PIXEL_CONVERSATIONS]-----------------------------

#define PICK_COMPONENT(color, shift, bits_per_component, comp_var) \
		comp_var = color;\
		comp_var = (comp_var) << (64 - (shift));\
		comp_var = (comp_var) >> (64 - (bits_per_component));

typedef struct tag_pixel {
	size_t	  bpp;		//bytes per pixel
	uint64_t data;
} pixel;

static uint64_t adjust_to_depth(uint8_t component, unsigned char bits_per_component){
	uint64_t volume = expon(2,bits_per_component) - 1;
	uint64_t comp_perc = ((uint64_t)component*100)/255;
	return (comp_perc*volume)/100;
};

static uint8_t adjust_to_32(uint64_t component, unsigned char bits_per_component){
	uint64_t volume = expon(2,bits_per_component) - 1;
	component = component*100;
	uint64_t comp_perc = component/volume;
	if((component % volume) >= (volume/2))
		comp_perc++;
	uint8_t result;
	comp_perc = comp_perc * 255;
	result = (uint8_t)(comp_perc/100);
	if((comp_perc % 100) >= 50)
		result++;
	return result;
};

static pixel solidcolor(uint8_t red, uint8_t green, uint8_t blue, uint8_t depth) {
	pixel result;
	unsigned char bias = depth / 3;
	unsigned char adj = 0;
	if(depth % 3 > 1){
		adj = 1;
	};
	uint64_t r = adjust_to_depth(red, bias + adj);
	uint64_t g = adjust_to_depth(green, bias + adj);
	uint64_t b = adjust_to_depth(blue, bias);
	result.bpp = depth/8;
	result.data = r << (2*bias + adj) | g << bias | b;
	return result;
};

static pixel alphacolor(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t dph) {
	pixel result;
	unsigned char bias = dph / 4;
	uint64_t red = adjust_to_depth(r, bias);
	uint64_t green = adjust_to_depth(g, bias);
	uint64_t blue = adjust_to_depth(b, bias);
	uint64_t alpha = adjust_to_depth(a, bias);
	result.bpp = dph/8;
	result.data = alpha <<(3*bias) | red <<(2*bias) | green << bias | blue;
	return result;
};

static pixel color(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha, uint8_t depth){
	pixel result;
	if(depth % 8){
		result.bpp = 0;
		return result;
	};
	if((depth == 8) || !(depth % 3)){
		if(depth == 24){
			result.bpp = 3;
			result.data = (r << 16 | g << 8 | b) ;
		}
		else{
			result = solidcolor(r, g, b, depth);
		};
		return result;
	};
	if(depth == 16){
		result = solidcolor(r, g, b, depth);
		if(alpha > 127){
			result.data = ((1 << 15) | result.data);
			return result;
		}
		else {
			return result;
		};
	};
	result = alphacolor(r,g,b,alpha,depth);
	return result;
};

static int pixel_to_int(pixel input){
	int result;
	uint64_t oldr,oldg,oldb,olda;
	uint8_t r,g,b,a;
	a = 0;
	uint8_t depth = input.bpp * 8;
	if((depth == 8) || !(depth % 3)){
		if(depth == 24){
			PICK_COMPONENT(input.data, 8, 8, oldb)
			b = (uint8_t)oldb;
			PICK_COMPONENT(input.data, 16, 8, oldg)
			g = (uint8_t)oldg;
			PICK_COMPONENT(input.data, 24, 8, oldr)
			r = (uint8_t)oldr;
		}
		else {
			unsigned char bias = depth / 3;
			unsigned char adj = 0;
			if(depth % 3 > 1){
				adj = 1;
			};
			PICK_COMPONENT(input.data, bias, bias, oldb)
			b = adjust_to_32(oldb, bias);
			PICK_COMPONENT(input.data, (2*bias+adj), (bias+adj), oldg)
			g = adjust_to_32(oldg, bias+adj);
			PICK_COMPONENT(input.data, (3*bias+2*adj), (bias+adj), oldr)
			r = adjust_to_32(oldr, bias+adj);
		};
	}
	else{
		if(depth == 16){
			PICK_COMPONENT(input.data, 5, 5, oldb)
			b = adjust_to_32(oldb, 5);
			PICK_COMPONENT(input.data, 10, 5, oldg)
			g = adjust_to_32(oldg, 5);
			PICK_COMPONENT(input.data, 15, 5, oldr)
			r = adjust_to_32(oldr, 5);
			PICK_COMPONENT(input.data, 16,1, olda)
			if(olda != 0){
				a = 0xFF;
			};
		}
		else {
			unsigned char bias = depth / 4;
			PICK_COMPONENT(input.data, bias, bias, oldb)
			b = adjust_to_32(oldb, bias);
			PICK_COMPONENT(input.data, 2*bias, bias, oldg)
			g = adjust_to_32(oldg, bias);
			PICK_COMPONENT(input.data, 3*bias, bias, oldr)
			r = adjust_to_32(oldr, bias);
			PICK_COMPONENT(input.data, 4*bias, bias, olda)
			a = adjust_to_32(olda, bias);
		};
	};
	result = r << 24 | g << 16 | b << 8 | a;
	return result;
};

static void int_to_pixel(int input, pixel *output){
	uint8_t r,g,b,a;
	uint8_t depth = output->bpp * 8;
	r = input >> 24;
	g = input >> 16;
	b = input >> 8;
	a = input;
	*output = color(r,g,b,a,depth);
};

//-------------------------[FORMAT SPECIFIC FUNCTIONS]-------------------------

typedef enum tag_colormodes {
	incorrect = 0,
	grayscale = 1,
	truecolor = 2,
	palette = 3
} colormodes ;

typedef struct tag_tgaheaders{
	unsigned char   idlen;	   //id length (in bytes);
	unsigned char   cmapt;	   //color map type (1 or 0);
	unsigned char   imgtype;
	unsigned short  i;         //1st index of colormap array (OPTIONAL);
	unsigned short  cmaplen;   //color map length (in units)(OPTIONAL);
	unsigned char   cmapusz;   //color map unit size (in bits)(OPTIONAL);
	unsigned short  x_orig;
	unsigned short  y_orig;
	unsigned short  height;	   //image height (in elements)
	unsigned short  width;	   //image width;
	unsigned char   depth;	   //bits per pixel;
	unsigned char   a_depth:4; //bits per pixel for alpha-channel;
	unsigned char   order:2;   //pixel order 00,01,10,11;
	unsigned char   end:2;     //must be zero;
}__attribute__((packed)) tgaheaders;

typedef struct tag_tgafooters{
	unsigned int	exzone_offset;
	unsigned int	devzone_offset;   
}__attribute__((packed)) tgafooters;

static tgaheaders *gen_header(short w, short h, colormodes mode, char RLE){
	tgaheaders *newhdr = malloc(sizeof(tgaheaders));
	newhdr->idlen = 0;
	newhdr->width = w;
	newhdr->height = h;
	switch(mode) {
		case grayscale:
			(RLE)?(newhdr->imgtype = 11):(newhdr->imgtype = 3);
			newhdr->cmapt = 0;
			newhdr->depth = 8;
			break;
		case truecolor:
			(RLE)?(newhdr->imgtype = 10):(newhdr->imgtype = 2);
			newhdr->cmapt = 0;
			newhdr->depth = 24;
			break;
		case palette:
			(RLE)?(newhdr->imgtype = 9):(newhdr->imgtype = 1);
			newhdr->cmapt = 1;
			newhdr->depth = 8;
			break;
	};
	return newhdr;
};

static int grab_header(FILE *input, tgaheaders *output){
	long saved_position = ftell(input);
	rewind(input);
	if( fread(output,sizeof(tgaheaders),1,input) != 1) {
		fseek(input,saved_position,SEEK_SET);
		return 1;
	};
	return 0;
};

static int analyse_header(tgaheaders *input){
	char cmap = input->cmaplen != 0 || input->cmapusz != 0 || input->i != 0;
	if(input->width == 0 || input->height == 0){
		return 0;
	};
	if(input->end != 0){
		return 0;
	};
	switch(input->cmapt) {
		case 0:
			if(cmap){
				break;
			};
			switch(input->imgtype){
				case 2:
					if(input->depth == 24 || input->depth == 16 || input->depth == 32){
						return 2;
					};
					break;
				case 3:
					if(input->depth == 8){
						return 3;
					};
					break;
				case 10:
					if(input->depth == 24 || input->depth == 16 || input->depth == 32){
						return 10;
					};
					break;
				case 11:
					if(input->depth == 8){
						return 11;
					};
					break;
			};
			break;
		case 1:
			if(input->cmaplen == 0 || input->cmapusz == 0){
				break;
			};
			if(input->imgtype == 1) {
				if(input->depth == 8){
					return 1;
				};
			};
			if(input->imgtype == 9) {
				if(input->depth == 8){
					return 9;
				};
			};
			break;
	};
	return 0;
};

static char *grab_id(char length, FILE *input){
	long saved_pos = ftell(input);
	char *result = malloc(length);
	if(fread(result,1,length,input) != length){
				fseek(input,saved_pos,SEEK_SET);
				return NULL;
	};
	return result;
};

static int **init_canvas(short width, short height){
	int **canvas = malloc((1 + width) * sizeof(int*));
	for(short w = 0; w < width; w++){
		canvas[w]= (int *)malloc(height * sizeof(int));
	};
	canvas[width] = NULL;
	return canvas;
};

static int *grab_palette(FILE *input, short length, uint8_t depth){
	int *result = malloc(length*(depth/8));
	pixel current;
	current.bpp = depth/8;
	for(short i = 0; i < length; i++){
		if(fread(&(current.data),current.bpp,1,input) != 1)
			return NULL;
		result[i] = pixel_to_int(current);
	};
	return result;
};

static void grab_indexfield(FILE *input, short w, short h, int *cmap, int **output){
	int current_index;
	for(short x = 0; x < w; x++){
		for(short y = 0; y < h; y++){
			current_index = fgetc(input);
			output[x][y] = cmap[current_index];
		};
	};
};

static int grab_canvas(FILE *input, short w, short h, size_t bpp, int **output){
	pixel current;
	current.bpp = bpp;
	for(short x = 0; x < w; x++){
		for(short y = 0; y < h; y++){
			if(fread(&(current.data),current.bpp,1,input) != 1)
				return 1;
			output[x][y] = pixel_to_int(current);
		};
	};
	return 0;
};

static int rle_grab(FILE *input, short w, short h, size_t bpp, int **output){
	pixel current = {.bpp = bpp, .data = 0};
	unsigned char series;	//Repetition Count field
	unsigned char count = 0;
	char undefined = 1;
	unsigned char packet_detector = 0;
	for(short x = 0; x < w; x++){
		for(short y = 0; y < h; y++){
			if(undefined){
				series = fgetc(input);
				packet_detector = series;
				packet_detector = (packet_detector >> 7);
				series = (series << 1);
				series = (series >> 1);
			};
			if(packet_detector){
				if(undefined){
					if(fread(&(current.data),bpp,1,input) != 1){
						return -1;
					};
				};
				count++;
				undefined = 0;
				output[x][y] = pixel_to_int(current);
				if(count == (series + 1)){
					undefined = 1;
					count = 0;
				};
			}
			else {
				if(fread(&(current.data),bpp,1,input) != 1){
						return -1;
				};
				undefined = 0;
				count++;
				output[x][y] = pixel_to_int(current);
				if(count == (series + 1)){
					undefined = 1;
					count = 0;
				};
			};
		};
	};
	return 0;
};

static int grab_rleindexfield(FILE *input,short w,short h,int *cmap,int **output){
	int current_index;
	unsigned char series;	//Repetition Count field
	unsigned char count = 0;
	char undefined = 1;
	unsigned char packet_detector = 0;
	for(short x = 0; x < w; x++){
		for(short y = 0; y < h; y++){
			if(undefined){
				series = fgetc(input);
				packet_detector = series;
				packet_detector = (packet_detector >> 7);
				series = (series << 1);
				series = (series >> 1);
			};
			if(packet_detector){
				if(undefined){
					current_index = fgetc(input);
				};
				count++;
				undefined = 0;
				output[x][y] = cmap[current_index];
				if(count == (series + 1)){
					undefined = 1;
					count = 0;
				};
			}
			else {
				current_index = fgetc(input);
				undefined = 0;
				count++;
				output[x][y] = cmap[current_index];
				if(count == (series + 1)){
					undefined = 1;
					count = 0;
				};
			};
		};
	};
	return 0;
};

static void raw_write(int **canvas, short w, short h, uint8_t depth, FILE *output){
	pixel current;
	current.bpp = depth/8;
	for(short x = 0; x < w; x++){
		for(short y = 0; y < h; y++){
			int_to_pixel((canvas)[x][y], &current);
			fwrite(&(current.data),current.bpp,1,output);
		};
	};
};

static void rle_write(int **canvas, short w, short h, uint8_t depth, FILE *output){
	pixel p;
	p.bpp = depth/8;
	int cur, prv;
	unsigned char raw_c = 0;
	unsigned char rle_c = 0;
	int *buffer = malloc(w*sizeof(int));
	for(short x = 0; x < w; x++){
		//FIRST PAIR INIT
		cur = canvas[x][0];
		//MAIN LINE
		for(short y = 1; y < h; y++){
			prv = cur;
			cur = canvas[x][y];
			if(cur == prv){
				rle_c++;
				if(rle_c > 127){
					rle_c--;
					rle_c |= 0x80;
					fwrite(&rle_c,1,1,output);
					int_to_pixel(prv,&p);
					fwrite(&(p.data),p.bpp,1,output);
					rle_c = 0;
					continue;
				};
				if(raw_c != 0){
					raw_c--;
					fwrite(&raw_c,1,1,output);
					for(int i = 0; i <= raw_c; i++){
						int_to_pixel(buffer[i],&p);
						fwrite(&(p.data),p.bpp,1,output);
					};
					raw_c = 0;
				};
				continue;
			}
			else {
				if(rle_c != 0){
					rle_c = rle_c|0x80;
					fwrite(&rle_c,1,1,output);
					int_to_pixel(prv,&p);
					fwrite(&(p.data),p.bpp,1,output);
					rle_c = 0;
					continue;
				}
				else {
					buffer[raw_c] = prv;
					raw_c++;
				};
				if(raw_c > 127){
					raw_c--;
					fwrite(&raw_c,1,1,output);
					for(int i = 0; i <= raw_c; i++){
						int_to_pixel(buffer[i],&p);
						fwrite(&(p.data),p.bpp,1,output);
					};
					raw_c = 0;
					continue;
				};
			};
		};
		//END OF LINE (EOF)
		if(cur != prv){
			if(raw_c != 0){
				buffer[raw_c] = cur;
				fwrite(&raw_c,1,1,output);
				for(int i = 0; i <= raw_c; i++){
					int_to_pixel(buffer[i],&p);
					fwrite(&(p.data),p.bpp,1,output);
				};
				raw_c = 0;
			}
			else {
				fwrite(&raw_c,1,1,output);
				int_to_pixel(cur,&p);
				fwrite(&(p.data),p.bpp,1,output);
			};
		}
		else{
			if(rle_c != 0)
				rle_c |= 0x80;
			fwrite(&rle_c,1,1,output);
			int_to_pixel(prv,&p);
			fwrite(&(p.data),p.bpp,1,output);
			rle_c = 0;
		};
		//go to new line..
	};
	free(buffer);
};

static void free_canvas(int **canvas){
	short i = 0;
	do{
		free(canvas[i]);
		i++;
	} while(canvas[i] != NULL);
	free(canvas);
};

static unsigned int get_ex_offset(FILE *input){
	unsigned int result;
	long saved_pos = ftell(input);
	fseek(input,-26,SEEK_END);
	fread(&result,sizeof(int),1,input);
	fseek(input,saved_pos,SEEK_SET);
	return result;
};

static unsigned int get_dev_offset(FILE *input){
	unsigned int result;
	long saved_pos = ftell(input);
	fseek(input,-22,SEEK_END);
	fread(&result,sizeof(int),1,input);
	fseek(input,saved_pos,SEEK_SET);
	return result;
};

static tgafooters *sign_footer(int ex_offset, int dev_offset){
	tgafooters *result = malloc(sizeof(tgafooters)+18);
	result->exzone_offset = ex_offset;
	result->devzone_offset = dev_offset;
	return result;
};

//---------------------------[APPLICATION_MODULE]------------------------------

TGAimage *create_image(short width, short height, image_types type){
	if(width == 0 || height == 0){
		return NULL;
	};
	char rle_enable = (type == 9 || type == 10 || type == 11);
	colormodes mode = incorrect;
	if(type == 1 || type == 9)
		mode = palette;
	if(type == 2 || type == 10)
		mode = truecolor;
	if(type == 3 || type == 11)
		mode = grayscale;
	if(mode == incorrect)
		return NULL;
	TGAimage *result = malloc(sizeof(TGAimage));
	if( result == NULL)
		return NULL;
	tgaheaders *blank_header = gen_header(width,height,mode,rle_enable);
	if(blank_header == NULL){
		free(blank_header);
		return NULL;
	};
	result->header = blank_header;
	result->canvas = init_canvas(blank_header->width,blank_header->height);
	if(result->canvas == NULL){
		free(blank_header);
		free(result);
		return NULL;
	};
	return result;
};

int set_pixel(TGAimage *dest, unsigned short x, unsigned short y, int clr){
	if(dest == NULL){
		return -1;
	};
	tgaheaders *dummy;
	dummy = dest->header;
	if((x >= dummy->width) || (y >= dummy->height)){
		return -2;
	};
	(dest->canvas)[x][y] = clr;
	return 0;
};


int get_pixel(TGAimage *dest, unsigned int x, unsigned int y){
	if(dest == NULL){
		return 0x00000000;
	};
	tgaheaders *dummy;
	dummy = dest->header;
	if(x >= (unsigned int)dummy->width){
		do {
			x = x - (unsigned int)dummy->width;
		} while(x < (unsigned int)dummy->width);
	};
	if(y >= (int)dummy->height){
		do {
			y = y - (unsigned int)dummy->width;
		} while(y < (unsigned int)dummy->width);
	};
	return RGBA_to_ARGB((unsigned int)(dest->canvas)[y][x]);
};

void set_mode(TGAimage *image, image_types new_mode){
	if(image == NULL || image->header == NULL || new_mode == no_image)
		return;
	tgaheaders *dummy;
	dummy = image->header;
	switch(new_mode){
		case uncompressed_colormapped:
			printf("Undeveloped mode\nСhoose another one\nSorry :(\n");
			break;
		case uncompressed_truecolor:
			dummy->cmapt = 0;
			dummy->imgtype = 2;
			dummy->i = 0;
			dummy->cmaplen = 0;
			dummy->cmapusz = 0;
			dummy->depth = 24;
			break;
		case uncompressed_grayscale:
			dummy->cmapt = 0;
			dummy->imgtype = 3;
			dummy->i = 0;
			dummy->cmaplen = 0;
			dummy->cmapusz = 0;
			dummy->depth = 8;
			break;
		case RLE_colormapped:
			printf("Undeveloped mode\nСhoose another one\nSorry :(\n");
			break;
		case RLE_truecolor:
			dummy->cmapt = 0;
			dummy->imgtype = 10;
			dummy->i = 0;
			dummy->cmaplen = 0;
			dummy->cmapusz = 0;
			dummy->depth = 24;
			break;
		case RLE_grayscale:
			dummy->cmapt = 0;
			dummy->imgtype = 11;
			dummy->i = 0;
			dummy->cmaplen = 0;
			dummy->cmapusz = 0;
			dummy->depth = 8;
			break;
	};
};

short get_width(TGAimage *image){
	if(image == NULL || image->header == NULL)
		return 0;
	tgaheaders *dummy;
	dummy = image->header;
	return dummy->width;
};

short get_height(TGAimage *image){
	if(image == NULL || image->header == NULL)
		return 0;
	tgaheaders *dummy;
	dummy = image->header;
	return dummy->height;
};

int save_image(TGAimage *image, char *filename){
	if(filename[0] == '\0')
		return 1;
	if(image == NULL)
		return 2;
	FILE *output = fopen(filename,"w+b");
	if(output == NULL)
		return 3;
	fwrite(image->header,sizeof(tgaheaders),1,output);
	tgaheaders *dummy;
	dummy = image->header;
	if(dummy->idlen != 0){
		if(image->id == NULL)
			return 4;
		if(fwrite(image->id,1,dummy->idlen,output) != dummy->idlen)
			return 5;
	};
	if(dummy->imgtype == 9 || dummy->imgtype == 10 || dummy->imgtype == 11){
		if(dummy->imgtype == 9){
			printf("Undeveloped type\nSaved in RLE truecolor format\nSorry :(\n");
			rewind(output);
			tgaheaders plug;
			plug.idlen = 0;
			plug.cmapt = 0;
			plug.imgtype = 10;
			plug.i = 0;
			plug.cmaplen = 0;
			plug.cmapusz = 0;
			plug.depth = 24;
			plug.x_orig = 0;
			plug.y_orig = 0;
			plug.width = dummy->width;
			plug.height = dummy->height;
			plug.order = 0;
			plug.end = 0;
			fwrite(&plug,sizeof(tgaheaders),1,output);
			rle_write(image->canvas,dummy->width,dummy->height,plug.depth,output);
		}
		else{
			rle_write(image->canvas,dummy->width,dummy->height,dummy->depth,output);
		};
	}
	else {
		if(dummy->imgtype == 1){
			printf("Undeveloped type\nSaved in uncompressed truecolor format\nSorry :(\n");
			rewind(output);
			tgaheaders plug;
			plug.idlen = 0;
			plug.cmapt = 0;
			plug.imgtype = 2;
			plug.i = 0;
			plug.cmaplen = 0;
			plug.cmapusz = 0;
			plug.depth = 24;
			plug.x_orig = 0;
			plug.y_orig = 0;
			plug.width = dummy->width;
			plug.height = dummy->height;
			plug.order = 0;
			plug.end = 0;
			fwrite(&plug,sizeof(tgaheaders),1,output);
			raw_write(image->canvas,dummy->width,dummy->height,plug.depth,output);
		}
		else{
			raw_write(image->canvas,dummy->width,dummy->height,dummy->depth,output);
		};
	};
	if(image->footer != NULL){
		fwrite(image->footer,2*sizeof(int),1,output);
		char signature[18] = "TRUEVISION-XFILE.\0";
		fwrite(signature,1,18,output);
	}
	fclose(output);
	return 0;
};

TGAimage *open_image(char *filename){
	if(filename[0] == '\0'){
		return NULL;
	};
	TGAimage *result = malloc(sizeof(TGAimage));
	if( result == NULL){
		return NULL;
	};
	FILE *input = fopen(filename,"r");
	if(input == NULL){
		return NULL;
	};
	tgaheaders *hdr = malloc(sizeof(tgaheaders));
	if(hdr == NULL){
		goto bad_end_0;
	};
	if( grab_header(input, hdr) != 0 ){
		goto bad_end_1;
	};
	int imgtype;
	if((imgtype = analyse_header(hdr)) == 0 ){
		goto bad_end_1;
	};
	result->header = hdr;
	if(hdr->idlen != 0){
		result->id = grab_id(hdr->idlen, input);
	};
	if(imgtype == 1 || imgtype == 9){
		//MALLOC INCLUDED
		result->color_map = grab_palette(input, hdr->cmaplen, hdr->cmapusz);
		if(result->color_map == NULL){
			goto bad_end_1;
		};
	};
	result->canvas = init_canvas(hdr->width,hdr->height);
	if(result->canvas == NULL){
		goto bad_end_1;
	};
	size_t bpp = (hdr->depth)/8;
	int open_err_c;
	if(imgtype == 9 || imgtype == 10 || imgtype == 11){
		if(imgtype == 9){
			open_err_c = grab_rleindexfield(input, hdr->width, hdr->height, result->color_map, result->canvas);
		}
		else{
			open_err_c = rle_grab(input,hdr->width,hdr->height,bpp,result->canvas);
		};
	}
	else {
		if(imgtype == 1){
			grab_indexfield(input, hdr->width, hdr->height, result->color_map, result->canvas);
			if(result->canvas != NULL)
				open_err_c = 0;
		}
		else {
			open_err_c = grab_canvas(input,hdr->width,hdr->height,bpp,result->canvas);
		};
		
	};
	if(open_err_c != 0){
		goto bad_end_2;
	};
	int ex = get_ex_offset(input);
	int dev = get_dev_offset(input);
	result->footer = sign_footer(ex, dev); //MALLOC INCLUDED
	return result;
	bad_end_2:
	free(result->canvas);
	bad_end_1:
	free(hdr);
	bad_end_0:
	free(result);
	return NULL;
};

#ifdef _WIN32
FILE *fmemopen(unsigned char arr[], unsigned int len, const char *mode);
#endif

TGAimage *open_embed_image(unsigned char arr[], unsigned int len){
	TGAimage *result = malloc(sizeof(TGAimage));
	if( result == NULL){
		return NULL;
	};
	FILE *input = fmemopen(arr,len,"rb");
	if(input == NULL){
		return NULL;
	};
	tgaheaders *hdr = malloc(sizeof(tgaheaders));
	if(hdr == NULL){
		goto bad_end_0;
	};
	if( grab_header(input, hdr) != 0 ){
		goto bad_end_1;
	};
	int imgtype;
	if((imgtype = analyse_header(hdr)) == 0 ){
		goto bad_end_1;
	};
	result->header = hdr;
	if(hdr->idlen != 0){
		result->id = grab_id(hdr->idlen, input);
	};
	if(imgtype == 1 || imgtype == 9){
		//MALLOC INCLUDED
		result->color_map = grab_palette(input, hdr->cmaplen, hdr->cmapusz);
		if(result->color_map == NULL){
			goto bad_end_1;
		};
	};
	result->canvas = init_canvas(hdr->width,hdr->height);
	if(result->canvas == NULL){
		goto bad_end_1;
	};
	size_t bpp = (hdr->depth)/8;
	int open_err_c;
	if(imgtype == 9 || imgtype == 10 || imgtype == 11){
		if(imgtype == 9){
			open_err_c = grab_rleindexfield(input, hdr->width, hdr->height, result->color_map, result->canvas);
		}
		else{
			open_err_c = rle_grab(input,hdr->width,hdr->height,bpp,result->canvas);
		};
	}
	else {
		if(imgtype == 1){
			grab_indexfield(input, hdr->width, hdr->height, result->color_map, result->canvas);
			if(result->canvas != NULL)
				open_err_c = 0;
		}
		else {
			open_err_c = grab_canvas(input,hdr->width,hdr->height,bpp,result->canvas);
		};
		
	};
	if(open_err_c != 0){
		goto bad_end_2;
	};
	int ex = get_ex_offset(input);
	int dev = get_dev_offset(input);
	result->footer = sign_footer(ex, dev); //MALLOC INCLUDED
	return result;
	bad_end_2:
	free(result->canvas);
	bad_end_1:
	free(hdr);
	bad_end_0:
	free(result);
	return NULL;
};

void eject_image(TGAimage *existing_image){
	if(existing_image->footer != NULL)
		free(existing_image->footer);
	free_canvas(existing_image->canvas);
	if(existing_image->color_map != NULL)
		free(existing_image->color_map);
	if(existing_image->id != NULL)
		free(existing_image->id);
	free(existing_image->header);
	free(existing_image);
};



