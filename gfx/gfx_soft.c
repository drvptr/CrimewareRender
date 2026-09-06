/*
 *	@(#)gfx_soft.c	1.0-вектор
 *
 *  Программный растеризатор. Реализует тот же gfx.h, что и gfx_gl.c, и
 *  этим проверяет твой тезис про модульность: весь переход на целочисленную
 *  арифметику живёт внутри одного файла, ни один другой файл движка о нём
 *  не знает.
 *
 *  ВЕСЬ КОНВЕЙЕР В FIXED-POINT. Матрицы переводятся в fixed при
 *  gfx_SetCamera, вершины умножаются на них через fixed_mul, деление на w
 *  через fixed_div, интерполяция по треугольнику - fixed. Ни одного float
 *  ниже границы модуля, кроме перевода входных данных на входе.
 *
 *  Отсюда весь вид эпохи PlayStation, и он не подделан, а получается сам:
 *
 *	*  вершины прыгают по сетке 1/2^N - геометрия шевелится при движении
 *	   камеры, потому что координаты округляются в мировом пространстве;
 *	*  текстуры плывут - интерполяция u,v ЛИНЕЙНАЯ по экрану, без
 *	   коррекции перспективы. Именно так делала PS1, и именно поэтому у
 *	   неё текстуры на полу текли. Коррекция стоила бы деления на пиксель;
 *	*  треугольники угловатые - края считаются с точностью 1/2^N пикселя;
 *	*  полигоны пропадают у самого носа - отсечение только по ближней
 *	   плоскости, без боковых.
 *
 *  Собери с -DFIXED_BITS=8 или 12, и всё это плавно исчезнет.
 *
 *  ЧЕГО ЗДЕСЬ НЕТ И НЕ БУДЕТ: мипмапов, билинейной фильтрации, сглаживания,
 *  коррекции перспективы, отсечения по боковым плоскостям. Всё это делает
 *  видеокарта, и делает лучше; смысл этого файла в том, чтобы показать, где
 *  проходит граница модуля, а не соревноваться с OpenGL.
 */
#include <stdio.h>
#include "gfx.h"
#include "../core/fixed.h"
#include "../plat/plat.h"
#include "../buf/buffer.h"

#define SOFT_MAX_W 1920
#define SOFT_MAX_H 1200
#define SOFT_MAX_MESHES 1024
#define SOFT_MAX_TEXTURES 256

/*  Ближе этого по w треугольник обрезается. В fixed, чтобы не делить на
 *  ноль и не переворачивать знак при перспективном делении.
 */
#define NEAR_W (FIXED_ONE / 4)

/*  РАЗНЫЕ Q-ФОРМАТЫ НА РАЗНЫХ СТАДИЯХ, и это не отступление от затеи, а
 *  как раз то, как фиксированную точку применяли на самом деле.
 *
 *  Первая версия этого файла интерполировала всё в том же FIXED_BITS, и на
 *  N=4 не рисовалось ничего: параметр вдоль отрезка считается как
 *  (FIXED_ONE << N) / длина_в_fixed, для отрезка в 300 пикселей это
 *  16*16/4800 = 0, шаг нулевой, все пиксели получают координату первого.
 *  Доля от нуля до единицы при N=4 имеет ровно 16 ступеней - на весь
 *  треугольник.
 *
 *  Поэтому ГЕОМЕТРИЯ (матрицы, вершины, экранные координаты) живёт в
 *  FIXED_BITS - оттуда весь эффект, - а ИНТЕРПОЛЯТОРЫ вдоль рёбер и
 *  отрезков считаются в Q16. Так и делали: одна точность на трансформацию,
 *  другая на растеризацию.
 */
#define IBITS 16
#define IONE (1 << IBITS)

static inline int
ilerp(int a, int b, int t)
{
	return a + (int)((((long long)(b - a)) * t) >> IBITS);
}

/*  Перевод из FIXED_BITS в Q16 и обратно.  */
#if IBITS >= FIXED_BITS
#define TO_Q16(f) ((int)(f) << (IBITS - FIXED_BITS))
#else
#define TO_Q16(f) ((int)(f) >> (FIXED_BITS - IBITS))
#endif

struct soft_mesh {
	const struct gfx_vertex *verts;
	int nverts;
	const unsigned short *index;
	int nindex;
	int used;
};

