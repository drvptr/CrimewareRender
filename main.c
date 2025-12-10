#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include "io.h"
#include "wavefront.h"
#include "render3d.h"

/*io_window_t *exception_win = NULL;

void quit_handler(int none){
	if(exception_win)
		io_CloseWindow(exception_win);
}

signal(SIGINT, quit_handler);
signal(SIGTERM, quit_handler);

exception_win = w;*/

#define PI 3.1415926535

#define RGB(r,g,b) (((r)<<16)|((g)<<8)|(b))

void DrawBackground(io_window_t *w, int width, int height){
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			io_SetPixel(w, x, y, RGB(x, y, 128));
		}
	}
}

int main(void) {
	io_keys_t *c = io_InitKeys();
	io_window_t *w = io_InitWindow();
	wf_wavefront_t *new = wf_LoadWavefront("freebsd.obj");
	if(new == NULL)
		return 1;
	//wf_WavefrontCalculateNormals(new);
	int playloop = 1;
	r3_camera_t *cam  = r3_InitCamera(w,700,800,0,0,0,0);
	while (playloop) {
		io_PollKeys(w, c, 0);
		if(c->status[KEY_ESC] == IO_TOGGLED)
			playloop = 0;
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		r3_RenderMesh(w, new, NULL);
		io_UpdateFrame(w);
	}
	io_CloseWindow(w);
	io_FreeKeys(c);
	wf_RemoveWavefront(new);
	r3_RemoveCamera(cam);
	return 0;
}
