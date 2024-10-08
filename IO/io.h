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
 *	@(#)io.h	1.0 (Potr Dervyshev) 08/10/2024
 */
 
#ifndef INPUTOUTPUT_H_SENTRY
#define INPUTOUTPUT_H_SENTRY

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600
#define TITLE "main"

typedef struct window_t window;
	 
typedef enum {
	none,
	press,
	release
} event;

#define MAX_KEYS 104

typedef struct controls_t{
	event type;
	int hold[MAX_KEYS];
	int toggle[MAX_KEYS];
	int x;
	int y;
} controls;

/*WINDOW FUNCTIONS*/
window *io_InitWindow();		//Constructor-func
int io_GetWidth(window *w);	//Acsessors-funcs
int io_GetHeight(window *w);
void io_SetPixel(window *w, int x, int y, int color);
int io_GetPixel(window *w, int x, int y);
void io_UpdateFrame(window *w);
void io_CloseWindow(window *w);	//Destructor-func

/*CONTROL FUNCTIONS*/
controls *io_InitControl();	//Constructor-func
void io_PollControls(window *w, controls *c, int mode);
#define io_FreeControl(control) (free(control))
#define NONBLOCK_POLL 0
#define BLOCK_POLL 1
#define TOGGLE(control, key) ((control)->toggle[(key)])
#define HOLD(control, key) ((control)->hold[(key)])
#define PRESS(control, key) ((((control)->hold[(key)]))&&\
			    (((control)->type==press)))
#define MOUSE_X(control) ((control)->x)
#define MOUSE_Y(control) ((control)->y)

/*fmemopen() Unix func for Windows implementation*/
#ifdef _WIN32
FILE *fmemopen(unsigned char arr[], unsigned int len, const char *mode);
#endif

/*KEY DEFINITIONS*/
#define MOUSE_L 101
#define MOUSE_R 103
#define MOUSE_M 102
#define KEY_ERROR 104
#define KEY_CTRL_L 94
#define KEY_SUPER_L 99
#define KEY_ALT_L 97
#define KEY_SPACE 0
#define KEY_AALT_R 98
#define KEY_MENU 100
#define KEY_CTRL_R 95
#define KEY_LEFT 56
#define KEY_DOWN 59
#define KEY_RIGHT 58
#define KP_INSERT 75
#define KP_DELETE 76
#define KP_ENTER 65
#define KEY_SHIFT_L 92
#define KEY_Z 48
#define KEY_X 46
#define KEY_C 25
#define KEY_V 44
#define KEY_B 24
#define KEY_N 36
#define KEY_M 35
#define KEY_COMMA 2
#define KEY_PERIOD 4
#define KEY_SLASH 5
#define KEY_SHIFT_R 93
#define KEY_UP 57
#define KP_END 73
#define KP_DOWN 70
#define KP_NEXT 72
#define KEY_CAPS_LOCK 96
#define KEY_A 23
#define KEY_S 41
#define KEY_D 26
#define KEY_F 28
#define KEY_G 29
#define KEY_H 30
#define KEY_J 32
#define KEY_K 33
#define KEY_L 34
#define KEY_SEMICOLON 17
#define KEY_APOSTROPHE 1
#define KEY_RETURN 51
#define KP_LEFT 67
#define KP_BEGIN 74
#define KP_RIGHT 69
#define KP_ADD 77
#define KEY_TAB 50
#define KEY_Q 39
#define KEY_W 45
#define KEY_E 27
#define KEY_R 40
#define KEY_T 42
#define KEY_Y 47
#define KEY_U 43
#define KEY_I 31
#define KEY_O 37
#define KEY_P 38
#define KEY_BRACKETLEFT 19
#define KEY_BRACKETRIGHT 21
#define KEY_BACKSLASH 20
#define KP_HOME 66
#define KP_UP 68
#define KP_PRIOR 71
#define KEY_GRAVE 22
#define KEY_1 7
#define KEY_2 8
#define KEY_3 9
#define KEY_4 10
#define KEY_5 11
#define KEY_6 12
#define KEY_7 13
#define KEY_8 14
#define KEY_9 15
#define KEY_0 6
#define KEY_MINUS 3
#define KEY_EQUAL 18
#define KEY_BACKSPACE 49
#define KEY_NUM_LOCK 64
#define KP_DIVIDE 79
#define KP_MULTIPLY 77
#define KP_SUBTRACT 78
#define KEY_F1 80
#define KEY_F2 81
#define KEY_F3 82
#define KEY_F4 83
#define KEY_F5 84
#define KEY_F6 85
#define KEY_F7 86
#define KEY_F8 87
#define KEY_F9 88
#define KEY_F10 89
#define KEY_F11 90
#define KEY_F12 91
#define KEY_PAUSE 52
#define KEY_PRINT 63
#define KEY_DELETE 99
#define KEY_HOME 55
#define KEY_PRIOR 60
#define KEY_NEXT 61
#define KEY_END 62
#define KEY_ESC 54

#endif