struct soft_texture {
	const unsigned char *pixels;
	int w;
	int h;
	int unit;
	int repeat;
	int used;
};

/*  Вершина после преобразования.  */
struct sv {
	fixed cx;	/* клип-координаты, FIXED_BITS		*/
	fixed cy;
	fixed cw;
	fixed x;	/* экран после деления, FIXED_BITS	*/
	fixed y;
	int iw;		/* 1/w, оно же глубина, Q16		*/
	int u;		/* координаты текстуры, Q16		*/
	int v;
	int light;	/* яркость 0..IONE, Q16			*/
};

static struct soft_mesh meshes[SOFT_MAX_MESHES];
static struct soft_texture textures[SOFT_MAX_TEXTURES];
static unsigned int next_texture = 1;

static unsigned int *canvas;
static fixed zbuf[SOFT_MAX_W * SOFT_MAX_H];
static int screen_w = 640;
static int screen_h = 480;

static fixed mat_view[16];
static fixed mat_proj[16];
static fixed mat_viewproj[16];
static fixed mat_mvp[16];
static fixed mat_model[16];

static fixed light_dir[3];
static fixed light_ambient;
static int fog_start;	/* Q16 */
static int fog_end;
static int fog_colour;
static int clear_colour;
static int draw_calls;
static int mode_2d;

/* ------------------------------------------------------------- утилиты */

static void
mat_to_fixed(const float m[16], fixed out[16])
{
	int i;

	for (i = 0; i < 16; i++)
		out[i] = FLOAT_TO_FIXED(m[i]);
}

static void
mat_mul_fixed(const fixed a[16], const fixed b[16], fixed out[16])
{
	fixed t[16];
	int col;
	int row;
	int i;
	fixed sum;

	for (col = 0; col < 4; col++) {
		for (row = 0; row < 4; row++) {
			sum = 0;
			for (i = 0; i < 4; i++)
				sum += fixed_mul(a[i * 4 + row],
				    b[col * 4 + i]);
			t[col * 4 + row] = sum;
		}
	}
	for (i = 0; i < 16; i++)
		out[i] = t[i];
}

static int
clamp_byte(int v)
{
	if (v < 0)
		return 0;
	if (v > 255)
		return 255;
	return v;
}

static int
pack_colour(const float rgba[4])
{
	int r;
	int g;
	int b;
	int a;

	r = clamp_byte((int)(rgba[0] * 255.0f));
	g = clamp_byte((int)(rgba[1] * 255.0f));
	b = clamp_byte((int)(rgba[2] * 255.0f));
	a = clamp_byte((int)(rgba[3] * 255.0f));
	return (a << 24) | (r << 16) | (g << 8) | b;
}

/* ------------------------------------------------------- жизненный цикл */

int
gfx_Init(void)
{
	unsigned long long geo;

	canvas = plat_Framebuffer(&geo);
	if (canvas == 0) {
		fprintf(stderr, "gfx: этот бэкенд платформы не даёт холста, "
		    "программному рендеру рисовать некуда\n");
		return 0;
	}
	screen_w = (int)BUF_COLS(geo);
	screen_h = (int)BUF_ROWS(geo);

	light_ambient = FLOAT_TO_FIXED(0.35f);
	light_dir[0] = FLOAT_TO_FIXED(-0.4f);
	light_dir[1] = FLOAT_TO_FIXED(-0.8f);
	light_dir[2] = FLOAT_TO_FIXED(-0.4f);
	fog_start = 0;
	fog_end = 0;

	printf("gfx: программный рендер, fixed-point, %d бит дробной части "
	    "(шаг сетки 1/%d)\n", FIXED_BITS, FIXED_ONE);
	return 1;
}

void
gfx_Shutdown(void)
{
}

void
gfx_Viewport(int width, int height)
{
	unsigned long long geo;

	canvas = plat_Framebuffer(&geo);
	if (canvas != 0) {
		screen_w = (int)BUF_COLS(geo);
		screen_h = (int)BUF_ROWS(geo);
	} else {
		screen_w = width;
		screen_h = height;
	}
	if (screen_w > SOFT_MAX_W)
		screen_w = SOFT_MAX_W;
	if (screen_h > SOFT_MAX_H)
		screen_h = SOFT_MAX_H;
}

