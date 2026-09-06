#include "gui.h"
#include "../gfx/gfx.h"

static const struct plat_input *input;
static float colour[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static unsigned int font_tex;
static int font_w = 8;
static int font_h = 8;
static int active;	/* the mouse went down on this widget	*/
static int widget;	/* counter, so widgets get an identity	*/
static int mouse_down;
static int mouse_was_down;

void
gui_Begin(const struct plat_input *in)
{
	input = in;
	widget = 0;
	mouse_was_down = mouse_down;
	mouse_down = in != 0 ? in->hold[PLAT_MOUSE_L] : 0;
	gfx_Begin2D();
}

void
gui_End(void)
{
	if (!mouse_down)
		active = 0;
	gfx_End2D();
}

void
gui_SetFont(unsigned int texture, int cell_w, int cell_h)
{
	font_tex = texture;
	font_w = cell_w;
	font_h = cell_h;
}

void
gui_SetColor(float r, float g, float b, float a)
{
	colour[0] = r;
	colour[1] = g;
	colour[2] = b;
	colour[3] = a;
}

void
gui_Rect(float x, float y, float w, float h)
{
	gfx_Quad(x, y, w, h, 0, 0.0f, 0.0f, 1.0f, 1.0f, colour);
}

void
gui_Image(float x, float y, float w, float h, unsigned int texture)
{
	gfx_Quad(x, y, w, h, texture, 0.0f, 0.0f, 1.0f, 1.0f, colour);
}

float
gui_TextWidth(const char *text, float scale)
{
	int n;

	n = 0;
	while (text != 0 && text[n] != 0)
		n++;
	return (float)n * (float)font_w * scale;
}

void
gui_Text(float x, float y, float scale, const char *text)
{
	int i;
	int c;
	int col;
	int row;
	float u0;
	float v0;
	float cw;
	float ch;

	if (font_tex == 0 || text == 0)
		return;

	cw = 1.0f / 16.0f;
	ch = 1.0f / 6.0f;

	for (i = 0; text[i] != 0; i++) {
		c = (unsigned char)text[i];
		if (c < 32 || c > 127)
			c = '?';
		c -= 32;
		col = c % 16;
		row = c / 16;
		u0 = (float)col * cw;
		v0 = (float)row * ch;
		gfx_Quad(x + (float)i * (float)font_w * scale, y,
		    (float)font_w * scale, (float)font_h * scale, font_tex,
		    u0, v0, u0 + cw, v0 + ch, colour);
	}
}

int
gui_MouseIn(float x, float y, float w, float h)
{
	float mx;
	float my;

	if (input == 0)
		return 0;
	mx = (float)input->mouse_x;
	my = (float)input->mouse_y;
	return mx >= x && mx < x + w && my >= y && my < y + h;
}

int
gui_Button(float x, float y, float w, float h, const char *label)
{
	int id;
	int over;
	int clicked;
	float text_x;
	float saved[4];
	int i;

	widget++;
	id = widget;
	over = gui_MouseIn(x, y, w, h);
	clicked = 0;

	if (over && mouse_down && !mouse_was_down)
		active = id;
	if (active == id && over && !mouse_down && mouse_was_down)
		clicked = 1;

	for (i = 0; i < 4; i++)
		saved[i] = colour[i];

	if (active == id && over)
		gui_SetColor(0.35f, 0.35f, 0.40f, 0.95f);
	else if (over)
		gui_SetColor(0.28f, 0.28f, 0.32f, 0.95f);
	else
		gui_SetColor(0.16f, 0.16f, 0.18f, 0.90f);
	gui_Rect(x, y, w, h);

	gui_SetColor(saved[0], saved[1], saved[2], saved[3]);
	text_x = x + (w - gui_TextWidth(label, 1.0f)) * 0.5f;
	gui_Text(text_x, y + (h - (float)font_h) * 0.5f, 1.0f, label);
	return clicked;
}

int
gui_Slider(float x, float y, float w, float h, float *value)
{
	int id;
	int over;
	int changed;
	float t;
	float knob;
	float saved[4];
	int i;

	widget++;
	id = widget;
	over = gui_MouseIn(x, y, w, h);
	changed = 0;

	if (over && mouse_down && !mouse_was_down)
		active = id;

	if (active == id && mouse_down && input != 0 && w > 1.0f) {
		t = ((float)input->mouse_x - x) / w;
		t = m3_clampf(t, 0.0f, 1.0f);
		if (t != *value) {
			*value = t;
			changed = 1;
		}
	}

	for (i = 0; i < 4; i++)
		saved[i] = colour[i];

	gui_SetColor(0.14f, 0.14f, 0.16f, 0.90f);
	gui_Rect(x, y, w, h);
	gui_SetColor(0.55f, 0.55f, 0.62f, 0.95f);
	knob = m3_clampf(*value, 0.0f, 1.0f) * (w - h);
	gui_Rect(x + knob, y, h, h);
	gui_SetColor(saved[0], saved[1], saved[2], saved[3]);
	return changed;
}
