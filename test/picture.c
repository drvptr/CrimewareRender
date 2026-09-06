/*
 *	@(#)picture.c	1.0-вектор
 *
 *  Один кадр программным рендером в файл, без окна и без видеокарты.
 *  Существует ради того, чтобы на fixed-point можно было ПОСМОТРЕТЬ, не
 *  запуская игру и не имея дисплея:
 *
 *	make picture FIXED_BITS=4
 *	make picture FIXED_BITS=8
 *	make picture FIXED_BITS=16
 *
 *  Сцена подобрана так, чтобы вылезли все три эффекта сразу: большой пол
 *  под острым углом (по нему видно, как плывёт текстура без коррекции
 *  перспективы), наклонная стена (видно зернистость краёв) и модель
 *  (видно, как трясётся геометрия).
 */
#include <stdio.h>
#include <stdlib.h>

#include "../core/m3.h"
#include "../core/arena.h"
#include "../core/fixed.h"
#include "../gfx/gfx.h"
#include "../plat/plat.h"
#include "../asset/tga.h"
#include "../asset/obj.h"
#include "../buf/buffer.h"

#define PIC_W 640
#define PIC_H 400

static char pool[32 * 1024 * 1024];
static struct arena mem;

static void *
slurp(const char *path, long *len)
{
	FILE *f;
	long size;
	void *p;

	f = fopen(path, "rb");
	if (f == 0)
		return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	p = arena_Alloc(&mem, size + 1, 8);
	if (p == 0 || fread(p, 1, (unsigned long)size, f) !=
	    (unsigned long)size) {
		fclose(f);
		return 0;
	}
	fclose(f);
	*len = size;
	return p;
}

/*  Пол сеткой из GRID x GRID плиток, а не двумя огромными треугольниками.
 *
 *  Это не украшательство: интерполяция координат текстуры здесь линейна по
 *  экрану, без коррекции перспективы, и на одном треугольнике во весь пол
 *  она размазывает текстуру в полосы. PlayStation дробила пол ровно по этой
 *  причине - чем мельче полигон, тем меньше ошибка внутри него. Дробление
 *  и есть способ бороться с аффинной развёрткой, другого у неё нет.
 */
#define GRID 16

static int
plane(struct gfx_vertex *v, unsigned short *ix, float y, float size,
    float tiles)
{
	int i;
	int j;
	int n;
	int q;
	float step;
	float ustep;

	step = size * 2.0f / (float)GRID;
	ustep = tiles / (float)GRID;
	n = 0;
	for (j = 0; j <= GRID; j++) {
		for (i = 0; i <= GRID; i++) {
			v[n].x = -size + step * (float)i;
			v[n].y = y;
			v[n].z = -size + step * (float)j;
			v[n].nx = 0.0f;
			v[n].ny = 1.0f;
			v[n].nz = 0.0f;
			v[n].u = ustep * (float)i;
			v[n].v = ustep * (float)j;
			n++;
		}
	}

	q = 0;
	for (j = 0; j < GRID; j++) {
		for (i = 0; i < GRID; i++) {
			int a;

			a = j * (GRID + 1) + i;
			ix[q++] = (unsigned short)a;
			ix[q++] = (unsigned short)(a + GRID + 1);
			ix[q++] = (unsigned short)(a + 1);
			ix[q++] = (unsigned short)(a + 1);
			ix[q++] = (unsigned short)(a + GRID + 1);
			ix[q++] = (unsigned short)(a + GRID + 2);
		}
	}
	return q;
}

static void
write_tga(const char *path, const unsigned int *pixels, int w, int h)
{
	unsigned char header[18];
	unsigned char row[4];
	FILE *f;
	long i;
	unsigned int c;

	for (i = 0; i < 18; i++)
		header[i] = 0;
	header[2] = 2;			/* truecolour, без сжатия	*/
	header[12] = (unsigned char)(w & 0xFF);
	header[13] = (unsigned char)((w >> 8) & 0xFF);
	header[14] = (unsigned char)(h & 0xFF);
	header[15] = (unsigned char)((h >> 8) & 0xFF);
	header[16] = 32;
	header[17] = 0x28;		/* альфа 8 бит, начало сверху	*/

	f = fopen(path, "wb");
	if (f == 0) {
		fprintf(stderr, "picture: не могу записать %s\n", path);
		return;
	}
	fwrite(header, 1, 18, f);
	for (i = 0; i < (long)w * h; i++) {
		c = pixels[i];
		row[0] = (unsigned char)(c & 0xFF);		/* B */
		row[1] = (unsigned char)((c >> 8) & 0xFF);	/* G */
		row[2] = (unsigned char)((c >> 16) & 0xFF);	/* R */
		row[3] = 0xFF;
		fwrite(row, 1, 4, f);
	}
	fclose(f);
}