void
gfx_BeginFrame(float r, float g, float b)
{
	unsigned long long geo;
	long i;
	long n;
	float rgba[4];

	canvas = plat_Framebuffer(&geo);
	if (canvas != 0) {
		screen_w = (int)BUF_COLS(geo);
		screen_h = (int)BUF_ROWS(geo);
		if (screen_w > SOFT_MAX_W)
			screen_w = SOFT_MAX_W;
		if (screen_h > SOFT_MAX_H)
			screen_h = SOFT_MAX_H;
	}

	rgba[0] = r;
	rgba[1] = g;
	rgba[2] = b;
	rgba[3] = 1.0f;
	clear_colour = pack_colour(rgba);

	draw_calls = 0;
	mode_2d = 0;
	n = (long)screen_w * (long)screen_h;
	for (i = 0; i < n; i++) {
		canvas[i] = (unsigned int)clear_colour;
		zbuf[i] = 0;	/* 1/w = 0 это бесконечно далеко */
	}
}

void
gfx_EndFrame(void)
{
}

int
gfx_DrawCalls(void)
{
	return draw_calls;
}

/* --------------------------------------------------------- дескрипторы */

unsigned int
gfx_MakeTexture(void *pixels, unsigned long long geo, int smooth, int repeat)
{
	unsigned int slot;

	(void)smooth;	/* фильтрации здесь нет и не планируется */
	if (pixels == 0 || !bufGeomCheck(geo) || !bufGeomIsPacked(geo))
		return 0;
	if (BUF_UNIT(geo) != 3 && BUF_UNIT(geo) != 4)
		return 0;
	if (next_texture >= SOFT_MAX_TEXTURES)
		return 0;

	slot = next_texture++;
	/*  ВНИМАНИЕ, ОТЛИЧИЕ ОТ gfx_gl.c: пиксели не копируются никуда.
	 *  У видеокарты есть своя память, и после glTexImage2D холст можно
	 *  выбросить. Здесь его выбрасывать нельзя - он и есть текстура.
	 */
	textures[slot].pixels = (const unsigned char *)pixels;
	textures[slot].w = (int)BUF_COLS(geo);
	textures[slot].h = (int)BUF_ROWS(geo);
	textures[slot].unit = (int)BUF_UNIT(geo);
	textures[slot].repeat = repeat;
	textures[slot].used = 1;
	return slot;
}

void
gfx_FreeTexture(unsigned int tex)
{
	if (tex > 0 && tex < SOFT_MAX_TEXTURES)
		textures[tex].used = 0;
}

unsigned int
gfx_MakeMesh(const struct gfx_vertex *verts, int nverts,
    const unsigned short *index, int nindex, int dynamic)
{
	int slot;

	(void)dynamic;
	if (verts == 0 || nverts <= 0 || index == 0 || nindex <= 0)
		return 0;

	for (slot = 0; slot < SOFT_MAX_MESHES; slot++) {
		if (!meshes[slot].used)
			break;
	}
	if (slot == SOFT_MAX_MESHES)
		return 0;

	/*  Тоже без копии: массив вершин остаётся у игры, в арене. Поэтому
	 *  gfx_UpdateMesh здесь вообще ничего не делает - игра пишет в свой
	 *  же массив, и рендер видит новые вершины сразу.
	 */
	meshes[slot].verts = verts;
	meshes[slot].nverts = nverts;
	meshes[slot].index = index;
	meshes[slot].nindex = nindex;
	meshes[slot].used = 1;
	return (unsigned int)(slot + 1);
}

void
gfx_UpdateMesh(unsigned int mesh, const struct gfx_vertex *verts, int nverts)
{
	(void)mesh;
	(void)verts;
	(void)nverts;
}

void
gfx_FreeMesh(unsigned int mesh)
{
	if (mesh > 0 && mesh <= SOFT_MAX_MESHES)
		meshes[mesh - 1].used = 0;
}

void
gfx_SetCamera(const float view[16], const float proj[16])
{
	mat_to_fixed(view, mat_view);
	mat_to_fixed(proj, mat_proj);
	mat_mul_fixed(mat_proj, mat_view, mat_viewproj);
}

