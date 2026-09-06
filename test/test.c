/*
 *	@(#)test.c	1.0
 *
 *  Everything that can be tested without a screen.  Links against
 *  plat_null.c and gfx_null.c, so it runs over ssh and in a container.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../buf/buffer.h"
#include "../core/m3.h"
#include "../core/arena.h"
#include "../asset/tga.h"
#include "../asset/obj.h"
#include "../asset/wav.h"
#include "../asset/anim.h"
#include "../world/world.h"
#include "../world/coll.h"
#include "../snd/snd.h"

static int checks;
static int failures;

static void
check(int ok, const char *what)
{
	checks++;
	if (!ok) {
		failures++;
		printf("FAIL  %s\n", what);
	}
}

static int
close_to(float a, float b)
{
	float d;

	d = a - b;
	if (d < 0.0f)
		d = -d;
	return d < 0.001f;
}

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

static void
test_math(void)
{
	vector a;
	vector b;
	vector c;
	float m[16];
	float n[16];
	float p[16];
	vector r;

	VEC_SET(a, 1.0f, 0.0f, 0.0f);
	VEC_SET(b, 0.0f, 1.0f, 0.0f);
	check(close_to(vec_dot(a, b), 0.0f), "dot of perpendicular is zero");
	vec_cross(a, b, c);
	check(close_to(c[Z], 1.0f), "cross of x and y is z");
	VEC_SET(c, 3.0f, 4.0f, 0.0f);
	check(close_to(vec_abs(c), 5.0f), "length 3 4 5");

	/*  Проверка того, что vec_cross терпит выходной аргумент, равный
	 *  входному. На master это было невозможно по построению.
	 */
	VEC_SET(a, 1.0f, 0.0f, 0.0f);
	VEC_SET(b, 0.0f, 1.0f, 0.0f);
	vec_cross(a, b, a);
	check(close_to(a[Z], 1.0f) && close_to(a[X], 0.0f),
	    "vec_cross выдерживает c == a");

	m4_translate(m, 1.0f, 2.0f, 3.0f);
	VEC_ZERO(c);
	m4_mul_point(m, c, r);
	check(close_to(r[X], 1.0f) && close_to(r[Y], 2.0f) &&
	    close_to(r[Z], 3.0f), "translation moves the origin");

	m4_rot_y(m, 1.5707963f);
	VEC_SET(c, 1.0f, 0.0f, 0.0f);
	m4_mul_point(m, c, r);
	check(close_to(r[Z], -1.0f), "90 degrees about y sends x to -z");

	m4_perspective(m, 1.0f, 1.5f, 0.1f, 100.0f);
	check(m4_invert(n, m) == 1, "a projection matrix is invertible");
	m4_mul(p, m, n);
	check(close_to(p[0], 1.0f) && close_to(p[5], 1.0f) &&
	    close_to(p[15], 1.0f), "matrix times its inverse is identity");

	m4_identity(m);
	m4_mul(n, m, m);
	check(close_to(n[0], 1.0f) && close_to(n[1], 0.0f),
	    "identity squared is identity");
}

static void
test_frustum(void)
{
	struct frustum f;
	float view[16];
	float proj[16];
	float viewproj[16];
	float near_min[3] = { -1.0f, -1.0f, -6.0f };
	float near_max[3] = { 1.0f, 1.0f, -4.0f };
	float behind_min[3] = { -1.0f, -1.0f, 4.0f };
	float behind_max[3] = { 1.0f, 1.0f, 6.0f };
	float far_min[3] = { -1.0f, -1.0f, -500.0f };
	float far_max[3] = { 1.0f, 1.0f, -400.0f };

	m4_identity(view);
	m4_perspective(proj, 1.2f, 1.333f, 0.1f, 100.0f);
	m4_mul(viewproj, proj, view);
	fr_from_matrix(&f, viewproj);

	check(fr_test_aabb(&f, near_min, near_max) == 1,
	    "a box in front is visible");
	check(fr_test_aabb(&f, behind_min, behind_max) == 0,
	    "a box behind the camera is not");
	check(fr_test_aabb(&f, far_min, far_max) == 0,
	    "a box past the far plane is not");
	{
		vector centre;

		VEC_SET(centre, 0.0f, 0.0f, -5.0f);
		check(fr_test_sphere(&f, centre, 1.0f) == 1,
		    "a sphere in front is visible");
	}
}

