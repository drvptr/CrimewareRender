#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "io.h"

/*------------------------------------------------- 
	# Opaque Type implemetntation #
------------------------------------------------- */

struct io_window_inc_t {
	Display	*x_dpy;
	int	x_scr;
	Window	x_win;
	GC	x_gc;
	Atom	x_wm_delete;
	int	io_w, io_h;
	XImage	*x_img;
	XShmSegmentInfo	x_shm;
	unsigned char	*io_buf;
};

/*------------------------------------------------- 
	# 0.STATIC FUNC (internal usage only) #
------------------------------------------------- */

static void st_io_DisableKeyRepeat(Display *display){
	XKeyboardControl control;
	control.auto_repeat_mode = AutoRepeatModeOff;
	XChangeKeyboardControl(display, KBAutoRepeatMode, &control);
}

static void st_io_EnableKeyRepeat(Display *display){
	XKeyboardControl control;
	control.auto_repeat_mode = AutoRepeatModeOn;
	XChangeKeyboardControl(display, KBAutoRepeatMode, &control);
}

static int st_io_ConvertKeysyms(int keysym){
	if (keysym == 32)				return 0;
	if (keysym == 39)				return 1;
	if ( (keysym >= 44) && (keysym <= 59))		return keysym - (44 - 2);
	if ( keysym == 61 )				return 18;
	if ( (keysym >= 91) && (keysym <= 93))  	return keysym - (91 - 19);
	if ( (keysym >= 96) && (keysym <= 122))		return keysym - (96 - 22);
	if ( (keysym >= 65288) && (keysym <= 65289))	return keysym - (65288 - 49);
	if ( keysym == 65293 )  			return 51;
	if ( keysym == 65299 )  			return 52;
	if ( keysym == 65307 )  			return 54;
	if ( (keysym >= 65360) && (keysym <= 65367))  	return keysym - (65360 - 55);
	if ( keysym == 65377 )  			return 63;
	if ( keysym == 65407 )  			return 64;
	if ( keysym == 65421 )  			return 65;
	if ( (keysym >= 65429) && (keysym <= 65439))  	return keysym - (65429 - 66);
	if ( (keysym >= 65450) && (keysym <= 65451))  	return 77;
	if ( keysym == 65453 )  			return 78;
	if ( keysym == 65455 )  			return 79;
	if ( (keysym >= 65470) && (keysym <= 65481))  	return keysym - (65470 - 80);
	if ( (keysym >= 65505) && (keysym <= 65509))  	return keysym - (65505 - 92);
	if ( (keysym >= 65513) && (keysym <= 65515))  	return keysym - (65513 - 97);
	if ( keysym == 65535 )  			return 99;
	if (keysym == 269025125)  			return 100;
	/* ERROR KEY */					return 104;
}

static Display *st_io_OpenDisplay(void) {
	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "io_xlib.c: Can't open X display\n");
		exit(1);
	}
	return dpy;
}

static Window st_io_CreateWindow(Display *dpy, int screen, int width, int height, Atom *wm_del) {
	Window win = XCreateSimpleWindow(
		dpy, RootWindow(dpy, screen), 0, 0, width, height, 1,
		BlackPixel(dpy, screen), WhitePixel(dpy, screen) );
	XSelectInput(dpy, win,
		ExposureMask | KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask |
		StructureNotifyMask | PointerMotionMask | FocusChangeMask);
	XStoreName(dpy, win, TITLE);
	*wm_del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, wm_del, 1);
	XMapWindow(dpy, win);
	return win;
}

static GC st_io_CreateGC(Display *dpy, Window win) {
	return XCreateGC(dpy, win, 0, NULL);
}

static XImage *st_io_CreateFramebuffer(Display *dpy, int screen, int width, int height, XShmSegmentInfo *shm_info) {
	if (!XShmQueryExtension(dpy)) {
		fprintf(stderr, "io_xlib.c: XShm not supported\n");
		exit(1);
	}
	XImage *ximg = XShmCreateImage(dpy,
		DefaultVisual(dpy, screen),
		DefaultDepth(dpy, screen),
		ZPixmap, NULL, shm_info, width, height);
	shm_info->shmid = shmget(IPC_PRIVATE, ximg->bytes_per_line * ximg->height,
		IPC_CREAT | 0777);
	shm_info->shmaddr = ximg->data = shmat(shm_info->shmid, 0, 0);
	shm_info->readOnly = False;
	if (shm_info->shmaddr == (void *)-1) {
		perror("io_xlib.c: shmat");
		exit(1);
	}
	XShmAttach(dpy, shm_info);
	return ximg;
}