void
gfx_SetLight(const vector direction, float ambient)
{
	vector d;

	vec_norm(direction, d);
	light_dir[0] = FLOAT_TO_FIXED(d[X]);
	light_dir[1] = FLOAT_TO_FIXED(d[Y]);
	light_dir[2] = FLOAT_TO_FIXED(d[Z]);
	light_ambient = FLOAT_TO_FIXED(ambient);
}

void
gfx_SetFog(float r, float g, float b, float start, float end)
{
	float rgba[4];

	rgba[0] = r;
	rgba[1] = g;
	rgba[2] = b;
	rgba[3] = 1.0f;
	fog_colour = pack_colour(rgba);
	fog_start = (int)(start * (float)IONE);
	fog_end = (int)(end * (float)IONE);
}

/* ---------------------------------------------------------- растеризация */

static fixed
sample_light(const struct gfx_vertex *v)
{
	fixed nx;
	fixed ny;
	fixed nz;
	fixed wx;
	fixed wy;
	fixed wz;
	fixed d;
	fixed l;

	nx = FLOAT_TO_FIXED(v->nx);
	ny = FLOAT_TO_FIXED(v->ny);
	nz = FLOAT_TO_FIXED(v->nz);

	/*  Нормаль поворачивается матрицей модели с w = 0: перенос ей не
	 *  нужен, она направление, а не точка.
	 */
	wx = fixed_mul(mat_model[0], nx) + fixed_mul(mat_model[4], ny) +
	    fixed_mul(mat_model[8], nz);
	wy = fixed_mul(mat_model[1], nx) + fixed_mul(mat_model[5], ny) +
	    fixed_mul(mat_model[9], nz);
	wz = fixed_mul(mat_model[2], nx) + fixed_mul(mat_model[6], ny) +
	    fixed_mul(mat_model[10], nz);

	d = -(fixed_mul(wx, light_dir[0]) + fixed_mul(wy, light_dir[1]) +
	    fixed_mul(wz, light_dir[2]));
	if (d < 0)
		d = 0;

	l = light_ambient + fixed_mul(d, FIXED_ONE - light_ambient);
	return fixed_clamp(l, 0, FIXED_ONE);
}

static void
transform(const struct gfx_vertex *v, struct sv *out)
{
	fixed px;
	fixed py;
	fixed pz;

	px = FLOAT_TO_FIXED(v->x);
	py = FLOAT_TO_FIXED(v->y);
	pz = FLOAT_TO_FIXED(v->z);

	out->cx = fixed_mul(mat_mvp[0], px) + fixed_mul(mat_mvp[4], py) +
	    fixed_mul(mat_mvp[8], pz) + mat_mvp[12];
	out->cy = fixed_mul(mat_mvp[1], px) + fixed_mul(mat_mvp[5], py) +
	    fixed_mul(mat_mvp[9], pz) + mat_mvp[13];
	out->cw = fixed_mul(mat_mvp[3], px) + fixed_mul(mat_mvp[7], py) +
	    fixed_mul(mat_mvp[11], pz) + mat_mvp[15];

	out->u = (int)(v->u * (float)IONE);
	out->v = (int)(v->v * (float)IONE);
	out->light = mode_2d ? IONE : TO_Q16(sample_light(v));
}

/*  Экранные координаты. Деление в 64 битах, результат в fixed: отсюда
 *  берётся зернистость краёв.
 */
static void
to_screen(struct sv *s)
{
	long long hw;
	long long hh;

	hw = (long long)screen_w << (FIXED_BITS - 1);
	hh = (long long)screen_h << (FIXED_BITS - 1);

	s->x = (fixed)(hw + (((long long)s->cx * hw) / s->cw));
	s->y = (fixed)(hh - (((long long)s->cy * hh) / s->cw));
	/*  1/w в Q16: (2^N * 2^16) / cw.  */
	s->iw = (int)((((long long)IONE) << FIXED_BITS) / s->cw);
}

/*  t в Q16. ilerp не зависит от формата a и b: это просто линейная смесь.  */
static struct sv
lerp_sv(const struct sv *a, const struct sv *b, int t)
{
	struct sv r;

	r.cx = ilerp(a->cx, b->cx, t);
	r.cy = ilerp(a->cy, b->cy, t);
	r.cw = ilerp(a->cw, b->cw, t);
	r.u = ilerp(a->u, b->u, t);
	r.v = ilerp(a->v, b->v, t);
	r.light = ilerp(a->light, b->light, t);
	r.x = ilerp(a->x, b->x, t);
	r.y = ilerp(a->y, b->y, t);
	r.iw = ilerp(a->iw, b->iw, t);
	return r;
}