static void
test_arena(void)
{
	struct arena a;
	char block[256];
	void *p;
	void *q;
	long mark;

	arena_Init(&a, block, (long)sizeof block);
	p = arena_Alloc(&a, 10, 4);
	check(p != 0, "a small allocation succeeds");
	check(((char *)p)[0] == 0, "arena memory comes back zeroed");

	q = arena_Alloc(&a, 10, 16);
	check(((char *)q - block) % 16 == 0, "alignment is honoured");

	mark = arena_Mark(&a);
	arena_Alloc(&a, 64, 1);
	arena_Reset(&a, mark);
	check(arena_Mark(&a) == mark, "reset returns to the mark");

	check(arena_Alloc(&a, 1000000, 1) == 0, "a full arena returns 0");
	check(a.failed == 1, "and remembers that it failed");
}

static void
test_collision(void)
{
	struct aabb a;
	struct aabb b;
	struct aabb wall;
	vector push;
	vector moved;
	vector centre;
	vector half;
	vector origin;
	vector dir;
	vector t0;
	vector t1;
	vector t2;
	float t;
	int ground;

	VEC_ZERO(centre);
	VEC_SET(half, 1.0f, 1.0f, 1.0f);
	coll_MakeAabb(centre, half, &a);
	VEC_SET(centre, 1.5f, 0.0f, 0.0f);
	coll_MakeAabb(centre, half, &b);
	check(coll_AabbAabb(&a, &b) == 1, "overlapping boxes overlap");

	VEC_SET(centre, 5.0f, 0.0f, 0.0f);
	coll_MakeAabb(centre, half, &b);
	check(coll_AabbAabb(&a, &b) == 0, "distant boxes do not");

	VEC_SET(centre, 0.5f, 0.5f, 0.5f);
	check(coll_PointAabb(centre, &a) == 1, "a point inside is inside");
	VEC_SET(centre, 9.0f, 0.0f, 0.0f);
	check(coll_PointAabb(centre, &a) == 0, "a point outside is outside");

	VEC_SET(centre, 1.5f, 0.0f, 0.0f);
	check(coll_SphereAabb(centre, 1.0f, &a, push) == 1,
	    "a sphere touching the box is reported");
	check(push[X] > 0.0f, "and is pushed out along +x");

	VEC_SET(origin, -5.0f, 0.0f, 0.0f);
	VEC_SET(dir, 1.0f, 0.0f, 0.0f);
	check(coll_RayAabb(origin, dir, &a, &t) == 1,
	    "a ray at the box hits it");
	check(close_to(t, 4.0f), "at the near face");

	VEC_SET(origin, -5.0f, 9.0f, 0.0f);
	check(coll_RayAabb(origin, dir, &a, &t) == 0,
	    "a ray over the box misses");

	VEC_SET(origin, 0.0f, 0.0f, -2.0f);
	VEC_SET(dir, 0.0f, 0.0f, 1.0f);
	VEC_SET(t0, -1.0f, -1.0f, 0.0f);
	VEC_SET(t1, 1.0f, -1.0f, 0.0f);
	VEC_SET(t2, 0.0f, 1.0f, 0.0f);
	check(coll_RayTri(origin, dir, t0, t1, t2, &t) == 1,
	    "a ray hits the triangle");
	check(close_to(t, 2.0f), "at the right distance");

	/*  Walk into a wall: the x part of the motion is eaten, the z part
	 *  survives.  This is the one behaviour a player will notice.
	 */
	VEC_SET(centre, 2.0f, 0.0f, 0.0f);
	VEC_SET(half, 0.5f, 2.0f, 4.0f);
	coll_MakeAabb(centre, half, &wall);
	VEC_ZERO(centre);
	VEC_SET(half, 0.5f, 1.0f, 0.5f);
	coll_MakeAabb(centre, half, &a);

	VEC_SET(dir, 5.0f, 0.0f, 1.0f);
	coll_MoveAabb(&a, dir, &wall, 1, &ground, moved);
	check(moved[X] < 1.1f, "movement into the wall is stopped");
	check(moved[Z] > 0.5f, "movement along it is not");

	VEC_SET(dir, 0.0f, -5.0f, 0.0f);
	coll_MoveAabb(&a, dir, &wall, 1, &ground, moved);
	check(close_to(moved[Y], -5.0f), "free fall is not blocked sideways");
}

