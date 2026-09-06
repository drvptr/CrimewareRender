/*
 *	@(#)gui.h	1.0
 *
 *  An immediate mode GUI: there are no widget objects, no tree, no layout
 *  engine and no callbacks.  You call gui_Button() in the same place you
 *  handle it, every frame, and it returns 1 on the frame it was clicked.
 *
 *	if (gui_Button(20, 20, 160, 32, "NEW GAME"))
 *		start_game();
 *
 *  A menu is therefore an if statement over a state variable, which is
 *  what a menu actually is.  The engine keeps two integers of state - what
 *  the mouse is over and what it pressed on - and nothing else.
 *
 *  Text needs a font atlas: a texture holding 96 glyphs, ASCII 32 to 127,
 *  in a 16 by 6 grid.  Any picture will do, the engine only slices it.
 */
#ifndef GUI_H_SENTRY
#define GUI_H_SENTRY

#include "../plat/plat.h"

void gui_Begin(const struct plat_input *in);
void gui_End(void);

void gui_SetFont(unsigned int texture, int cell_w, int cell_h);
void gui_SetColor(float r, float g, float b, float a);

void gui_Rect(float x, float y, float w, float h);
void gui_Image(float x, float y, float w, float h, unsigned int texture);
void gui_Text(float x, float y, float scale, const char *text);
float gui_TextWidth(const char *text, float scale);

int gui_Button(float x, float y, float w, float h, const char *label);
int gui_Slider(float x, float y, float w, float h, float *value);
int gui_MouseIn(float x, float y, float w, float h);

#endif /* GUI_H_SENTRY */