/*  u и v в Q16, в долях текстуры. Выход за единицу - это укладка плитки.  */
static int
texel(const struct soft_texture *t, int u, int v)
{
	int x;
	int y;
	long off;
	const unsigned char *p;

	x = (int)((((long long)u) * t->w) >> IBITS);
	y = (int)((((long long)v) * t->h) >> IBITS);

	if (t->repeat) {
		x = x % t->w;
		y = y % t->h;
		if (x < 0)
			x += t->w;
		if (y < 0)
			y += t->h;
	} else {
		if (x < 0)
			x = 0;
		if (y < 0)
			y = 0;
		if (x >= t->w)
			x = t->w - 1;
		if (y >= t->h)
			y = t->h - 1;
	}

	off = ((long)y * t->w + x) * t->unit;
	p = t->pixels + off;
	if (t->unit == 4)
		return (p[3] << 24) | (p[0] << 16) | (p[1] << 8) | p[2];
	return (0xFF << 24) | (p[0] << 16) | (p[1] << 8) | p[2];
}

/*  Один горизонтальный отрезок. Здесь живёт вся стоимость кадра.  */
static void
span(int y, struct sv *l, struct sv *r, const struct soft_texture *tex,
    int tint, int test_depth)
{
	int x0;
	int x1;
	int x;
	long long width;
	int t;
	int z;
	int u;
	int v;
	int li;
	int dz;
	int du;
	int dv;
	int dli;
	int cr;
	int cg;
	int cb;
	int ca;
	int depth;
	int fog_t;
	long at;

	if (l->x > r->x) {
		struct sv *swap;

		swap = l;
		l = r;
		r = swap;
	}

	x0 = FIXED_TO_INT(l->x);
	x1 = FIXED_TO_INT(r->x);
	if (x1 < 0 || x0 >= screen_w)
		return;

	width = (long long)(r->x - l->x);
	if (width <= 0)
		return;

	/*  Шаг на ОДИН ПИКСЕЛЬ, а не доля вдоль отрезка: (attr_r - attr_l),
	 *  делённое на длину в пикселях. Длина в пикселях это width/2^N,
	 *  поэтому умножаем на FIXED_ONE. Одно деление на атрибут на
	 *  отрезок, ни одного на пиксель.
	 */
	dz = (int)(((long long)(r->iw - l->iw) * FIXED_ONE) / width);
	du = (int)(((long long)(r->u - l->u) * FIXED_ONE) / width);
	dv = (int)(((long long)(r->v - l->v) * FIXED_ONE) / width);
	dli = (int)(((long long)(r->light - l->light) * FIXED_ONE) / width);

	/*  Стартовое значение в центре первого целого пикселя отрезка.  */
	t = (int)((((long long)(INT_TO_FIXED(x0) - l->x)) << IBITS) / width);
	z = ilerp(l->iw, r->iw, t);
	u = ilerp(l->u, r->u, t);
	v = ilerp(l->v, r->v, t);
	li = ilerp(l->light, r->light, t);

	if (x0 < 0) {
		z += (int)((long long)dz * (0 - x0));
		u += (int)((long long)du * (0 - x0));
		v += (int)((long long)dv * (0 - x0));
		li += (int)((long long)dli * (0 - x0));
		x0 = 0;
	}
	if (x1 >= screen_w)
		x1 = screen_w - 1;

	at = (long)y * screen_w + x0;

	for (x = x0; x <= x1; x++, at++) {
		if (test_depth && z <= zbuf[at])
			goto next;

		if (tex != 0) {
			int tc;

			tc = texel(tex, u, v);
			ca = ((tc >> 24) & 0xFF) * ((tint >> 24) & 0xFF) / 255;
			if (ca < 8)
				goto next;
			cr = ((tc >> 16) & 0xFF) * ((tint >> 16) & 0xFF) / 255;
			cg = ((tc >> 8) & 0xFF) * ((tint >> 8) & 0xFF) / 255;
			cb = (tc & 0xFF) * (tint & 0xFF) / 255;
		} else {
			ca = (tint >> 24) & 0xFF;
			cr = (tint >> 16) & 0xFF;
			cg = (tint >> 8) & 0xFF;
			cb = tint & 0xFF;
			if (ca < 8)
				goto next;
		}

		cr = (int)(((long long)cr * li) >> IBITS);
		cg = (int)(((long long)cg * li) >> IBITS);
		cb = (int)(((long long)cb * li) >> IBITS);

		if (fog_end > fog_start && z > 0) {
			depth = (int)((((long long)IONE) << IBITS) / z);
			fog_t = (int)((((long long)(depth - fog_start)) <<
			    IBITS) / (fog_end - fog_start));
			if (fog_t < 0)
				fog_t = 0;
			if (fog_t > IONE)
				fog_t = IONE;
			cr = ilerp(cr, (fog_colour >> 16) & 0xFF, fog_t);
			cg = ilerp(cg, (fog_colour >> 8) & 0xFF, fog_t);
			cb = ilerp(cb, fog_colour & 0xFF, fog_t);
		}

		if (ca < 255) {
			int old;

			old = (int)canvas[at];
			cr = (cr * ca + ((old >> 16) & 0xFF) *
			    (255 - ca)) / 255;
			cg = (cg * ca + ((old >> 8) & 0xFF) *
			    (255 - ca)) / 255;
			cb = (cb * ca + (old & 0xFF) * (255 - ca)) / 255;
		}

		canvas[at] = (unsigned int)(0xFF000000 |
		    (clamp_byte(cr) << 16) | (clamp_byte(cg) << 8) |
		    clamp_byte(cb));
		if (test_depth)
			zbuf[at] = z;
next:
		z += dz;
		u += du;
		v += dv;
		li += dli;
	}
}