static void
test_world(void)
{
	static struct world w;
	float view[16];
	float proj[16];
	float viewproj[16];
	struct aabb solids[8];
	struct aabb tmp;
	vector centre;
	vector half;
	vector eye;
	vector a0;
	vector a1;
	vector a2;
	vector a3;
	int list[16];
	int n;
	int here;
	int there;

	wld_Clear(&w);
	VEC_SET(centre, 0.0f, 2.0f, 0.0f);
	VEC_SET(half, 8.0f, 2.0f, 8.0f);
	coll_MakeAabb(centre, half, &tmp);
	here = wld_AddSector(&w, &tmp, 1, 1);
	VEC_SET(centre, 0.0f, 1.0f, 0.0f);
	VEC_SET(half, 1.0f, 1.0f, 1.0f);
	coll_MakeAabb(centre, half, &tmp);
	wld_AddSolid(&w, here, &tmp);

	VEC_SET(centre, 0.0f, 2.0f, 16.0f);
	VEC_SET(half, 8.0f, 2.0f, 8.0f);
	coll_MakeAabb(centre, half, &tmp);
	there = wld_AddSector(&w, &tmp, 2, 1);
	VEC_SET(centre, 0.0f, 1.0f, 16.0f);
	VEC_SET(half, 1.0f, 1.0f, 1.0f);
	coll_MakeAabb(centre, half, &tmp);
	wld_AddSolid(&w, there, &tmp);

	VEC_SET(a0, -2.0f, 0.0f, 8.0f);
	VEC_SET(a1, 2.0f, 0.0f, 8.0f);
	VEC_SET(a2, 2.0f, 3.0f, 8.0f);
	VEC_SET(a3, -2.0f, 3.0f, 8.0f);
	wld_AddPortal(&w, here, there, a0, a1, a2, a3);

	VEC_SET(centre, 0.0f, 1.0f, 0.0f);
	check(wld_SectorAt(&w, centre) == here, "a point finds its sector");
	VEC_SET(centre, 0.0f, 1.0f, 16.0f);
	check(wld_SectorAt(&w, centre) == there, "and the other one");
	VEC_SET(centre, 100.0f, 0.0f, 0.0f);
	check(wld_SectorAt(&w, centre) == -1,
	    "a point outside every sector is nowhere");

	/*  Looking towards the doorway: both sectors are drawn.  */
	VEC_SET(eye, 0.0f, 1.7f, 0.0f);
	m4_fps_view(view, eye, 3.14159f, 0.0f);
	m4_perspective(proj, 1.2f, 1.333f, 0.1f, 100.0f);
	m4_mul(viewproj, proj, view);
	n = wld_Visible(&w, here, viewproj, list, 16);
	check(n == 2, "looking at the portal shows both sectors");

	/*  Turned around: the portal is behind us, so the far room is not
	 *  drawn at all.  This is the entire point of the portal graph.
	 */
	m4_fps_view(view, eye, 0.0f, 0.0f);
	m4_mul(viewproj, proj, view);
	n = wld_Visible(&w, here, viewproj, list, 16);
	check(n == 1, "turning away hides the far sector");

	n = wld_SolidsNear(&w, list, n, solids, 8);
	check(n == 1, "only the near sector's solids come back");
}

/*  Камера и тело - разные точки, и движок это обязан выдерживать.
 *  Проверка стоит здесь потому, что в первой версии демки они были одной
 *  переменной, и никакой тест этого не ловил.
 */
