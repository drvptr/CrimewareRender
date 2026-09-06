#include "cut.h"

void
cut_Clear(struct cutscene *c)
{
	c->nkeys = 0;
	c->nmarks = 0;
	c->nsubtitles = 0;
	c->clock = 0.0f;
	c->length = 0.0f;
	c->fade_in = 0.0f;
	c->fade_out = 0.0f;
	c->playing = 0;
}

static void
note_length(struct cutscene *c, float at)
{
	if (at > c->length)
		c->length = at;
}

int
cut_Camera(struct cutscene *c, float at, struct vec3 eye, struct vec3 look)
{
	if (c->nkeys >= CUT_MAX_KEYS)
		return -1;
	c->key[c->nkeys].at = at;
	c->key[c->nkeys].eye = eye;
	c->key[c->nkeys].look = look;
	c->nkeys++;
	note_length(c, at);
	return 0;
}

int
cut_Mark(struct cutscene *c, float at, int event)
{
	if (c->nmarks >= CUT_MAX_MARKS)
		return -1;
	c->mark[c->nmarks].at = at;
	c->mark[c->nmarks].event = event;
	c->mark[c->nmarks].fired = 0;
	c->nmarks++;
	note_length(c, at);
	return 0;
}

int
cut_Subtitle(struct cutscene *c, float from, float to, const char *text)
{
	int i;

	if (c->nsubtitles >= CUT_MAX_MARKS || text == 0)
		return -1;
	c->subtitle[c->nsubtitles].from = from;
	c->subtitle[c->nsubtitles].to = to;
	for (i = 0; i < CUT_TEXT_LEN - 1 && text[i] != 0; i++)
		c->subtitle[c->nsubtitles].text[i] = text[i];
	c->subtitle[c->nsubtitles].text[i] = 0;
	c->nsubtitles++;
	note_length(c, to);
	return 0;
}

void
cut_Fades(struct cutscene *c, float in_seconds, float out_seconds)
{
	c->fade_in = in_seconds;
	c->fade_out = out_seconds;
}

void
cut_Start(struct cutscene *c)
{
	int i;

	c->clock = 0.0f;
	c->playing = 1;
	for (i = 0; i < c->nmarks; i++)
		c->mark[i].fired = 0;
}

void
cut_Stop(struct cutscene *c)
{
	c->playing = 0;
}

int
cut_Playing(const struct cutscene *c)
{
	return c->playing;
}

void
cut_Advance(struct cutscene *c, float dt)
{
	if (!c->playing)
		return;
	c->clock += dt;
	if (c->clock >= c->length)
		c->playing = 0;
}

int
cut_Poll(struct cutscene *c)
{
	int i;

	for (i = 0; i < c->nmarks; i++) {
		if (!c->mark[i].fired && c->clock >= c->mark[i].at) {
			c->mark[i].fired = 1;
			return c->mark[i].event;
		}
	}
	return -1;
}

void
cut_View(const struct cutscene *c, float view[16])
{
	int i;
	int a;
	int b;
	float t;
	struct vec3 eye;
	struct vec3 look;

	if (c->nkeys == 0) {
		m4_identity(view);
		return;
	}

	a = 0;
	for (i = 0; i < c->nkeys; i++) {
		if (c->key[i].at <= c->clock)
			a = i;
	}
	b = a + 1 < c->nkeys ? a + 1 : a;

	t = 0.0f;
	if (b != a && c->key[b].at > c->key[a].at)
		t = (c->clock - c->key[a].at) /
		    (c->key[b].at - c->key[a].at);
	t = m3_clampf(t, 0.0f, 1.0f);

	/*  Smoothstep, so the camera does not start and stop with a jerk.
	 *  A spline through the keys would be nicer; this is two lines.
	 */
	t = t * t * (3.0f - 2.0f * t);

	eye = v3_add(c->key[a].eye,
	    v3_scale(v3_sub(c->key[b].eye, c->key[a].eye), t));
	look = v3_add(c->key[a].look,
	    v3_scale(v3_sub(c->key[b].look, c->key[a].look), t));
	m4_look_at(view, eye, look, v3(0.0f, 1.0f, 0.0f));
}

float
cut_Fade(const struct cutscene *c)
{
	float left;

	if (c->fade_in > 0.0f && c->clock < c->fade_in)
		return 1.0f - c->clock / c->fade_in;
	if (c->fade_out > 0.0f) {
		left = c->length - c->clock;
		if (left < c->fade_out)
			return 1.0f - left / c->fade_out;
	}
	return 0.0f;
}

const char *
cut_Text(const struct cutscene *c)
{
	int i;

	for (i = 0; i < c->nsubtitles; i++) {
		if (c->clock >= c->subtitle[i].from &&
		    c->clock <= c->subtitle[i].to)
			return c->subtitle[i].text;
	}
	return 0;
}
