/*
 * BSD-0-Clause
 *
 * Copyright (c) 2025
 *	Potr Dervyshev.  All rights reserved.
 *	@(#)io.h	1.0 (Potr Dervyshev) 06/07/2025
 */

#ifndef MYGAME_IO_H_SENTRY
#define MYGAME_IO_H_SENTRY

/*------------------------------------------------- 
		# 1.WINDOW AND DISPLAY #
------------------------------------------------- */
#define TITLE "main"

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

//MAIN SUBJECT:
typedef struct io_window_inc_t io_window_t;
//FUNCS
io_window_t *io_InitWindow();
int io_GetWidth(io_window_t *w);
int io_GetHeight(io_window_t *w);
//int io_Resize(io_window_t *w, int dx, int dy);
void io_SetPixel(io_window_t *w, int x, int y, unsigned int color);
unsigned int io_GetPixel(io_window_t *w, int x, int y);
void io_UpdateFrame(io_window_t *w);
void io_CloseWindow(io_window_t *w);

/*------------------------------------------------- 
		# 2.KEYBOARD AND MOUSE #
------------------------------------------------- */
#define KEYCODE 104	// total keys count

#define NONBLOCK_POLL 0
#define BLOCK_POLL 1

enum io_key_status {IO_NONE, IO_HOLD, IO_TOGGLED, IO_HOLD_AND_TOGGLET};

//MAIN SUBJECT:
typedef struct {
	enum io_key_status status[KEYCODE];
	int x,y;
} io_keys_t;

//keynums
typedef enum io_keycode_tmp { 
KEY_SPACE,	KEY_APOSTROPHE,	KEY_COMMA,	KEY_MINUS,	KEY_PERIOD,	
KEY_SLASH,	KEY_0,	KEY_1,	KEY_2,	KEY_3,	KEY_4,	KEY_5,	KEY_6,	KEY_7,	KEY_8,	KEY_9,	KEY_NONE1,
KEY_SEMICOLON,	KEY_EQUAL,	KEY_BRACKETLEFT,	KEY_BACKSLASH,	KEY_BRACKETRIGHT,	KEY_GRAVE,	
KEY_A,	KEY_B,	KEY_C,	KEY_D,	KEY_E,	KEY_F,	KEY_G,	KEY_H,	KEY_I,	KEY_J,	KEY_K,	KEY_L,	KEY_M,	
KEY_N,	KEY_O,	KEY_P,	KEY_Q,	KEY_R,	KEY_S,	KEY_T,	KEY_U,	KEY_V,	KEY_W,	KEY_X,	KEY_Y,	KEY_Z,	
KEY_BACKSPACE,	KEY_TAB,	KEY_RETURN,	KEY_PAUSE, KEY_RES,	KEY_ESC,	KEY_HOME,	KEY_LEFT,	KEY_UP,	
KEY_RIGHT,	KEY_DOWN,	KEY_PRIOR,	KEY_NEXT,	KEY_END,	KEY_PRINT,	KEY_NUM_LOCK,	KP_ENTER,	
KP_HOME,	KP_LEFT,	KP_UP,	KP_RIGHT,	KP_DOWN,	KP_PRIOR,	KP_NEXT,	KP_END,	KP_BEGIN,	
KP_INSERT,	KP_DELETE,	KP_ADD,	KP_SUBTRACT,	KP_DIVIDE,	KEY_F1,	KEY_F2,	
KEY_F3,	KEY_F4,	KEY_F5,	KEY_F6,	KEY_F7,	KEY_F8,	KEY_F9,	KEY_F10,	KEY_F11,	KEY_F12,	
KEY_SHIFT_L,	KEY_SHIFT_R,	KEY_CTRL_L,	KEY_CTRL_R,	KEY_CAPS_LOCK,	KEY_ALT_L,	KEY_AALT_R,	
KEY_DELETE,	KEY_SUPER_L,	KEY_MENU,	MOUSE_L,	MOUSE_M,	MOUSE_R,	KEY_ERROR 
} io_keycode;

//FUNCS!
static inline enum io_key_status io_GetKeyStatus(io_keys_t *c, int key){
	return c->status[key];
}

io_keys_t *io_InitKeys(void);
void io_PollKeys(io_window_t *w, io_keys_t *c, int mode);
void io_FreeKeys(io_keys_t *c);

#endif	//sentry

