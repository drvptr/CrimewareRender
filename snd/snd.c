#include <math.h>
#include "snd.h"

struct voice {
	const short *pcm;
	long frames;
	int channels;
	long step;		/* 16.16 source frames per output frame */
	long position;		/* 16.16					*/
	float gain;
	vector at;
	int spatial;
	int loop;
	int busy;
};

static struct voice voices[SND_VOICES];
static int device_rate = 44100;
static vector ear_pos;
static vector ear_right;
static float range_ref = 4.0f;
static float range_max = 60.0f;

void
snd_Init(int rate)
{
	int i;

	device_rate = rate > 0 ? rate : 44100;
	for (i = 0; i < SND_VOICES; i++)
		voices[i].busy = 0;
	VEC_ZERO(ear_pos);
	VEC_SET(ear_right, 1.0f, 0.0f, 0.0f);
}

void
snd_Listener(const vector position, const vector forward)
{
	vector up;
	vector dir;
	vector right;

	VEC_ASSIGMENT(position, ear_pos);
	VEC_SET(up, 0.0f, 1.0f, 0.0f);
	vec_norm(forward, dir);
	vec_cross(dir, up, right);
	vec_norm(right, ear_right);
}

void
snd_SetRange(float reference, float maximum)
{
	if (reference > 0.01f)
		range_ref = reference;
	if (maximum > range_ref)
		range_max = maximum;
}

static int
start(const struct wav *sound, float gain, int loop)
{
	int i;
	struct voice *v;

	if (sound == 0 || sound->pcm == 0 || sound->frames <= 0)
		return -1;

	for (i = 0; i < SND_VOICES; i++) {
		if (!voices[i].busy)
			break;
	}
	if (i == SND_VOICES)
		return -1;

	v = &voices[i];
	v->pcm = sound->pcm;
	v->frames = sound->frames;
	v->channels = sound->channels;
	v->step = ((long)sound->rate << 16) / device_rate;
	v->position = 0;
	v->gain = gain;
	v->loop = loop;
	v->busy = 1;
	v->spatial = 0;
	VEC_ZERO(v->at);
	return i;
}

int
snd_Play(const struct wav *sound, const vector position, float gain, int loop)
{
	int id;

	id = start(sound, gain, loop);
	if (id < 0)
		return -1;
	voices[id].spatial = 1;
	VEC_ASSIGMENT(position, voices[id].at);
	return id;
}

int
snd_Play2D(const struct wav *sound, float gain, int loop)
{
	return start(sound, gain, loop);
}

void
snd_Move(int voice, const vector position)
{
	if (voice < 0 || voice >= SND_VOICES)
		return;
	VEC_ASSIGMENT(position, voices[voice].at);
}

void
snd_Stop(int voice)
{
	if (voice < 0 || voice >= SND_VOICES)
		return;
	voices[voice].busy = 0;
}

void
snd_StopAll(void)
{
	int i;

	for (i = 0; i < SND_VOICES; i++)
		voices[i].busy = 0;
}

int
snd_Busy(void)
{
	int i;
	int n;

	n = 0;
	for (i = 0; i < SND_VOICES; i++) {
		if (voices[i].busy)
			n++;
	}
	return n;
}

/*  Volume for the left and right ear.  Distance first, then which side of
 *  the head it is on.  Nothing else.
 */
static void
levels(const struct voice *v, float *left, float *right)
{
	vector delta;
	vector unit;
	float distance;
	float attenuation;
	float pan;

	if (!v->spatial) {
		*left = v->gain;
		*right = v->gain;
		return;
	}

	vec_sub(v->at, ear_pos, delta);
	distance = vec_abs(delta);

	if (distance >= range_max) {
		*left = 0.0f;
		*right = 0.0f;
		return;
	}
	if (distance <= range_ref)
		attenuation = 1.0f;
	else
		attenuation = range_ref / distance;

	pan = 0.0f;
	if (distance > 0.001f) {
		vec_scalar_mul(delta, 1.0f / distance, unit);
		pan = vec_dot(unit, ear_right);
	}
	pan = m3_clampf(pan, -1.0f, 1.0f);

	/*  Constant power: the two gains squared add up to one, so walking
	 *  past a source does not make it louder in the middle.
	 */
	*left = v->gain * attenuation * sqrtf((1.0f - pan) * 0.5f);
	*right = v->gain * attenuation * sqrtf((1.0f + pan) * 0.5f);
}

static int
clip(int sample)
{
	if (sample > 32767)
		return 32767;
	if (sample < -32768)
		return -32768;
	return sample;
}

void
snd_Mix(short *out, int frames)
{
	int i;
	int f;
	long index;
	int sample;
	float left;
	float right;
	struct voice *v;
	int acc_l;
	int acc_r;

	for (f = 0; f < frames; f++) {
		out[f * 2 + 0] = 0;
		out[f * 2 + 1] = 0;
	}

	for (i = 0; i < SND_VOICES; i++) {
		v = &voices[i];
		if (!v->busy)
			continue;

		levels(v, &left, &right);
		if (left <= 0.0001f && right <= 0.0001f && !v->loop) {
			/*  Still has to advance, or a sound that starts far
			 *  away would play from its beginning when you walk
			 *  up to it.
			 */
		}

		for (f = 0; f < frames; f++) {
			index = v->position >> 16;
			if (index >= v->frames) {
				if (!v->loop) {
					v->busy = 0;
					break;
				}
				v->position = 0;
				index = 0;
			}

			if (v->channels == 1) {
				sample = v->pcm[index];
				acc_l = (int)((float)sample * left);
				acc_r = (int)((float)sample * right);
			} else {
				acc_l = (int)((float)v->pcm[index * 2] * left);
				acc_r = (int)((float)v->pcm[index * 2 + 1] *
				    right);
			}

			out[f * 2 + 0] = (short)clip(out[f * 2 + 0] + acc_l);
			out[f * 2 + 1] = (short)clip(out[f * 2 + 1] + acc_r);
			v->position += v->step;
		}
	}
}
