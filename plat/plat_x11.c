/*
 *	@(#)plat_x11.c	1.0
 *
 *  X11 window, GLX context, ALSA output.  Both GL and ALSA are opened with
 *  dlopen() and their entry points taken with dlsym(), so this file needs
 *  no GL header, no ALSA header, and links against nothing but X11 and dl.
 *  A machine without sound still runs the game; it is just quiet.
 */
#define _POSIX_C_SOURCE 199309L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#include <time.h>
#include <stdio.h>
#include "plat.h"

/*  GLX, declared by hand.  GLXContext is an opaque pointer, drawables are
 *  XIDs.  These four signatures have not changed since 1998.
 */
static XVisualInfo *(*p_glXChooseVisual)(Display *, int, int *);
static void *(*p_glXCreateContext)(Display *, XVisualInfo *, void *, int);
static int (*p_glXMakeCurrent)(Display *, unsigned long, void *);
static void (*p_glXSwapBuffers)(Display *, unsigned long);
static void (*p_glXDestroyContext)(Display *, void *);
static void *(*p_glXGetProcAddressARB)(const unsigned char *);

#define GLX_RGBA		4
#define GLX_DOUBLEBUFFER	5
#define GLX_RED_SIZE		8
#define GLX_GREEN_SIZE		9
#define GLX_BLUE_SIZE		10
#define GLX_DEPTH_SIZE		12
#define GLX_STENCIL_SIZE	13

static Display *dpy;
static Window win;
static Atom wm_delete;
static void *gl_lib;
static void *gl_ctx;
static int win_w;
static int win_h;
static int grabbed;
static int last_mx;
static int last_my;
static int acc_dx;
static int acc_dy;
static double time_base;

/* ---------------------------------------------------------------- time */

static double
now_seconds(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

double
plat_Time(void)
{
	return now_seconds() - time_base;
}

void
plat_Sleep(double seconds)
{
	struct timespec ts;

	if (seconds <= 0.0)
		return;
	ts.tv_sec = (long)seconds;
	ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
	nanosleep(&ts, 0);
}

/* ------------------------------------------------------------------ gl */

/*  dlsym() returns void *, which ISO C will not let you assign to a
 *  function pointer; POSIX requires exactly that and every loader in
 *  existence does it.  The complaint is silenced for the two loader
 *  functions and nowhere else in the tree.
 */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

static int
load_gl(void)
{
	gl_lib = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
	if (gl_lib == 0)
		gl_lib = dlopen("libGL.so", RTLD_LAZY | RTLD_GLOBAL);
	if (gl_lib == 0) {
		fprintf(stderr, "plat: no libGL.so.1\n");
		return 0;
	}
	p_glXChooseVisual = dlsym(gl_lib, "glXChooseVisual");
	p_glXCreateContext = dlsym(gl_lib, "glXCreateContext");
	p_glXMakeCurrent = dlsym(gl_lib, "glXMakeCurrent");
	p_glXSwapBuffers = dlsym(gl_lib, "glXSwapBuffers");
	p_glXDestroyContext = dlsym(gl_lib, "glXDestroyContext");
	p_glXGetProcAddressARB = dlsym(gl_lib, "glXGetProcAddressARB");

	if (p_glXChooseVisual == 0 || p_glXCreateContext == 0 ||
	    p_glXMakeCurrent == 0 || p_glXSwapBuffers == 0) {
		fprintf(stderr, "plat: libGL is missing GLX entry points\n");
		return 0;
	}
	return 1;
}

void *
plat_GlProc(const char *name)
{
	void *p;

	p = 0;
	if (p_glXGetProcAddressARB != 0)
		p = p_glXGetProcAddressARB((const unsigned char *)name);
	if (p == 0 && gl_lib != 0)
		p = dlsym(gl_lib, name);
	return p;
}

/* -------------------------------------------------------------- window */

int
plat_Init(void)
{
	time_base = now_seconds();
	dpy = XOpenDisplay(0);
	if (dpy == 0) {
		fprintf(stderr, "plat: cannot open display\n");
		return 0;
	}
	if (!load_gl()) {
		XCloseDisplay(dpy);
		dpy = 0;
		return 0;
	}
	return 1;
}

void
plat_Shutdown(void)
{
	plat_AudioClose();
	if (dpy != 0) {
		XCloseDisplay(dpy);
		dpy = 0;
	}
	if (gl_lib != 0) {
		dlclose(gl_lib);
		gl_lib = 0;
	}
}

int
plat_OpenWindow(const char *title, int width, int height)
{
	int attribs[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		0
	};
	XVisualInfo *vi;
	XSetWindowAttributes swa;
	Colormap cmap;

	if (dpy == 0)
		return 0;

	vi = p_glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
	if (vi == 0) {
		fprintf(stderr, "plat: no usable visual\n");
		return 0;
	}

	cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual,
	    AllocNone);
	swa.colormap = cmap;
	swa.background_pixmap = None;
	swa.border_pixel = 0;
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
	    ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
	    StructureNotifyMask | FocusChangeMask;

	win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0,
	    (unsigned)width, (unsigned)height, 0, vi->depth, InputOutput,
	    vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);
	if (win == 0) {
		XFree(vi);
		return 0;
	}

	XStoreName(dpy, win, title);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wm_delete, 1);
	XMapWindow(dpy, win);

	gl_ctx = p_glXCreateContext(dpy, vi, 0, True);
	XFree(vi);
	if (gl_ctx == 0) {
		fprintf(stderr, "plat: cannot create a GL context\n");
		return 0;
	}
	p_glXMakeCurrent(dpy, win, gl_ctx);

	win_w = width;
	win_h = height;
	return 1;
}