static void
test_camera_is_not_the_body(void)
{
	static struct world w;
	struct aabb body_solids[8];
	struct aabb cam_solids[8];
	struct aabb tmp;
	float view[16];
	float proj[16];
	float viewproj[16];
	vector centre;
	vector half;
	vector a0;
	vector a1;
	vector a2;
	vector a3;
	int list[16];
	int body_sector;
	int cam_sector;
	int here;
	int there;
	vector body_at;
	vector cam_at;

	wld_Clear(&w);
	VEC_SET(centre, 0.0f, 2.0f, 0.0f);
	VEC_SET(half, 8.0f, 2.0f, 8.0f);
	coll_MakeAabb(centre, half, &tmp);
	here = wld_AddSector(&w, &tmp, 1, 1);
	VEC_SET(centre, 3.0f, 1.0f, 0.0f);
	VEC_SET(half, 1.0f, 1.0f, 1.0f);
	coll_MakeAabb(centre, half, &tmp);
	wld_AddSolid(&w, here, &tmp);

	VEC_SET(centre, 0.0f, 2.0f, 16.0f);
	VEC_SET(half, 8.0f, 2.0f, 8.0f);
	coll_MakeAabb(centre, half, &tmp);
	there = wld_AddSector(&w, &tmp, 2, 1);
	VEC_SET(centre, 0.0f, 1.0f, 16.0f);
	VEC_SET(half, 1.0f, 1.0f, 1.0f);
	coll_MakeAabb(centre, half, &tmp);
	wld_AddSolid(&w, there, &tmp);
	VEC_SET(centre, 5.0f, 1.0f, 20.0f);
	coll_MakeAabb(centre, half, &tmp);
	wld_AddSolid(&w, there, &tmp);

	VEC_SET(a0, -2.0f, 0.0f, 8.0f);
	VEC_SET(a1, 2.0f, 0.0f, 8.0f);
	VEC_SET(a2, 2.0f, 3.0f, 8.0f);
	VEC_SET(a3, -2.0f, 3.0f, 8.0f);
	wld_AddPortal(&w, here, there, a0, a1, a2, a3);
	wld_AddPortal(&w, there, here, a1, a0, a3, a2);

	/*  Персонаж только прошёл проём, камера от третьего лица ещё в
	 *  предыдущей комнате. Обычное положение дел, не крайний случай.
	 */
	VEC_SET(body_at, 0.0f, 1.0f, 10.0f);
	VEC_SET(cam_at, 0.0f, 2.0f, 5.0f);

	body_sector = wld_SectorAt(&w, body_at);
	cam_sector = wld_SectorAt(&w, cam_at);
	check(body_sector == there, "тело в дальней комнате");
	check(cam_sector == here, "камера ещё в ближней");
	check(body_sector != cam_sector, "и это разные секторы");

	check(wld_SolidsNear(&w, &body_sector, 1, body_solids, 8) == 2,
	    "столкновения берутся вокруг тела");
	check(wld_SolidsNear(&w, &cam_sector, 1, cam_solids, 8) == 1,
	    "а не вокруг камеры");

	/*  Видимость, наоборот, считается от камеры: рисуется то, что
	 *  видно ей, а не то, что видно персонажу.
	 */
	m4_fps_view(view, cam_at, 3.14159f, 0.0f);
	m4_perspective(proj, 1.2f, 1.333f, 0.1f, 100.0f);
	m4_mul(viewproj, proj, view);
	check(wld_Visible(&w, cam_sector, viewproj, list, 16) == 2,
	    "из камеры видны обе комнаты");

	m4_fps_view(view, cam_at, 0.0f, 0.0f);
	m4_mul(viewproj, proj, view);
	check(wld_Visible(&w, cam_sector, viewproj, list, 16) == 1,
	    "камера отвернулась - дальней комнаты нет, где бы ни стояло тело");
}

static void
test_tga(void)
{
	void *bytes;
	long len;
	long mark;
	void *pixels;
	unsigned long long geo;
	unsigned char *p;

	mark = arena_Mark(&mem);
	bytes = slurp("demo/freebsd.tga", &len);
	if (bytes == 0) {
		printf("SKIP  demo/freebsd.tga not found\n");
		arena_Reset(&mem, mark);
		return;
	}

	check(tga_Probe(bytes, len) == 1, "the tga is one we can read");
	geo = tga_Geo(bytes);
	check(bufGeomCheck(geo) == 1, "its geometry is valid");
	check(BUF_UNIT(geo) == 4, "decoded pixels are 4 bytes");
	check(BUF_COLS(geo) == tga_Width(bytes), "width matches the header");

	pixels = arena_Alloc(&mem, tga_CanvasBytes(bytes), 4);
	check(tga_Decode(pixels, tga_CanvasBytes(bytes), bytes, len) == 0,
	    "it decodes");
	check(tga_Decode(pixels, 4, bytes, len) == -1,
	    "and refuses a destination that is too small");

	/*  Every alpha byte must have been written: a decoder that walks
	 *  the destination wrongly leaves zeroes behind.
	 */
	p = (unsigned char *)pixels;
	check(p[3] == 255 && p[tga_CanvasBytes(bytes) - 1] == 255,
	    "alpha is set at both ends of the canvas");

	arena_Reset(&mem, mark);
}

