/*
 *	@(#)audio_alsa.c	1.0-вектор
 *
 *  ALSA, прямой линковкой с -lasound. Прототипы объявлены здесь: это
 *  "простая" часть API ALSA, она не менялась столько же, сколько
 *  существует, а заголовки тянут за собой пакет -dev.
 *
 *  Модель push, а не колбэк: спрашиваем у устройства, сколько влезет,
 *  микшируем ровно столько, пишем. Ни потока, ни мьютекса.
 */
#include "plat.h"

#define SND_PCM_STREAM_PLAYBACK		0
#define SND_PCM_FORMAT_S16_LE		2
#define SND_PCM_ACCESS_RW_INTERLEAVED	3

extern int snd_pcm_open(void **, const char *, int, int);
extern int snd_pcm_set_params(void *, int, int, unsigned int, unsigned int,
    int, unsigned int);
extern long snd_pcm_avail_update(void *);
extern long snd_pcm_writei(void *, const void *, unsigned long);
extern int snd_pcm_recover(void *, int, int);
extern int snd_pcm_close(void *);

static void *pcm;

int
plat_AudioOpen(int rate, int channels)
{
	int err;

	if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		pcm = 0;
		return 0;
	}
	err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
	    SND_PCM_ACCESS_RW_INTERLEAVED, (unsigned)channels,
	    (unsigned)rate, 1, 60000);
	if (err < 0) {
		snd_pcm_close(pcm);
		pcm = 0;
		return 0;
	}
	return 1;
}

int
plat_AudioSpace(void)
{
	long avail;

	if (pcm == 0)
		return 0;
	avail = snd_pcm_avail_update(pcm);
	if (avail < 0) {
		/*  Опустошение буфера: устройство встало, слышно щелчком.
		 *  Без восстановления звук пропал бы до конца сессии.
		 */
		snd_pcm_recover(pcm, (int)avail, 1);
		return 0;
	}
	return (int)avail;
}

int
plat_AudioWrite(const short *data, int frames)
{
	long n;

	if (pcm == 0 || frames <= 0)
		return 0;
	n = snd_pcm_writei(pcm, data, (unsigned long)frames);
	if (n < 0) {
		snd_pcm_recover(pcm, (int)n, 1);
		return 0;
	}
	return (int)n;
}

void
plat_AudioClose(void)
{
	if (pcm != 0)
		snd_pcm_close(pcm);
	pcm = 0;
}