typedef void (*st_io_EventHandler_t)(XEvent *e, io_keys_t *c, io_window_t *w);

static void st_HandleKey(XEvent *e, io_keys_t *c, io_window_t *w) {
	int key = st_io_ConvertKeysyms(XLookupKeysym(&e->xkey, 0));
	if (key < 0 || key >= KEYCODE) return;
	if (e->type == KeyPress) {
		if (c->status[key] < IO_TOGGLED)
			c->status[key] += IO_TOGGLED; 
		else
			c->status[key] -= IO_TOGGLED; 
		c->status[key] += IO_HOLD;
	}
	else if (e->type == KeyRelease) {
		c->status[key] -= IO_HOLD;
	}
}

static void st_HandleMouse(XEvent *e, io_keys_t *c, io_window_t *w) {
	int key = e->xbutton.button - 1 + MOUSE_L;
	if (key < 0 || key >= KEYCODE) return;
	if (e->type == ButtonPress) {
		if (c->status[key] < IO_TOGGLED)
			c->status[key] += IO_TOGGLED; 
		else
			c->status[key] -= IO_TOGGLED; 
		c->status[key] += IO_HOLD;
	}
	else if (e->type == ButtonRelease) {
		c->status[key] -= IO_HOLD;
	}
}

static void st_HandleMotion(XEvent *e, io_keys_t *c, io_window_t *w) {
	c->x = e->xmotion.x;
	c->y = e->xmotion.y;
}

static void st_HandleConfigure(XEvent *e, io_keys_t *c, io_window_t *w) {
	XConfigureEvent *ce = &e->xconfigure;
	int new_w = ce->width, new_h = ce->height;
	while (XPending(w->x_dpy)) {
		XEvent tmp;
		XPeekEvent(w->x_dpy, &tmp);
		if (tmp.type != ConfigureNotify) break;
		XNextEvent(w->x_dpy, &tmp);
		new_w = tmp.xconfigure.width;
		new_h = tmp.xconfigure.height;
	}
	if (new_w == w->io_w && new_h == w->io_h) return;
	w->io_w = new_w;
	w->io_h = new_h;
	XShmDetach(w->x_dpy, &w->x_shm);
	XDestroyImage(w->x_img);
	shmdt(w->x_shm.shmaddr);
	shmctl(w->x_shm.shmid, IPC_RMID, NULL);
	w->x_img = st_io_CreateFramebuffer(w->x_dpy, w->x_scr, w->io_w, w->io_h, &w->x_shm);
	w->io_buf = (unsigned char *)w->x_img->data;
}

static void st_HandleClose(XEvent *e, io_keys_t *c, io_window_t *w) {
	if ((Atom)e->xclient.data.l[0] == w->x_wm_delete) {
		io_CloseWindow(w);
		exit(0);
	}
}

static void st_HandleFocus(XEvent *e, io_keys_t *c, io_window_t *w) {
	if (e->type == FocusIn) {
		st_io_DisableKeyRepeat(w->x_dpy);
		#ifdef DEBUG
		printf("(dbg) io_xlib.c: FOCUSED\n");
		#endif
	}
	if (e->type == FocusOut) {
		st_io_EnableKeyRepeat(w->x_dpy);
		#ifdef DEBUG
		printf("(dbg) io_xlib.c: UNFOCUSED\n");
		#endif
	}
}


static st_io_EventHandler_t st_EventHandlers[LASTEvent] = {
	[KeyPress]        = st_HandleKey,
	[KeyRelease]      = st_HandleKey,
	[ButtonPress]     = st_HandleMouse,
	[ButtonRelease]   = st_HandleMouse,
	[MotionNotify]    = st_HandleMotion,
	[ConfigureNotify] = st_HandleConfigure,
	[ClientMessage]   = st_HandleClose,
	[FocusIn]         = st_HandleFocus,
	[FocusOut]        = st_HandleFocus
};

