#include <ncurses.h>
#include <stdlib.h>
#include "io.h"

static char ColorToSymbol(int rgba) {
	const int gradient_size = 24;
	const char gradient[gradient_size] = " .,:;!+=*?<>oamO8GE#$%&@";
	unsigned char r = (rgba >> 24) & 0xFF;
	unsigned char g = (rgba >> 16) & 0xFF;
	unsigned char b = (rgba >> 8) & 0xFF;
	float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
	int index = (int)((luminance / 255.0f) * (gradient_size - 1) + 0.5f);
	if (index < 0)
		index = 0;
	if (index >= gradient_size)
		index = gradient_size - 1;
	return gradient[gradient_size - 1 - index];
}

struct window_t {
	int width;
	int height;
};

window *io_InitWindow() {
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	nodelay(stdscr, TRUE);
	window *res = malloc(sizeof(window));
	getmaxyx(stdscr, res->height, res->width);
	return res;
}

int io_GetWidth(window *w){
	return w->width;
};

int io_GetHeight(window *w){
	return w->height;
};

void io_SetPixel(window *w, int x, int y, int color) {
	mvaddch(y, x, ColorToSymbol(color));
}

int io_GetPixel(window *w, int x, int y) {
	return 0;
}

void io_UpdateFrame(window *w) {
	refresh();
}

void io_CloseWindow(window *w) {
	endwin();
	free(w);
}

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

void io_PollControls(window *w, controls *c, int mode) {
	c->type = none;
	int ch = getch();
	if (ch != ERR || mode) {
		if (ch != ERR) {
			c->type = press;
			HOLD(c, ch) = 1;
			TOGGLE(c, ch) = !TOGGLE(c, ch);
			//if (ch == KEY_MOUSE) { i have no idea. help
			//}
		}
	}
}
