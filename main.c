#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include "IO/io.h"
#include "GRAPHIC/algebra.h"
#include "GRAPHIC/tgatool.h"
#include "GRAPHIC/wavefront.h"
#include "GRAPHIC/basics.h"
#include "GRAPHIC/render3d.h"



#define R 400

int main(){
	wavefront_obj *obj = ImportObj("freebsd.obj");
	WavefrontPrintLog(obj);
	TGAimage *texture = open_image("freebsd.tga");
	if(obj == NULL){
		fprintf(stderr,"nothing to render\n");
		return 1;
	}
	window *w = io_InitWindow();
	camera *cam = InitCamera(w,0,0,0,0,0,0,400);
	float t = 0;
	while(1){
		vector new_pos = { R*cosf(t), R*sinf(t), 0 };
		t = t + 0.01;
		if(t >= M_PI*2){
			t = t - M_PI*2;
		}
		DrawGradient(w,0,0,io_GetWidth(w),io_GetHeight(w),0xEEFFFE,0x8080FF);
		MoveCamera(cam, new_pos, NULL);
		/* RENDERERS DEMONSTRATION (UNCOMENT ONE) */
		//RenderShaded(w, cam, obj, 0xFFAA0000);
		//RenderTextured(w, cam, obj, texture);
		RenderGouraud(w, cam, obj, texture, 0xFF212121);
		//RenderZBuffer(w, cam, obj, 8000);
		//RenderWireframe(w, cam, obj, 0xFF121212);
		io_UpdateFrame(w);
	};
	eject_image(texture);
	FreeCamera(cam);
	FreeObj(obj);
	io_CloseWindow(w);
	return 0;
};