/*------------------------------------------------- 
	# 1.WINDOW INMPLEMENTATION #
------------------------------------------------- */

io_window_t *io_InitWindow(void) {
	io_window_t *w = calloc(1, sizeof(io_window_t));
	if (!w) return NULL;
	w->io_w = DEFAULT_WINDOW_WIDTH;
	w->io_h = DEFAULT_WINDOW_HEIGHT;
	w->x_dpy = st_io_OpenDisplay();
	w->x_scr = DefaultScreen(w->x_dpy);
	w->x_win = st_io_CreateWindow(w->x_dpy, w->x_scr, w->io_w, w->io_h, &w->x_wm_delete);
	w->x_gc = st_io_CreateGC(w->x_dpy, w->x_win);
	w->x_img = st_io_CreateFramebuffer(w->x_dpy, w->x_scr, w->io_w, w->io_h, &w->x_shm);
	w->io_buf = (unsigned char *)w->x_img->data;
	return w;
}

void io_CloseWindow(io_window_t *w) {
	if (!w) return;
	st_io_EnableKeyRepeat(w->x_dpy);
	XShmDetach(w->x_dpy, &w->x_shm);
	XDestroyImage(w->x_img);
	shmdt(w->x_shm.shmaddr);
	shmctl(w->x_shm.shmid, IPC_RMID, NULL);
	XFreeGC(w->x_dpy, w->x_gc);
	XDestroyWindow(w->x_dpy, w->x_win);
	XCloseDisplay(w->x_dpy);
	free(w);
}

void io_SetPixel(io_window_t *w, int x, int y, unsigned int color) {
	if ((unsigned)x >= (unsigned)w->io_w || (unsigned)y >= (unsigned)w->io_h)
		return;
	uint32_t pixel = 0;
	pixel |= ((color >> 16) & 0xFF) << __builtin_ctz(w->x_img->red_mask);
	pixel |= ((color >> 8) & 0xFF) << __builtin_ctz(w->x_img->green_mask);
	pixel |= (color & 0xFF) << __builtin_ctz(w->x_img->blue_mask);
	*(uint32_t *)(w->x_img->data + y * w->x_img->bytes_per_line + x * 4) = pixel;
}

unsigned int io_GetPixel(io_window_t *w, int x, int y) {
	if ((unsigned)x >= (unsigned)w->io_w || (unsigned)y >= (unsigned)w->io_h)
		return 0;
	uint32_t pixel = *(uint32_t *)(w->x_img->data + y * w->x_img->bytes_per_line + x * 4);
	unsigned int r = ((pixel & w->x_img->red_mask)   >> __builtin_ctz(w->x_img->red_mask));
	unsigned int g = ((pixel & w->x_img->green_mask) >> __builtin_ctz(w->x_img->green_mask));
	unsigned int b = ((pixel & w->x_img->blue_mask)  >> __builtin_ctz(w->x_img->blue_mask));
	return (r << 16) | (g << 8) | b;
}

void io_UpdateFrame(io_window_t *w) {
	XShmPutImage(w->x_dpy, w->x_win, w->x_gc, w->x_img, 0, 0, 0, 0, w->io_w, w->io_h, False);
	XFlush(w->x_dpy);
}

int io_GetWidth(io_window_t *w){
	return w->io_w;
}

int io_GetHeight(io_window_t *w){
	return w->io_h;
}

/*------------------------------------------------- 
	# 2.KEYBOARD AND MOUSE INMPLEMENTATION #
------------------------------------------------- */

io_keys_t *io_InitKeys(void){
	io_keys_t *c = malloc(sizeof(io_keys_t));
	for(int i = 0; i <= KEYCODE; i++){
		c->status[i] = IO_NONE;
	};
	c->x = 0; c->y = 0;
	return c;
}

void io_PollKeys(io_window_t *w, io_keys_t *c, int mode) {
	XEvent e;
	while (mode == BLOCK_POLL || XPending(w->x_dpy)) {
		XNextEvent(w->x_dpy, &e);
		if (e.type < LASTEvent && st_EventHandlers[e.type])
			st_EventHandlers[e.type](&e, c, w);
		if (mode == NONBLOCK_POLL)
			break;
	}
}

void io_FreeKeys(io_keys_t *c){
	if (c == NULL) return;
	free(c);
}