void
plat_CloseWindow(void)
{
	if (dpy == 0)
		return;
	if (gl_ctx != 0) {
		p_glXMakeCurrent(dpy, 0, 0);
		if (p_glXDestroyContext != 0)
			p_glXDestroyContext(dpy, gl_ctx);
		gl_ctx = 0;
	}
	if (win != 0) {
		XDestroyWindow(dpy, win);
		win = 0;
	}
}

void
plat_Swap(void)
{
	if (dpy != 0 && win != 0)
		p_glXSwapBuffers(dpy, win);
}

/* --------------------------------------------------------------- input */

static int
key_from_sym(KeySym sym)
{
	if (sym >= XK_a && sym <= XK_z)
		return (int)('A' + (sym - XK_a));
	if (sym >= XK_A && sym <= XK_Z)
		return (int)sym;
	if (sym >= XK_0 && sym <= XK_9)
		return (int)sym;
	if (sym == XK_space)
		return ' ';
	if (sym >= XK_F1 && sym <= XK_F12)
		return PLAT_KEY_F1 + (int)(sym - XK_F1);

	if (sym == XK_Escape)
		return PLAT_KEY_ESC;
	if (sym == XK_Tab)
		return PLAT_KEY_TAB;
	if (sym == XK_Return || sym == XK_KP_Enter)
		return PLAT_KEY_ENTER;
	if (sym == XK_BackSpace)
		return PLAT_KEY_BACKSPACE;
	if (sym == XK_Shift_L || sym == XK_Shift_R)
		return PLAT_KEY_SHIFT;
	if (sym == XK_Control_L || sym == XK_Control_R)
		return PLAT_KEY_CTRL;
	if (sym == XK_Alt_L || sym == XK_Alt_R)
		return PLAT_KEY_ALT;
	if (sym == XK_Up)
		return PLAT_KEY_UP;
	if (sym == XK_Down)
		return PLAT_KEY_DOWN;
	if (sym == XK_Left)
		return PLAT_KEY_LEFT;
	if (sym == XK_Right)
		return PLAT_KEY_RIGHT;
	return PLAT_KEY_NONE;
}

static void
press(struct plat_input *in, int key, int down)
{
	if (key <= 0 || key >= PLAT_KEYS)
		return;
	if (down) {
		if (!in->hold[key])
			in->hit[key] = 1;
		in->hold[key] = 1;
	} else {
		if (in->hold[key])
			in->rel[key] = 1;
		in->hold[key] = 0;
	}
}

static void
warp_to_centre(void)
{
	last_mx = win_w / 2;
	last_my = win_h / 2;
	XWarpPointer(dpy, None, win, 0, 0, 0, 0, last_mx, last_my);
}

void
plat_GrabMouse(int on)
{
	XColor black;
	Pixmap blank;
	static char nothing[8];
	Cursor cur;

	if (dpy == 0 || win == 0)
		return;
	grabbed = on;
	if (on) {
		black.red = 0;
		black.green = 0;
		black.blue = 0;
		blank = XCreateBitmapFromData(dpy, win, nothing, 8, 8);
		cur = XCreatePixmapCursor(dpy, blank, blank, &black, &black,
		    0, 0);
		XDefineCursor(dpy, win, cur);
		XFreeCursor(dpy, cur);
		XFreePixmap(dpy, blank);
		warp_to_centre();
	} else {
		XUndefineCursor(dpy, win);
	}
}

