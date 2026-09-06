/*
 *	@(#)cut.h	1.0
 *
 *  Cutscenes.  A cutscene is a list of keys on a clock: camera positions to
 *  fly between, fades, subtitles, and marks that fire once when the clock
 *  passes them.  The engine moves the clock and interpolates; what a mark
 *  MEANS is the game's business, which is the same rule as everywhere else
 *  here - no scripting language, no interpreter, no bytecode.
 *
 *	cut_Clear(&scene);
 *	cut_Camera(&scene, 0.0f, eye0, look0);
 *	cut_Camera(&scene, 4.0f, eye1, look1);
 *	cut_Subtitle(&scene, 0.5f, 3.0f, "They took the bridge at dawn.");
 *	cut_Mark(&scene, 4.0f, EVENT_DOOR_OPENS);
 *	cut_Start(&scene);
 *	...
 *	while ((event = cut_Poll(&scene)) >= 0)
 *		handle(event);
 */
#ifndef CUT_H_SENTRY
#define CUT_H_SENTRY

#include "../core/m3.h"

#define CUT_MAX_KEYS 64
#define CUT_MAX_MARKS 64
#define CUT_TEXT_LEN 96

struct cut_key {
	float at;
	struct vec3 eye;
	struct vec3 look;
};

struct cut_mark {
	float at;
	int event;
	int fired;
};

struct cut_subtitle {
	float from;
	float to;
	char text[CUT_TEXT_LEN];
};

struct cutscene {
	struct cut_key key[CUT_MAX_KEYS];
	int nkeys;
	struct cut_mark mark[CUT_MAX_MARKS];
	int nmarks;
	struct cut_subtitle subtitle[CUT_MAX_MARKS];
	int nsubtitles;
	float clock;
	float length;
	float fade_in;
	float fade_out;
	int playing;
};

void cut_Clear(struct cutscene *c);
int cut_Camera(struct cutscene *c, float at, struct vec3 eye,
    struct vec3 look);
int cut_Mark(struct cutscene *c, float at, int event);
int cut_Subtitle(struct cutscene *c, float from, float to, const char *text);
void cut_Fades(struct cutscene *c, float in_seconds, float out_seconds);

void cut_Start(struct cutscene *c);
void cut_Stop(struct cutscene *c);
int cut_Playing(const struct cutscene *c);
void cut_Advance(struct cutscene *c, float dt);

int cut_Poll(struct cutscene *c);
/* RETURN VALUE: the next event whose time has passed, or -1.  Call it in a
 *		 loop until it returns -1.
 */

void cut_View(const struct cutscene *c, float view[16]);
float cut_Fade(const struct cutscene *c);	/* 0 clear, 1 black */
const char *cut_Text(const struct cutscene *c);	/* 0 when silent */

#endif /* CUT_H_SENTRY */
