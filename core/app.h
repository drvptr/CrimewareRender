/*
 *	@(#)app.h	1.0
 *
 *  The loop.  Fixed simulation step, free rendering rate, interpolation in
 *  between:
 *
 *	while (running) {
 *		poll input
 *		while (behind) { step(1/60); behind -= 1/60; }
 *		draw(behind / (1/60))
 *		push audio
 *		sleep if there is time left
 *	}
 *
 *  ON epoll.  You asked whether the loop should be built on it.  It should
 *  not, and the reason is worth writing down: epoll answers "which of my
 *  file descriptors is ready", and it blocks until one is.  A game loop
 *  must run when NOTHING is ready - the world moves while the player sits
 *  still.  X11 gives you a socket you could hand to epoll, but you would
 *  still need a timer to wake you every 16 ms, and then epoll is doing
 *  nothing that XPending() in a non-blocking drain does not already do,
 *  while costing you Linux.
 *
 *  epoll earns its place the moment you add a network: then there really
 *  are descriptors that become ready on their own schedule, and the right
 *  shape is epoll_wait() with a timeout equal to the time left in the
 *  frame.  That is a change to one function, and it is deliberately in
 *  plat/, not here.
 */
#ifndef APP_H_SENTRY
#define APP_H_SENTRY

#include "../plat/plat.h"

struct app_hooks {
	const char *title;
	int width;
	int height;
	float step_hz;		/* 0 means 60			*/
	int max_fps;		/* 0 means do not limit		*/
	int audio_rate;		/* 0 means no sound		*/

	int (*init)(void *user);
	void (*step)(void *user, float dt);
	void (*draw)(void *user, float alpha);
	void (*quit)(void *user);
	void *user;
};

int app_Run(struct app_hooks *hooks);
void app_Quit(void);

const struct plat_input *app_Input(void);
int app_Width(void);
int app_Height(void);
float app_Fps(void);

#endif /* APP_H_SENTRY */
