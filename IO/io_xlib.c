#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "io.h"

static int ConvertKeysyms(int keysym);
static void DisableKeyRepeat(Display *display);
static void EnableKeyRepeat(Display *display);

struct window_t{
	Display *dsp;
	Window win;
	int scr;
	GC gc;
	XImage *buf;
	int width;
	int height;
};

window *io_InitWindow(){
	Display *dsp = XOpenDisplay(NULL);
	if (dsp == NULL) {
		fprintf(stderr, "Cant open display\n");
		exit(1);
	}
	int scr = DefaultScreen(dsp);
	Window win = XCreateSimpleWindow(dsp, RootWindow(dsp, scr),
		 10, 10, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
		 1, BlackPixel(dsp, scr), WhitePixel(dsp, scr));
	XStoreName(dsp, win, TITLE);
	XSelectInput(dsp, win, ExposureMask | KeyPressMask | KeyReleaseMask |
			FocusChangeMask | ButtonPressMask | ButtonReleaseMask |
			PointerMotionMask | StructureNotifyMask);
	XMapWindow(dsp, win);
	GC gc = XCreateGC(dsp, win, 0, NULL);
	window *res = malloc(sizeof(window));
	res->width = DEFAULT_WINDOW_WIDTH;
	res->height = DEFAULT_WINDOW_HEIGHT;
	char *canvas = malloc(res->width * res->height * sizeof(int));
	XImage *buffer = XCreateImage(dsp, DefaultVisual(dsp, scr),
			DefaultDepth(dsp, scr),ZPixmap, 0, 
			canvas, res->width, res->height, 32, 0);
	res->win = win;
	res->dsp = dsp;
	res->scr = scr;
	res->gc = gc;
	res->buf = buffer;
	return res;
};

int io_GetWidth(window *w){
	return w->width;
};

int io_GetHeight(window *w){
	return w->height;
};

void io_SetPixel(window *w, int x, int y, int color){
	XPutPixel(w->buf,x,y,color);
};

int io_GetPixel(window *w, int x, int y){
	return (int)XGetPixel(w->buf,x,y);
};

void io_UpdateFrame(window *w){
	XPutImage(w->dsp, w->win, w->gc, w->buf,
		  0, 0, 0, 0, io_GetWidth(w), io_GetHeight(w));
};

void io_CloseWindow(window *w){
	EnableKeyRepeat(w->dsp);
	XDestroyImage(w->buf);
	XFreeGC(w->dsp, w->gc);
	XDestroyWindow(w->dsp, w->win);
	XCloseDisplay(w->dsp);
	free(w);
};

controls *io_InitControl(){
	controls *c = malloc(sizeof(controls));
	c->type = none;
	for(int i = 0; i <= MAX_KEYS; i++){
		HOLD(c,i) = 0;
		TOGGLE(c, i) = 0;
	};
	c->x = 0; c->y = 0;
	return c;
};

void io_PollControls(window *w, controls *c, int mode){
	c->type = none;
	if ((XPending(w->dsp) > 0) || mode){
		XEvent e;
		XNextEvent(w->dsp, &e);
		if(e.type == FocusIn) {
			DisableKeyRepeat(w->dsp);
		};
		if (e.type == FocusOut) {
			EnableKeyRepeat(w->dsp);
		};
		int key;
		if (e.type == KeyPress) {
			c->type = press;
			key = ConvertKeysyms(XLookupKeysym(&e.xkey, 0));
			HOLD(c, key) = 1;
			TOGGLE(c, key) = !TOGGLE(c, key);
		};
		if (e.type == KeyRelease) {
			c->type = release;
			key = ConvertKeysyms(XLookupKeysym(&e.xkey, 0));
			HOLD(c, key) = 0;
		};
		if (e.type == ButtonPress) {
			c->type = press;
			key = e.xbutton.button + 100;
			HOLD(c, key) = 1;
			TOGGLE(c, key) = !TOGGLE(c, key);
		};
		if (e.type == ButtonRelease) {
			c->type = release;
			key = e.xbutton.button + 100;
			HOLD(c, key) = 0;
		};
		if (e.type == MotionNotify) {
			MOUSE_X(c) = e.xmotion.x;
			MOUSE_Y(c) = e.xmotion.y;
		}
		if (e.type == ConfigureNotify) {
			XWindowAttributes a;
			XGetWindowAttributes(w->dsp,w->win,&a);
			w->width = a.width;
			w->height = a.height;
			XDestroyImage(w->buf);
			char *canvas = malloc(w->width*w->height*sizeof(int));
			XImage *new_buffer = XCreateImage(w->dsp, 
					DefaultVisual(w->dsp, w->scr),
					DefaultDepth(w->dsp, w->scr),ZPixmap,0, 
					canvas, w->width, w->height, 32, 0);
			w->buf = new_buffer;
		}
	}
};

/*STATIC FUNCTIONS*/
static void DisableKeyRepeat(Display *display){
	XKeyboardControl control;
	control.auto_repeat_mode = AutoRepeatModeOff;
	XChangeKeyboardControl(display, KBAutoRepeatMode, &control);
}

static void EnableKeyRepeat(Display *display){
	XKeyboardControl control;
	control.auto_repeat_mode = AutoRepeatModeOn;
	XChangeKeyboardControl(display, KBAutoRepeatMode, &control);
}

static int ConvertKeysyms(int keysym){
	if (keysym == 32) {
		return 0;
	}
	if (keysym == 39) {
		return 1;
	}
	if ( (keysym >= 44) && (keysym <= 59)) {
		return keysym - (44 - 2);
	}
	if ( keysym == 61 ) {
		return 18;
	}
	if ( (keysym >= 91) && (keysym <= 93)) {
		return keysym - (91 - 19);
	}
	if ( (keysym >= 96) && (keysym <= 122)) {
		return keysym - (96 - 22);
	}
	if ( (keysym >= 65288) && (keysym <= 65289)) {
		return keysym - (65288 - 49);
	}
	if ( keysym == 65293 ) {
		return 51;
	}
	if ( keysym == 65299 ) {
		return 52;
	}
	if ( keysym == 65307 ) {
		return 54;
	}
	if ( (keysym >= 65360) && (keysym <= 65367)) {
		return keysym - (65360 - 55);
	}
	if ( keysym == 65377 ) {
		return 63;
	}
	if ( keysym == 65407 ) {
		return 64;
	}
	if ( keysym == 65421 ) {
		return 65;
	}
	if ( (keysym >= 65429) && (keysym <= 65439)) {
		return keysym - (65429 - 66);
	}
	if ( (keysym >= 65450) && (keysym <= 65451)) {
		return 77;
	}
	if ( keysym == 65453 ) {
		return 78;
	}
	if ( keysym == 65455 ) {
		return 79;
	}
	if ( (keysym >= 65470) && (keysym <= 65481)) {
		return keysym - (65470 - 80);
	}
	if ( (keysym >= 65505) && (keysym <= 65509)) {
		return keysym - (65505 - 92);
	}
	if ( (keysym >= 65513) && (keysym <= 65515)) {
		return keysym - (65513 - 97);
	}
	if ( keysym == 65535 ) {
		return 99;
	}
	if (keysym == 269025125) {
		return 100;
	}
	return 104;
};