static void
raster(struct sv *a, struct sv *b, struct sv *c,
    const struct soft_texture *tex, int tint, int test_depth)
{
	struct sv *tmp;
	struct sv left;
	struct sv right;
	int y;
	int y0;
	int y1;
	int y2;
	int ymid;
	int t;
	long long area;

	/*  Отсечение задних граней: знак площади в экранных координатах.
	 *  В 64 битах, иначе на больших треугольниках произведение уедет.
	 */
	area = (long long)(b->x - a->x) * (long long)(c->y - a->y) -
	    (long long)(c->x - a->x) * (long long)(b->y - a->y);
	if (area >= 0)
		return;

	/*  Сортировка по y пузырьком: три элемента, три сравнения.  */
	if (a->y > b->y) {
		tmp = a;
		a = b;
		b = tmp;
	}
	if (b->y > c->y) {
		tmp = b;
		b = c;
		c = tmp;
	}
	if (a->y > b->y) {
		tmp = a;
		a = b;
		b = tmp;
	}

	y0 = FIXED_TO_INT(a->y);
	y2 = FIXED_TO_INT(c->y);
	ymid = FIXED_TO_INT(b->y);

	if (y2 < 0 || y0 >= screen_h)
		return;
	y1 = y0 < 0 ? 0 : y0;
	if (y2 >= screen_h)
		y2 = screen_h - 1;

	for (y = y1; y <= y2; y++) {
		/*  Длинная сторона a-c даёт одну границу, короткие a-b и
		 *  b-c - другую. Параметр ребра в Q16, иначе на N=4 у
		 *  треугольника было бы 16 различимых строк.
		 */
		if (c->y != a->y)
			t = (int)((((long long)(INT_TO_FIXED(y) - a->y)) <<
			    IBITS) / (c->y - a->y));
		else
			t = 0;
		/*  Зажимать обязательно. Строка y берётся целой, а y вершины
		 *  дробное, поэтому на границе между половинами треугольника
		 *  параметр выходит отрицательным - это уже не интерполяция,
		 *  а экстраполяция, и точка улетает за экран длинной полосой.
		 */
		t = t < 0 ? 0 : (t > IONE ? IONE : t);
		left = lerp_sv(a, c, t);

		if (y < ymid) {
			if (b->y != a->y)
				t = (int)((((long long)(INT_TO_FIXED(y) -
				    a->y)) << IBITS) / (b->y - a->y));
			else
				t = 0;
			t = t < 0 ? 0 : (t > IONE ? IONE : t);
			right = lerp_sv(a, b, t);
		} else {
			if (c->y != b->y)
				t = (int)((((long long)(INT_TO_FIXED(y) -
				    b->y)) << IBITS) / (c->y - b->y));
			else
				t = 0;
			t = t < 0 ? 0 : (t > IONE ? IONE : t);
			right = lerp_sv(b, c, t);
		}

		span(y, &left, &right, tex, tint, test_depth);
	}
}

