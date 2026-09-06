/*
 *	@(#)plat.h	1.0
 *
 *  Everything the engine needs from the operating system, and nothing else.
 *  One window, one keyboard, one audio device - a game does not need more,
 *  and pretending otherwise is where portability layers go to die.
 *
 *  Swap the .c file and the engine follows:
 *
 *	plat_x11.c	X11 + GLX	(Linux, FreeBSD)
 *	plat_win32.c	Win32 + WGL	(not written yet)
 *	plat_null.c	nothing at all	(tests, headless, dedicated server)
 *
 *  The engine core includes this header and never anything below it.  Grep
 *  the tree for <X11 or <windows.h: it appears in plat/ and nowhere else.
 */
#ifndef PLAT_H_SENTRY
#define PLAT_H_SENTRY

#define PLAT_KEYS 256

/*  Printable keys ARE their uppercase ASCII code: 'W', 'A', '1', ' '.
 *  You never look up a table for the common case.
 */
enum {
	PLAT_KEY_NONE = 0,
	PLAT_KEY_ESC = 1,
	PLAT_KEY_TAB = 2,
	PLAT_KEY_ENTER = 3,
	PLAT_KEY_BACKSPACE = 4,
	PLAT_KEY_SHIFT = 5,
	PLAT_KEY_CTRL = 6,
	PLAT_KEY_ALT = 7,
	PLAT_KEY_UP = 8,
	PLAT_KEY_DOWN = 9,
	PLAT_KEY_LEFT = 10,
	PLAT_KEY_RIGHT = 11,
	PLAT_KEY_F1 = 12,	/* F1..F12 are 12..23 */
	PLAT_KEY_F12 = 23,
	PLAT_MOUSE_L = 24,
	PLAT_MOUSE_R = 25,
	PLAT_MOUSE_M = 26,
	PLAT_WHEEL_UP = 27,
	PLAT_WHEEL_DOWN = 28
};

struct plat_input {
	unsigned char hold[PLAT_KEYS];	/* down right now		*/
	unsigned char hit[PLAT_KEYS];	/* went down during this poll	*/
	unsigned char rel[PLAT_KEYS];	/* came up during this poll	*/
	int mouse_x;
	int mouse_y;
	int mouse_dx;			/* movement since the last poll	*/
	int mouse_dy;
	int width;			/* window size, updated on resize */
	int height;
	int resized;
	int quit;			/* window closed or ALT+F4	*/
};

int plat_Init(void);
void plat_Shutdown(void);

int plat_OpenWindow(const char *title, int width, int height);
void plat_CloseWindow(void);
void plat_Poll(struct plat_input *in);
void plat_Swap(void);
void plat_GrabMouse(int on);	/* pointer hidden and pinned: mouse look */

double plat_Time(void);		/* monotonic seconds since plat_Init	*/
void plat_Sleep(double seconds);

void *plat_GlProc(const char *name);
/* NOTE:  the graphics backend asks for its own entry points here, which is
 *	  why this file needs no GL header and no -lGL at link time.
 */

/*  AUDIO.  A pull model with a callback needs a thread and a lock; this is
 *  a push model, and the mixer runs in the main loop like everything else.
 *  Ask how much room the device has, mix exactly that much, write it.
 */
int plat_AudioOpen(int rate, int channels);
int plat_AudioSpace(void);	/* frames writable without blocking	*/
int plat_AudioWrite(const short *pcm, int frames);
void plat_AudioClose(void);

#endif /* PLAT_H_SENTRY */
