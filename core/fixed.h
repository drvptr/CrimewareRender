/*
 *	@(#)fixed.h	1.0-вектор
 *
 *  Целочисленная арифметика с фиксированной точкой. Основа твоя, добавлено
 *  то, без чего конвейер разваливается на больших N.
 *
 *  Число хранится как int, младшие FIXED_BITS бит - дробная часть. При
 *  FIXED_BITS = 4 шаг сетки 1/16, и это ровно та грубость, ради которой всё
 *  затевалось: вершины прыгают по сетке, текстуры плывут, треугольники
 *  становятся угловатыми.
 *
 *  Собирается с -DFIXED_BITS=n. Значения, которые имеет смысл пробовать:
 *
 *	4	твоё исходное. Шаг 1/16 пикселя. Всё трясётся, геометрия
 *		узнаваема, текстуры уезжают. Разгар безумия.
 *	8	примерно PlayStation: заметный джиттер, но играбельно.
 *	12	лёгкое дрожание на дальних объектах.
 *	16	визуально почти неотличимо от float.
 *
 *  ПРО ПЕРЕПОЛНЕНИЕ. Твой макрос mul(a,b) = (a*b)>>N честен только пока
 *  произведение влезает в int. a и b - это x<<N и y<<N, значит a*b = x*y<<2N.
 *  При N=4 это переполняется на x*y около 8 миллионов - далеко, не мешает.
 *  При N=16 - на x*y = 0.5, то есть сразу. Поэтому ниже есть две версии:
 *  твоя быстрая (FIXED_MUL_FAST) и через long long (fixed_mul), которая
 *  верна при любом N. В конвейере используется вторая; на N=4 они дают
 *  одинаковый результат.
 */
#ifndef FIXED_H_SENTRY
#define FIXED_H_SENTRY

#ifndef FIXED_BITS
#define FIXED_BITS 4
#endif

typedef int fixed;

#define FIXED_ONE ((fixed)1 << FIXED_BITS)
#define FIXED_HALF ((fixed)1 << (FIXED_BITS - 1))
#define FIXED_INF 2147483647

/*  Твои макросы, как были.  */
#define FIXED_MUL_FAST(a, b) (((a) * (b)) >> FIXED_BITS)
#define FIXED_DIV_FAST(a, b) (((a) << FIXED_BITS) / (b))
#define FIXED_TO_INT(f) ((f) >> FIXED_BITS)
#define INT_TO_FIXED(i) ((fixed)(i) << FIXED_BITS)
#define FIXED_TO_FLOAT(f) ((float)(f) / (float)FIXED_ONE)
#define FLOAT_TO_FIXED(f) ((fixed)((f) * (float)FIXED_ONE))

/*  Безопасные версии: промежуточное произведение в 64 битах.  */
static inline fixed
fixed_mul(fixed a, fixed b)
{
	return (fixed)(((long long)a * (long long)b) >> FIXED_BITS);
}

static inline fixed
fixed_div(fixed a, fixed b)
{
	if (b == 0)
		return a >= 0 ? FIXED_INF : -FIXED_INF;
	return (fixed)((((long long)a) << FIXED_BITS) / (long long)b);
}

static inline fixed
fixed_abs(fixed a)
{
	return a < 0 ? -a : a;
}

static inline fixed
fixed_clamp(fixed v, fixed lo, fixed hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

/*  Корень методом Ньютона по целым. Нужен нормализации векторов; sqrtf
 *  здесь звать нельзя, иначе вся затея теряет смысл.
 */
static inline fixed
fixed_sqrt(fixed a)
{
	long long x;
	long long guess;
	int i;

	if (a <= 0)
		return 0;

	/*  Ищем корень из a * 2^N, потому что sqrt(x)<<N = sqrt(x<<2N).  */
	x = ((long long)a) << FIXED_BITS;
	guess = x > FIXED_ONE ? x >> 1 : FIXED_ONE;
	for (i = 0; i < 16; i++) {
		if (guess == 0)
			break;
		guess = (guess + x / guess) >> 1;
	}
	return (fixed)guess;
}

/*  Линейная интерполяция: a + (b - a) * t, t от 0 до FIXED_ONE.  */
static inline fixed
fixed_lerp(fixed a, fixed b, fixed t)
{
	return a + fixed_mul(b - a, t);
}

#endif /* FIXED_H_SENTRY */