/*  Отсечение по ближней плоскости, один проход Сазерленда-Ходжмена. Без
 *  него стена, в которую упёрся нос, делит на отрицательное w и
 *  выворачивается наизнанку через весь экран.
 */
static int
clip_near(struct sv *in, int n, struct sv *out)
{
	int i;
	int j;
	int m;
	int t;

	m = 0;
	for (i = 0; i < n; i++) {
		j = (i + 1) % n;
		if (in[i].cw >= NEAR_W)
			out[m++] = in[i];
		if ((in[i].cw >= NEAR_W) != (in[j].cw >= NEAR_W)) {
			t = (int)((((long long)(NEAR_W - in[i].cw)) <<
			    IBITS) / (in[j].cw - in[i].cw));
			out[m] = lerp_sv(&in[i], &in[j], t);
			out[m].cw = NEAR_W;
			m++;
		}
	}
	return m;
}

static void
draw_triangle(const struct gfx_vertex *v0, const struct gfx_vertex *v1,
    const struct gfx_vertex *v2, const struct soft_texture *tex, int tint,
    int test_depth)
{
	struct sv in[3];
	struct sv out[6];
	int n;
	int i;

	transform(v0, &in[0]);
	transform(v1, &in[1]);
	transform(v2, &in[2]);

	n = clip_near(in, 3, out);
	if (n < 3)
		return;

	for (i = 0; i < n; i++)
		to_screen(&out[i]);

	for (i = 1; i + 1 < n; i++)
		raster(&out[0], &out[i], &out[i + 1], tex, tint, test_depth);
}

/* ----------------------------------------------------------- рисование */

void
gfx_DrawMesh(unsigned int handle, const float model[16], unsigned int tex,
    const float rgba[4], int first, int count)
{
	struct soft_mesh *m;
	struct soft_texture *t;
	float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	int tint;
	int i;

	if (canvas == 0 || handle == 0 || handle > SOFT_MAX_MESHES)
		return;
	m = &meshes[handle - 1];
	if (!m->used)
		return;

	if (count < 0)
		count = m->nindex;
	if (first < 0)
		first = 0;
	if (first + count > m->nindex)
		count = m->nindex - first;
	if (count <= 0)
		return;

	mat_to_fixed(model, mat_model);
	mat_mul_fixed(mat_viewproj, mat_model, mat_mvp);

	t = 0;
	if (tex > 0 && tex < SOFT_MAX_TEXTURES && textures[tex].used)
		t = &textures[tex];
	tint = pack_colour(rgba != 0 ? rgba : white);

	for (i = first; i + 2 < first + count; i += 3) {
		draw_triangle(&m->verts[m->index[i]],
		    &m->verts[m->index[i + 1]],
		    &m->verts[m->index[i + 2]], t, tint, 1);
	}
	draw_calls++;
}