int
main(void)
{
	struct gfx_vertex floor_v[(GRID + 1) * (GRID + 1)];
	unsigned short floor_i[GRID * GRID * 6];
	int floor_n;
	struct obj_mesh model;
	unsigned int floor_mesh;
	unsigned int model_mesh;
	unsigned int tex;
	unsigned long long geo;
	void *bytes;
	void *pixels;
	long len;
	float view[16];
	float proj[16];
	float m[16];
	float spin[16];
	float scale[16];
	float move[16];
	float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	vector eye;
	vector at;
	vector up;
	vector light;
	unsigned int *canvas;
	char name[64];
	long lit;
	long i;

	arena_Init(&mem, pool, (long)sizeof pool);

	if (!plat_Init() || !plat_OpenWindow("picture", PIC_W, PIC_H)) {
		fprintf(stderr, "picture: платформа не поднялась\n");
		return 1;
	}
	if (!gfx_Init()) {
		fprintf(stderr, "picture: рендер не поднялся\n");
		return 1;
	}
	gfx_Viewport(PIC_W, PIC_H);

	bytes = slurp("demo/freebsd.tga", &len);
	if (bytes == 0 || !tga_Probe(bytes, len)) {
		fprintf(stderr, "picture: нет demo/freebsd.tga\n");
		return 1;
	}
	geo = tga_Geo(bytes);
	pixels = arena_Alloc(&mem, tga_CanvasBytes(bytes), 4);
	tga_Decode(pixels, tga_CanvasBytes(bytes), bytes, len);
	tex = gfx_MakeTexture(pixels, geo, 0, 1);

	floor_n = plane(floor_v, floor_i, 0.0f, 40.0f, 12.0f);
	floor_mesh = gfx_MakeMesh(floor_v, (GRID + 1) * (GRID + 1), floor_i,
	    floor_n, 0);

	bytes = slurp("demo/freebsd.obj", &len);
	model_mesh = 0;
	if (bytes != 0 && obj_Parse(&model, bytes, len, &mem) == 0) {
		/*  МАСШТАБ ЗАПЕКАЕТСЯ В ВЕРШИНЫ, а не идёт матрицей.
		 *  m4_scale(0.005) при FIXED_BITS=4 - это нулевая матрица:
		 *  0.005 * 16 = 0.08, округляется в ноль, и модель
		 *  схлопывается в точку. Наименьшее ненулевое число при
		 *  N=4 равно 1/16, всё что мельче не существует.
		 */
		for (i = 0; i < model.nverts; i++) {
			model.verts[i].x *= 0.005f;
			model.verts[i].y *= 0.005f;
			model.verts[i].z *= 0.005f;
		}
		model_mesh = gfx_MakeMesh(model.verts, model.nverts,
		    model.index, model.nindex, 0);
	}

	VEC_SET(light, -0.4f, -0.9f, -0.3f);
	gfx_SetLight(light, 0.35f);
	gfx_SetFog(0.05f, 0.06f, 0.09f, 20.0f, 70.0f);

	/*  Камера почти на уровне пола: под таким углом аффинная
	 *  интерполяция текстуры видна лучше всего.
	 */
	VEC_SET(eye, 0.0f, 1.4f, 9.0f);
	VEC_SET(at, 0.0f, 1.0f, 0.0f);
	VEC_SET(up, 0.0f, 1.0f, 0.0f);
	m4_look_at(view, eye, at, up);
	m4_perspective(proj, 1.1f, (float)PIC_W / (float)PIC_H, 0.1f, 200.0f);

	gfx_BeginFrame(0.05f, 0.06f, 0.09f);
	gfx_SetCamera(view, proj);

	m4_identity(m);
	gfx_DrawMesh(floor_mesh, m, tex, white, 0, -1);

	if (model_mesh != 0) {
		m4_rot_y(spin, 0.6f);
		m4_translate(move, 0.0f, 1.6f, 0.0f);
		m4_mul(m, move, spin);
		gfx_DrawMesh(model_mesh, m, tex, white, 0, -1);
	}
	(void)scale;
	gfx_EndFrame();

	canvas = plat_Framebuffer(&geo);
	lit = 0;
	for (i = 0; i < (long)PIC_W * PIC_H; i++) {
		if ((canvas[i] & 0x00FFFFFF) != 0x000D0F17)
			lit++;
	}

	snprintf(name, sizeof name, "picture-fixed%d.tga", FIXED_BITS);
	write_tga(name, canvas, PIC_W, PIC_H);
	printf("picture: %s, вызовов отрисовки %d, закрашено %ld из %d "
	    "пикселей\n", name, gfx_DrawCalls(), lit, PIC_W * PIC_H);
	return 0;
}
