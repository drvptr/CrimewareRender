#include "app.h"
#include "../gfx/gfx.h"
#include "../snd/snd.h"

#define AUDIO_SCRATCH_FRAMES 8192

static struct plat_input input;
static int running;
static float measured_fps;
static short audio_scratch[AUDIO_SCRATCH_FRAMES * 2];
static int audio_on;

const struct plat_input *
app_Input(void)
{
	return &input;
}

int
app_Width(void)
{
	return input.width;
}

int
app_Height(void)
{
	return input.height;
}

float
app_Fps(void)
{
	return measured_fps;
}

void
app_Quit(void)
{
	running = 0;
}

static void
push_audio(void)
{
	int room;

	if (!audio_on)
		return;
	room = plat_AudioSpace();
	if (room <= 0)
		return;
	if (room > AUDIO_SCRATCH_FRAMES)
		room = AUDIO_SCRATCH_FRAMES;
	snd_Mix(audio_scratch, room);
	plat_AudioWrite(audio_scratch, room);
}

int
app_Run(struct app_hooks *h)
{
	float step;
	double now;
	double previous;
	double behind;
	double frame_start;
	double budget;
	int steps;
	int frames;
	double fps_clock;
	int i;

	if (h == 0 || h->step == 0 || h->draw == 0)
		return 1;

	step = h->step_hz > 0.0f ? 1.0f / h->step_hz : 1.0f / 60.0f;

	if (!plat_Init())
		return 1;
	if (!plat_OpenWindow(h->title != 0 ? h->title : "engine",
	    h->width > 0 ? h->width : 800, h->height > 0 ? h->height : 600)) {
		plat_Shutdown();
		return 1;
	}
	if (!gfx_Init()) {
		plat_CloseWindow();
		plat_Shutdown();
		return 1;
	}

	for (i = 0; i < PLAT_KEYS; i++) {
		input.hold[i] = 0;
		input.hit[i] = 0;
		input.rel[i] = 0;
	}
	input.width = h->width > 0 ? h->width : 800;
	input.height = h->height > 0 ? h->height : 600;
	input.quit = 0;
	gfx_Viewport(input.width, input.height);

	audio_on = 0;
	if (h->audio_rate > 0) {
		audio_on = plat_AudioOpen(h->audio_rate, 2);
		snd_Init(h->audio_rate);
	}

	if (h->init != 0 && h->init(h->user) != 0) {
		gfx_Shutdown();
		plat_CloseWindow();
		plat_Shutdown();
		return 1;
	}

	running = 1;
	previous = plat_Time();
	behind = 0.0;
	frames = 0;
	fps_clock = previous;

	while (running) {
		frame_start = plat_Time();
		now = frame_start;
		behind += now - previous;
		previous = now;

		plat_Poll(&input);
		if (input.quit)
			running = 0;
		if (input.resized)
			gfx_Viewport(input.width, input.height);

		/*  Cap the catch up.  Without this, one long stall (a
		 *  breakpoint, a swapped out page) makes the loop try to
		 *  simulate the missing seconds, which takes longer than
		 *  they did, and the game never recovers.
		 */
		steps = 0;
		while (behind >= (double)step && steps < 5) {
			h->step(h->user, step);
			behind -= (double)step;
			steps++;
		}
		if (behind > (double)step * 5.0)
			behind = 0.0;

		h->draw(h->user, (float)(behind / (double)step));
		gfx_EndFrame();
		plat_Swap();
		push_audio();

		frames++;
		if (now - fps_clock >= 0.5) {
			measured_fps = (float)((double)frames /
			    (now - fps_clock));
			frames = 0;
			fps_clock = now;
		}

		if (h->max_fps > 0) {
			budget = 1.0 / (double)h->max_fps;
			now = plat_Time();
			if (now - frame_start < budget)
				plat_Sleep(budget - (now - frame_start));
		}
	}

	if (h->quit != 0)
		h->quit(h->user);
	gfx_Shutdown();
	plat_AudioClose();
	plat_CloseWindow();
	plat_Shutdown();
	return 0;
}