void
gfx_DrawSprite(const vector centre, float w, float h, unsigned int tex,
    const float rgba[4])
{
	struct gfx_vertex quad[4];
	struct soft_texture *t;
	float identity[16];
	float right[3];
	float up[3];
	int tint;
	int i;

	if (canvas == 0)
		return;

	/*  Оси камеры лежат строками в матрице вида. Здесь она уже в fixed,
	 *  поэтому обратно во float.
	 */
	right[0] = FIXED_TO_FLOAT(mat_view[0]);
	right[1] = FIXED_TO_FLOAT(mat_view[4]);
	right[2] = FIXED_TO_FLOAT(mat_view[8]);
	up[0] = FIXED_TO_FLOAT(mat_view[1]);
	up[1] = FIXED_TO_FLOAT(mat_view[5]);
	up[2] = FIXED_TO_FLOAT(mat_view[9]);

	for (i = 0; i < 4; i++) {
		float sx;
		float sy;

		sx = (i == 1 || i == 2) ? 0.5f : -0.5f;
		sy = (i >= 2) ? 0.5f : -0.5f;
		quad[i].x = centre[X] + right[0] * w * sx + up[0] * h * sy;
		quad[i].y = centre[Y] + right[1] * w * sx + up[1] * h * sy;
		quad[i].z = centre[Z] + right[2] * w * sx + up[2] * h * sy;
		quad[i].nx = 0.0f;
		quad[i].ny = 1.0f;
		quad[i].nz = 0.0f;
		quad[i].u = (i == 1 || i == 2) ? 1.0f : 0.0f;
		quad[i].v = (i >= 2) ? 1.0f : 0.0f;
	}

	for (i = 0; i < 16; i++)
		identity[i] = 0.0f;
	identity[0] = 1.0f;
	identity[5] = 1.0f;
	identity[10] = 1.0f;
	identity[15] = 1.0f;

	mat_to_fixed(identity, mat_model);
	mat_mul_fixed(mat_viewproj, mat_model, mat_mvp);

	t = 0;
	if (tex > 0 && tex < SOFT_MAX_TEXTURES && textures[tex].used)
		t = &textures[tex];
	tint = pack_colour(rgba);

	mode_2d = 1;	/* спрайт не освещается */
	draw_triangle(&quad[0], &quad[1], &quad[2], t, tint, 1);
	draw_triangle(&quad[0], &quad[2], &quad[3], t, tint, 1);
	draw_triangle(&quad[2], &quad[1], &quad[0], t, tint, 1);
	draw_triangle(&quad[3], &quad[2], &quad[0], t, tint, 1);
	mode_2d = 0;
	draw_calls++;
}

void
gfx_Begin2D(void)
{
	mode_2d = 1;
}

/*  Прямоугольник рисуется напрямую, без конвейера: он всегда параллелен
 *  осям экрана, и гонять его через матрицы было бы смешно.
 */
void
gfx_Quad(float x, float y, float w, float h, unsigned int tex,
    float u0, float v0, float u1, float v1, const float rgba[4])
{
	struct soft_texture *t;
	int tint;
	int x0;
	int y0;
	int x1;
	int y1;
	int px;
	int py;
	int tc;
	int cr;
	int cg;
	int cb;
	int ca;
	int old;
	fixed u;
	fixed v;
	long at;

	if (canvas == 0)
		return;

	x0 = (int)x;
	y0 = (int)y;
	x1 = (int)(x + w);
	y1 = (int)(y + h);
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > screen_w)
		x1 = screen_w;
	if (y1 > screen_h)
		y1 = screen_h;
	if (x1 <= x0 || y1 <= y0)
		return;

	t = 0;
	if (tex > 0 && tex < SOFT_MAX_TEXTURES && textures[tex].used)
		t = &textures[tex];
	tint = pack_colour(rgba);

	for (py = y0; py < y1; py++) {
		at = (long)py * screen_w + x0;
		for (px = x0; px < x1; px++, at++) {
			ca = (tint >> 24) & 0xFF;
			cr = (tint >> 16) & 0xFF;
			cg = (tint >> 8) & 0xFF;
			cb = tint & 0xFF;

			if (t != 0) {
				u = FLOAT_TO_FIXED(u0 + (u1 - u0) *
				    ((float)(px - (int)x) / w));
				v = FLOAT_TO_FIXED(v0 + (v1 - v0) *
				    ((float)(py - (int)y) / h));
				tc = texel(t, u, v);
				ca = ca * ((tc >> 24) & 0xFF) / 255;
				cr = cr * ((tc >> 16) & 0xFF) / 255;
				cg = cg * ((tc >> 8) & 0xFF) / 255;
				cb = cb * (tc & 0xFF) / 255;
			}
			if (ca < 8)
				continue;

			if (ca < 255) {
				old = (int)canvas[at];
				cr = (cr * ca + ((old >> 16) & 0xFF) *
				    (255 - ca)) / 255;
				cg = (cg * ca + ((old >> 8) & 0xFF) *
				    (255 - ca)) / 255;
				cb = (cb * ca + (old & 0xFF) *
				    (255 - ca)) / 255;
			}
			canvas[at] = (unsigned int)(0xFF000000 |
			    (cr << 16) | (cg << 8) | cb);
		}
	}
	draw_calls++;
}

void
gfx_End2D(void)
{
	mode_2d = 0;
}