static void
test_obj(void)
{
	void *bytes;
	long len;
	long mark;
	struct obj_mesh m;
	int i;
	float length;

	mark = arena_Mark(&mem);
	bytes = slurp("demo/freebsd.obj", &len);
	if (bytes == 0) {
		printf("SKIP  demo/freebsd.obj not found\n");
		arena_Reset(&mem, mark);
		return;
	}

	check(obj_Parse(&m, bytes, len, &mem) == 0, "the obj parses");
	check(m.nverts > 0 && m.nindex > 0, "it has vertices and indices");
	check(m.nindex % 3 == 0, "indices are whole triangles");
	check(m.ngroups >= 1, "there is at least one group");

	for (i = 0; i < m.nindex; i++) {
		if (m.index[i] >= m.nverts)
			break;
	}
	check(i == m.nindex, "every index is in range");

	length = sqrtf(m.verts[0].nx * m.verts[0].nx +
	    m.verts[0].ny * m.verts[0].ny + m.verts[0].nz * m.verts[0].nz);
	check(close_to(length, 1.0f), "normals come out normalized");
	check(m.max[0] > m.min[0], "the bounding box is not empty");

	printf("      obj: %d verts, %d indices, %d groups, %ld KB\n",
	    m.nverts, m.nindex, m.ngroups,
	    (long)(m.nverts * (int)sizeof(struct gfx_vertex) +
	    m.nindex * 2) / 1024);

	arena_Reset(&mem, mark);
}

static void
test_obj_numbers(void)
{
	static const char text[] =
	    "v -1.5 0 2.5e1\n"
	    "v 1.5 0 -25\n"
	    "v 0 3.25 0\n"
	    "vt 0 0\n"
	    "vt 1 0\n"
	    "vt 0.5 1\n"
	    "usemtl stone\n"
	    "f 1/1 2/2 3/3\n"
	    "f -3/-3 -2/-2 -1/-1\n";
	struct obj_mesh m;
	long mark;

	mark = arena_Mark(&mem);
	check(obj_Parse(&m, text, (long)sizeof text - 1, &mem) == 0,
	    "a small obj parses");
	check(m.nindex == 6, "two triangles");
	check(m.nverts == 3, "duplicate corners are folded together");
	check(close_to(m.verts[0].x, -1.5f), "fractions survive");
	check(close_to(m.verts[0].z, 25.0f), "exponents survive");
	check(close_to(m.verts[0].v, 1.0f), "v is flipped for the GPU");
	check(m.groups[0].name[0] == 's', "usemtl names the group");
	check(m.source[0] == 0 && m.source[2] == 2,
	    "every vertex remembers its v line");

	{
		static unsigned char file[24 + 3 * 3 * 4];
		struct anim a;
		float *frames;

		file[0] = 'V'; file[1] = 'A'; file[2] = 'N'; file[3] = '1';
		file[4] = 3;	/* nverts, one per v line	*/
		file[8] = 1;	/* one frame			*/
		file[12] = 24;	/* fps				*/
		frames = (float *)(void *)(file + 24);
		frames[7] = 99.0f;	/* vertex 2, y			*/

		check(anim_Parse(&a, file, (long)sizeof file) == 0,
		    "a one frame animation parses");
		anim_Sample(&a, 0.0f, 0, m.verts, m.nverts, m.source);
		check(close_to(m.verts[2].y, 99.0f),
		    "sampling through source hits the right vertex");
	}
	arena_Reset(&mem, mark);
}

