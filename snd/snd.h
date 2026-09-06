/*
 *	@(#)snd.h	1.0
 *
 *  A software mixer.  You were right that you do not need OpenAL for this:
 *  distance attenuation is one divide and stereo panning is one dot
 *  product, and that is the whole of "3D sound" for a game of this size.
 *
 *  What OpenAL would additionally give you - HRTF, Doppler, EFX reverb,
 *  hardware voices - is exactly the part you said you did not want.  What
 *  you cannot avoid is a way to hand samples to the operating system, and
 *  that lives in plat_AudioWrite(), sixty lines, no library.
 *
 *  Mixing is a buffer operation like any other: N sources of shorts into
 *  one buffer of shorts.
 */
#ifndef SND_H_SENTRY
#define SND_H_SENTRY

#include "../core/m3.h"
#include "../asset/wav.h"

#define SND_VOICES 32

void snd_Init(int device_rate);
void snd_Listener(const vector position, const vector forward);
void snd_SetRange(float reference, float maximum);
/* NOTE:  full volume until `reference` metres, silent past `maximum`,
 *	  1/distance in between.  Two numbers, no model to configure.
 */

int snd_Play(const struct wav *sound, const vector position, float gain,
    int loop);
int snd_Play2D(const struct wav *sound, float gain, int loop);
/* RETURN VALUE: a voice id, or -1 when all voices are busy.  A voice id is
 *		 valid until the sound ends; snd_Stop() on a finished voice
 *		 is harmless.
 */
void snd_Move(int voice, const vector position);
void snd_Stop(int voice);
void snd_StopAll(void);
int snd_Busy(void);

void snd_Mix(short *out, int frames);
/* USAGE:
	int room = plat_AudioSpace();
	if (room > 0) { snd_Mix(scratch, room); plat_AudioWrite(scratch, room); }
   NOTE:
	stereo interleaved, always exactly `frames` frames written, silence
	included.  Nothing here allocates, blocks or takes a lock.
*/

#endif /* SND_H_SENTRY */
