/*
 *	@(#)audio_none.c	1.0-вектор
 *
 *  Тишина. Ставится по умолчанию на этой ветке, чтобы сборка --static не
 *  требовала libasound. Игра идёт молча; отсутствие звуковой карты никогда
 *  не было поводом не запускаться.
 */
#include "plat.h"

int
plat_AudioOpen(int rate, int channels)
{
	(void)rate;
	(void)channels;
	return 0;
}

int
plat_AudioSpace(void)
{
	return 0;
}

int
plat_AudioWrite(const short *pcm, int frames)
{
	(void)pcm;
	return frames;
}

void
plat_AudioClose(void)
{
}