static void
test_anim(void)
{
	static unsigned char file[24 + 2 * 2 * 3 * 4];
	struct anim a;
	struct gfx_vertex v[2];
	float *frames;

	file[0] = 'V';
	file[1] = 'A';
	file[2] = 'N';
	file[3] = '1';
	file[4] = 2;	/* nverts  */
	file[8] = 2;	/* nframes */
	file[12] = 10;	/* fps	   */

	frames = (float *)(void *)(file + 24);
	frames[0] = 0.0f;
	frames[1] = 0.0f;
	frames[2] = 0.0f;
	frames[3] = 1.0f;
	frames[4] = 0.0f;
	frames[5] = 0.0f;
	frames[6] = 0.0f;
	frames[7] = 10.0f;
	frames[8] = 0.0f;
	frames[9] = 1.0f;
	frames[10] = 10.0f;
	frames[11] = 0.0f;

	check(anim_Parse(&a, file, (long)sizeof file) == 0,
	    "the animation header parses");
	check(a.nverts == 2 && a.nframes == 2, "counts are right");
	check(close_to(anim_Length(&a), 0.2f), "length is frames over fps");

	anim_Sample(&a, 0.0f, 1, v, 2, 0);
	check(close_to(v[0].y, 0.0f), "frame 0 is the first frame");

	anim_Sample(&a, 0.05f, 1, v, 2, 0);
	check(close_to(v[0].y, 5.0f), "halfway between frames is halfway");

	anim_Sample(&a, 0.1f, 1, v, 2, 0);
	check(close_to(v[0].y, 10.0f), "frame 1 is the second frame");
}

static void
test_wav_and_mixer(void)
{
	static unsigned char file[44 + 8];
	static short out[64 * 2];
	struct wav w;
	short *samples;
	vector ear;
	vector front;
	vector far_right;
	int i;
	int loud;

	/*  A four frame, 16 bit, mono, 8 kHz file, written by hand.  */
	file[0] = 'R'; file[1] = 'I'; file[2] = 'F'; file[3] = 'F';
	file[4] = 44;
	file[8] = 'W'; file[9] = 'A'; file[10] = 'V'; file[11] = 'E';
	file[12] = 'f'; file[13] = 'm'; file[14] = 't'; file[15] = ' ';
	file[16] = 16;
	file[20] = 1;			/* PCM		*/
	file[22] = 1;			/* mono		*/
	file[24] = 0x40; file[25] = 0x1F;	/* 8000 Hz	*/
	file[34] = 16;			/* bits		*/
	file[36] = 'd'; file[37] = 'a'; file[38] = 't'; file[39] = 'a';
	file[40] = 8;

	samples = (short *)(void *)(file + 44);
	samples[0] = 10000;
	samples[1] = -10000;
	samples[2] = 10000;
	samples[3] = -10000;

	check(wav_Parse(&w, file, (long)sizeof file, &mem) == 0,
	    "the wav parses");
	check(w.rate == 8000 && w.channels == 1, "rate and channels");
	check(w.frames == 4, "frame count");
	check(w.pcm == samples, "16 bit samples are used in place");

	snd_Init(44100);
	VEC_ZERO(ear);
	VEC_SET(front, 0.0f, 0.0f, -1.0f);
	snd_Listener(ear, front);
	snd_SetRange(2.0f, 50.0f);

	check(snd_Play2D(&w, 1.0f, 1) >= 0, "a voice starts");
	check(snd_Busy() == 1, "and is counted");
	snd_Mix(out, 64);

	loud = 0;
	for (i = 0; i < 64 * 2; i++) {
		if (out[i] != 0)
			loud = 1;
	}
	check(loud == 1, "the mixer wrote something");

	snd_StopAll();
	snd_Mix(out, 64);
	loud = 0;
	for (i = 0; i < 64 * 2; i++) {
		if (out[i] != 0)
			loud = 1;
	}
	check(loud == 0, "and silence when nothing plays");

	/*  Far away on the right: quieter, and louder in the right ear.  */
	VEC_SET(far_right, 30.0f, 0.0f, 0.0f);
	snd_Play(&w, far_right, 1.0f, 1);
	snd_Mix(out, 64);
	check(out[1] != 0 || out[3] != 0, "a distant source is audible");

	snd_StopAll();
}

int
main(void)
{
	arena_Init(&mem, pool, (long)sizeof pool);

	test_math();
	test_frustum();
	test_arena();
	test_collision();
	test_world();
	test_camera_is_not_the_body();
	test_tga();
	test_obj();
	test_obj_numbers();
	test_anim();
	test_wav_and_mixer();

	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