void
plat_Poll(struct plat_input *in)
{
	XEvent ev;
	KeySym sym;
	int i;
	int key;

	for (i = 0; i < PLAT_KEYS; i++) {
		in->hit[i] = 0;
		in->rel[i] = 0;
	}
	acc_dx = 0;
	acc_dy = 0;
	in->resized = 0;

	if (dpy == 0)
		return;

	while (XPending(dpy) > 0) {
		XNextEvent(dpy, &ev);
		if (ev.type == KeyPress || ev.type == KeyRelease) {
			sym = XLookupKeysym(&ev.xkey, 0);
			key = key_from_sym(sym);
			press(in, key, ev.type == KeyPress);
		} else if (ev.type == ButtonPress ||
		    ev.type == ButtonRelease) {
			key = PLAT_KEY_NONE;
			if (ev.xbutton.button == Button1)
				key = PLAT_MOUSE_L;
			if (ev.xbutton.button == Button2)
				key = PLAT_MOUSE_M;
			if (ev.xbutton.button == Button3)
				key = PLAT_MOUSE_R;
			if (ev.xbutton.button == Button4)
				key = PLAT_WHEEL_UP;
			if (ev.xbutton.button == Button5)
				key = PLAT_WHEEL_DOWN;
			press(in, key, ev.type == ButtonPress);
		} else if (ev.type == MotionNotify) {
			acc_dx += ev.xmotion.x - last_mx;
			acc_dy += ev.xmotion.y - last_my;
			last_mx = ev.xmotion.x;
			last_my = ev.xmotion.y;
			in->mouse_x = ev.xmotion.x;
			in->mouse_y = ev.xmotion.y;
		} else if (ev.type == ConfigureNotify) {
			if (ev.xconfigure.width != win_w ||
			    ev.xconfigure.height != win_h) {
				win_w = ev.xconfigure.width;
				win_h = ev.xconfigure.height;
				in->resized = 1;
			}
		} else if (ev.type == ClientMessage) {
			if ((Atom)ev.xclient.data.l[0] == wm_delete)
				in->quit = 1;
		}
	}

	in->mouse_dx = acc_dx;
	in->mouse_dy = acc_dy;
	in->width = win_w;
	in->height = win_h;

	if (grabbed && (acc_dx != 0 || acc_dy != 0))
		warp_to_centre();
}

/* --------------------------------------------------------------- audio */
/*
 *  ALSA's "simple" entry points, taken with dlsym.  snd_pcm_set_params()
 *  does in one call what the hw_params dance does in twenty, and it has
 *  been stable for as long as it has existed.
 */
static void *alsa_lib;
static void *pcm;
static int (*p_snd_pcm_open)(void **, const char *, int, int);
static int (*p_snd_pcm_set_params)(void *, int, int, unsigned int,
    unsigned int, int, unsigned int);
static long (*p_snd_pcm_avail_update)(void *);
static long (*p_snd_pcm_writei)(void *, const void *, unsigned long);
static int (*p_snd_pcm_recover)(void *, int, int);
static int (*p_snd_pcm_prepare)(void *);
static int (*p_snd_pcm_close)(void *);
static int audio_channels;

#define SND_PCM_STREAM_PLAYBACK		0
#define SND_PCM_FORMAT_S16_LE		2
#define SND_PCM_ACCESS_RW_INTERLEAVED	3

int
plat_AudioOpen(int rate, int channels)
{
	int err;

	alsa_lib = dlopen("libasound.so.2", RTLD_LAZY);
	if (alsa_lib == 0)
		return 0;

	p_snd_pcm_open = dlsym(alsa_lib, "snd_pcm_open");
	p_snd_pcm_set_params = dlsym(alsa_lib, "snd_pcm_set_params");
	p_snd_pcm_avail_update = dlsym(alsa_lib, "snd_pcm_avail_update");
	p_snd_pcm_writei = dlsym(alsa_lib, "snd_pcm_writei");
	p_snd_pcm_recover = dlsym(alsa_lib, "snd_pcm_recover");
	p_snd_pcm_prepare = dlsym(alsa_lib, "snd_pcm_prepare");
	p_snd_pcm_close = dlsym(alsa_lib, "snd_pcm_close");
	if (p_snd_pcm_open == 0 || p_snd_pcm_set_params == 0 ||
	    p_snd_pcm_writei == 0 || p_snd_pcm_avail_update == 0)
		return 0;

	if (p_snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		pcm = 0;
		return 0;
	}
	err = p_snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
	    SND_PCM_ACCESS_RW_INTERLEAVED, (unsigned)channels,
	    (unsigned)rate, 1, 60000);
	if (err < 0) {
		p_snd_pcm_close(pcm);
		pcm = 0;
		return 0;
	}
	audio_channels = channels;
	return 1;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

int
plat_AudioSpace(void)
{
	long avail;

	if (pcm == 0)
		return 0;
	avail = p_snd_pcm_avail_update(pcm);
	if (avail < 0) {
		if (p_snd_pcm_recover != 0)
			p_snd_pcm_recover(pcm, (int)avail, 1);
		else if (p_snd_pcm_prepare != 0)
			p_snd_pcm_prepare(pcm);
		return 0;
	}
	return (int)avail;
}

int
plat_AudioWrite(const short *data, int frames)
{
	long n;

	if (pcm == 0 || frames <= 0)
		return 0;
	n = p_snd_pcm_writei(pcm, data, (unsigned long)frames);
	if (n < 0) {
		if (p_snd_pcm_recover != 0)
			p_snd_pcm_recover(pcm, (int)n, 1);
		return 0;
	}
	return (int)n;
}

void
plat_AudioClose(void)
{
	if (pcm != 0 && p_snd_pcm_close != 0)
		p_snd_pcm_close(pcm);
	pcm = 0;
	if (alsa_lib != 0) {
		dlclose(alsa_lib);
		alsa_lib = 0;
	}
}
