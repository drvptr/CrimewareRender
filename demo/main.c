/*
 *	@(#)main.c	2.0
 *
 *  Игра, а не список возможностей движка. Две комнаты, соединённые
 *  проёмом, три подопечных, коробки, сквозь которые нельзя пройти,
 *  гравитация, три режима камеры.
 *
 *  ЧТО ЗДЕСЬ СПЕЦИАЛЬНО РАЗДЕЛЕНО. В первой версии этого файла позиция
 *  камеры и позиция игрока были одной переменной, и это была ошибка -
 *  не потому, что так нельзя, а потому, что демка это шаблон, который
 *  копируют, и вместе с ней копировалось бы предположение "игра от
 *  первого лица, персонаж один".
 *
 *  Поэтому здесь три сущности, и ни одна не знает про две другие:
 *
 *	struct unit	 кто ходит по уровню. Их несколько.
 *	struct camera	 откуда смотрим. Живёт в demo/camera.c.
 *	g->controlled	 кем сейчас управляют. Просто индекс.
 *
 *  Движок ни одну из них не видит: он получает две матрицы, номер
 *  сектора и список коробок.
 *
 *  Управление:
 *	WASD		идти (или вести камеру в режиме сверху)
 *	мышь		смотреть
 *	пробел		прыжок (или приказ идти в точку в режиме сверху)
 *	1 2 3		первое лицо / третье лицо / вид сверху
 *	TAB		переключить подопечного
 *	G		отпустить курсор
 *	ESC		выход
 *
 *  Файлы читает этот файл, а не движок: смотри slurp() ниже.
 */
#include <stdio.h>
#include <math.h>

#include "../core/app.h"
#include "../core/arena.h"
#include "../core/m3.h"
#include "../gfx/gfx.h"
#include "../asset/tga.h"
#include "../asset/obj.h"
#include "../world/world.h"
#include "../snd/snd.h"
#include "../ui/gui.h"
#include "camera.h"

static char pool[48 * 1024 * 1024];

#define UNITS 3

struct unit {
	struct vec3 pos;	/* центр коробки столкновений	*/
	struct vec3 velocity;
	float yaw;		/* куда развёрнута модель	*/
	int on_floor;
	int has_order;		/* идёт в точку, а не по кнопкам */
	struct vec3 order;
	float colour[4];
};

struct game {
	struct arena mem;
	struct world world;

	unsigned int room_mesh;
	unsigned int wall_tex;

	struct obj_mesh model;
	unsigned int model_mesh;

	struct unit unit[UNITS];
	int controlled;

	struct camera cam;
	struct vec3 map_focus;	/* точка, над которой висит вид сверху */
	int mouse_look;
};

static void *
slurp(struct arena *a, const char *path, long *len_out)
{
	FILE *f;
	long size;
	void *mem;

	f = fopen(path, "rb");
	if (f == 0) {
		fprintf(stderr, "demo: cannot open %s\n", path);
		return 0;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	mem = arena_Alloc(a, size + 1, 8);
	if (mem == 0) {
		fclose(f);
		return 0;
	}
	if (fread(mem, 1, (unsigned long)size, f) != (unsigned long)size) {
		fclose(f);
		return 0;
	}
	fclose(f);
	if (len_out != 0)
		*len_out = size;
	return mem;
}

/* ------------------------------------------------------------ geometry */

struct builder {
	struct gfx_vertex *v;
	unsigned short *i;
	int nv;
	int ni;
};

static void
quad(struct builder *b, struct vec3 a, struct vec3 c, struct vec3 d,
    struct vec3 e, float tiles)
{
	struct vec3 n;
	struct vec3 corner[4];
	float u[4];
	float w[4];
	int k;

	corner[0] = a;
	corner[1] = c;
	corner[2] = d;
	corner[3] = e;
	u[0] = 0.0f;      w[0] = 0.0f;
	u[1] = tiles;     w[1] = 0.0f;
	u[2] = tiles;     w[2] = tiles;
	u[3] = 0.0f;      w[3] = tiles;

	n = v3_norm(v3_cross(v3_sub(c, a), v3_sub(e, a)));

	for (k = 0; k < 4; k++) {
		b->v[b->nv + k].x = corner[k].x;
		b->v[b->nv + k].y = corner[k].y;
		b->v[b->nv + k].z = corner[k].z;
		b->v[b->nv + k].nx = n.x;
		b->v[b->nv + k].ny = n.y;
		b->v[b->nv + k].nz = n.z;
		b->v[b->nv + k].u = u[k];
		b->v[b->nv + k].v = w[k];
	}

	b->i[b->ni + 0] = (unsigned short)(b->nv + 0);
	b->i[b->ni + 1] = (unsigned short)(b->nv + 1);
	b->i[b->ni + 2] = (unsigned short)(b->nv + 2);
	b->i[b->ni + 3] = (unsigned short)(b->nv + 0);
	b->i[b->ni + 4] = (unsigned short)(b->nv + 2);
	b->i[b->ni + 5] = (unsigned short)(b->nv + 3);
	b->nv += 4;
	b->ni += 6;
}

/*  Комната - это коробка, видимая изнутри, поэтому каждый четырёхугольник
 *  обходится в обратную сторону. Одна стена пропускается там, где дверь.
 */
static void
room(struct builder *b, float x0, float z0, float x1, float z1, float h,
    int door_side)
{
	quad(b, v3(x0, 0.0f, z1), v3(x1, 0.0f, z1), v3(x1, 0.0f, z0),
	    v3(x0, 0.0f, z0), 4.0f);
	quad(b, v3(x0, h, z0), v3(x1, h, z0), v3(x1, h, z1),
	    v3(x0, h, z1), 4.0f);

	if (door_side != 0)
		quad(b, v3(x0, 0.0f, z0), v3(x1, 0.0f, z0), v3(x1, h, z0),
		    v3(x0, h, z0), 2.0f);
	if (door_side != 1)
		quad(b, v3(x1, 0.0f, z1), v3(x0, 0.0f, z1), v3(x0, h, z1),
		    v3(x1, h, z1), 2.0f);
	if (door_side != 2)
		quad(b, v3(x0, 0.0f, z1), v3(x0, 0.0f, z0), v3(x0, h, z0),
		    v3(x0, h, z1), 2.0f);
	if (door_side != 3)
		quad(b, v3(x1, 0.0f, z0), v3(x1, 0.0f, z1), v3(x1, h, z1),
		    v3(x1, h, z0), 2.0f);
}

static struct aabb
box(float x0, float y0, float z0, float x1, float y1, float z1)
{
	struct aabb b;

	b.min[0] = x0;
	b.min[1] = y0;
	b.min[2] = z0;
	b.max[0] = x1;
	b.max[1] = y1;
	b.max[2] = z1;
	return b;
}

/*  Коробки рядом с точкой. Сектор считается для КАЖДОГО, кто двигается,
 *  и отдельно для камеры: в третьем лице она сплошь и рядом оказывается
 *  в другой комнате, чем персонаж, и это нормально.
 */
static int
solids_around(struct game *g, struct vec3 at, struct aabb *out, int max)
{
	int here;
	int all[16];
	int n;
	int i;

	here = wld_SectorAt(&g->world, at);
	if (here >= 0)
		return wld_SolidsNear(&g->world, &here, 1, out, max);

	n = g->world.nsectors < 16 ? g->world.nsectors : 16;
	for (i = 0; i < n; i++)
		all[i] = i;
	return wld_SolidsNear(&g->world, all, n, out, max);
}

/* ---------------------------------------------------------------- game */

static int
game_init(void *user)
{
	struct game *g;
	void *bytes;
	long len;
	void *pixels;
	unsigned long long geo;
	struct builder b;
	long mark;
	int here;
	int there;
	int i;

	g = (struct game *)user;
	arena_Init(&g->mem, pool, (long)sizeof pool);

	mark = arena_Mark(&g->mem);
	bytes = slurp(&g->mem, "demo/freebsd.tga", &len);
	if (bytes != 0 && tga_Probe(bytes, len)) {
		geo = tga_Geo(bytes);
		pixels = arena_Alloc(&g->mem, tga_CanvasBytes(bytes), 4);
		if (pixels != 0 &&
		    tga_Decode(pixels, tga_CanvasBytes(bytes), bytes,
		    len) == 0)
			g->wall_tex = gfx_MakeTexture(pixels, geo, 1, 1);
	}
	arena_Reset(&g->mem, mark);

	bytes = slurp(&g->mem, "demo/freebsd.obj", &len);
	if (bytes != 0 && obj_Parse(&g->model, bytes, len, &g->mem) == 0) {
		g->model_mesh = gfx_MakeMesh(g->model.verts, g->model.nverts,
		    g->model.index, g->model.nindex, 0);
		printf("demo: model %d vertices, %d indices, %d groups\n",
		    g->model.nverts, g->model.nindex, g->model.ngroups);
	}

	b.v = arena_Alloc(&g->mem, 256 * (long)sizeof(struct gfx_vertex), 4);
	b.i = arena_Alloc(&g->mem, 512 * (long)sizeof(unsigned short), 2);
	b.nv = 0;
	b.ni = 0;
	room(&b, -8.0f, -8.0f, 8.0f, 8.0f, 4.0f, 1);
	room(&b, -8.0f, 8.0f, 8.0f, 24.0f, 4.0f, 0);
	g->room_mesh = gfx_MakeMesh(b.v, b.nv, b.i, b.ni, 0);

	wld_Clear(&g->world);
	here = wld_AddSector(&g->world,
	    box(-8.0f, 0.0f, -8.0f, 8.0f, 4.0f, 8.0f), g->room_mesh,
	    g->wall_tex);
	wld_AddSolid(&g->world, here, box(-1.5f, 0.0f, -1.5f, 1.5f, 1.0f,
	    1.5f));
	wld_AddSolid(&g->world, here, box(4.0f, 0.0f, 2.0f, 6.0f, 2.0f,
	    4.0f));

	there = wld_AddSector(&g->world,
	    box(-8.0f, 0.0f, 8.0f, 8.0f, 4.0f, 24.0f), g->room_mesh,
	    g->wall_tex);
	wld_AddSolid(&g->world, there, box(-6.0f, 0.0f, 18.0f, -2.0f, 3.0f,
	    22.0f));

	wld_AddPortal(&g->world, here, there, v3(-2.0f, 0.0f, 8.0f),
	    v3(2.0f, 0.0f, 8.0f), v3(2.0f, 3.0f, 8.0f),
	    v3(-2.0f, 3.0f, 8.0f));
	wld_AddPortal(&g->world, there, here, v3(2.0f, 0.0f, 8.0f),
	    v3(-2.0f, 0.0f, 8.0f), v3(-2.0f, 3.0f, 8.0f),
	    v3(2.0f, 3.0f, 8.0f));

	for (i = 0; i < UNITS; i++) {
		g->unit[i].pos = v3(-4.0f + (float)i * 4.0f, 0.9f, 4.0f);
		g->unit[i].velocity = v3(0.0f, 0.0f, 0.0f);
		g->unit[i].yaw = 0.0f;
		g->unit[i].has_order = 0;
		g->unit[i].colour[3] = 1.0f;
	}
	g->unit[0].colour[0] = 1.0f;
	g->unit[0].colour[1] = 0.85f;
	g->unit[0].colour[2] = 0.7f;
	g->unit[1].colour[0] = 0.7f;
	g->unit[1].colour[1] = 1.0f;
	g->unit[1].colour[2] = 0.8f;
	g->unit[2].colour[0] = 0.8f;
	g->unit[2].colour[1] = 0.85f;
	g->unit[2].colour[2] = 1.0f;
	g->controlled = 0;

	cam_Init(&g->cam, CAM_FIRST);
	g->map_focus = v3(0.0f, 0.0f, 4.0f);
	g->mouse_look = 1;
	plat_GrabMouse(1);

	gfx_SetLight(v3(-0.5f, -1.0f, -0.3f), 0.35f);
	gfx_SetFog(0.05f, 0.06f, 0.08f, 12.0f, 40.0f);

	printf("demo: arena %ld of %ld bytes used\n", g->mem.used,
	    g->mem.size);
	printf("demo: WASD move, mouse look, space jump, 1/2/3 camera, "
	    "TAB next unit, G release pointer, ESC quit\n");
	return 0;
}

/*  Один шаг одного подопечного. Ему всё равно, управляют им с клавиатуры,
 *  ведёт ли его приказ или он вообще стоит: движение одинаковое.
 */
static void
unit_step(struct game *g, struct unit *u, struct vec3 wish, float dt)
{
	struct aabb body;
	struct aabb near_solids[64];
	int nsolids;
	struct vec3 to;
	float distance;

	if (u->has_order) {
		to = v3_sub(u->order, u->pos);
		to.y = 0.0f;
		distance = v3_len(to);
		if (distance < 0.4f)
			u->has_order = 0;
		else
			wish = v3_scale(v3_scale(to, 1.0f / distance), 3.0f);
	}

	if (v3_len(wish) > 0.01f)
		u->yaw = atan2f(wish.x, -wish.z);

	u->velocity.x = wish.x;
	u->velocity.z = wish.z;
	u->velocity.y -= 18.0f * dt;

	nsolids = solids_around(g, u->pos, near_solids, 64);
	body = coll_MakeAabb(u->pos, v3(0.35f, 0.9f, 0.35f));
	u->pos = v3_add(u->pos, coll_MoveAabb(body,
	    v3_scale(u->velocity, dt), near_solids, nsolids, &u->on_floor));

	/*  У комнат нет солида под полом, поэтому пол держится вручную.
	 *  В уровне из .obj пол был бы такой же коробкой, как всё прочее.
	 */
	if (u->pos.y < 0.9f) {
		u->pos.y = 0.9f;
		u->velocity.y = 0.0f;
		u->on_floor = 1;
	}
}

static void
game_step(void *user, float dt)
{
	struct game *g;
	const struct plat_input *in;
	struct vec3 forward;
	struct vec3 right;
	struct vec3 wish;
	struct vec3 pan;
	struct aabb near_solids[64];
	int nsolids;
	int i;
	float speed;

	g = (struct game *)user;
	in = app_Input();

	if (in->hit[PLAT_KEY_ESC])
		app_Quit();
	if (in->hit['G']) {
		g->mouse_look = !g->mouse_look;
		plat_GrabMouse(g->mouse_look);
	}
	if (in->hit['1'])
		cam_SetMode(&g->cam, CAM_FIRST);
	if (in->hit['2'])
		cam_SetMode(&g->cam, CAM_THIRD);
	if (in->hit['3'])
		cam_SetMode(&g->cam, CAM_TOP);
	if (in->hit[PLAT_KEY_TAB])
		g->controlled = (g->controlled + 1) % UNITS;

	if (g->mouse_look)
		cam_Look(&g->cam, (float)in->mouse_dx, (float)in->mouse_dy);

	/*  Движение считается относительно взгляда КАМЕРЫ, а не персонажа:
	 *  игрок жмёт W и ожидает, что пойдёт туда, куда смотрит экран.
	 */
	right = cam_Right(&g->cam);
	forward = v3(right.z, 0.0f, -right.x);

	wish = v3(0.0f, 0.0f, 0.0f);
	if (in->hold['W'])
		wish = v3_add(wish, forward);
	if (in->hold['S'])
		wish = v3_sub(wish, forward);
	if (in->hold['D'])
		wish = v3_add(wish, right);
	if (in->hold['A'])
		wish = v3_sub(wish, right);

	speed = in->hold[PLAT_KEY_SHIFT] ? 8.0f : 4.0f;
	wish = v3_scale(v3_norm(wish), speed);

	if (g->cam.mode == CAM_TOP) {
		/*  Сверху клавиши ведут камеру, а не персонажа: камера ни к
		 *  кому не привязана. Пробел отдаёт приказ идти в ту точку,
		 *  над которой она висит.
		 */
		pan = v3_scale(wish, dt * 2.5f);
		g->map_focus = v3_add(g->map_focus, pan);
		g->map_focus.x = m3_clampf(g->map_focus.x, -8.0f, 8.0f);
		g->map_focus.z = m3_clampf(g->map_focus.z, -8.0f, 24.0f);

		if (in->hit[' ']) {
			g->unit[g->controlled].has_order = 1;
			g->unit[g->controlled].order = g->map_focus;
		}
		wish = v3(0.0f, 0.0f, 0.0f);
	} else {
		if (g->unit[g->controlled].on_floor && in->hold[' '])
			g->unit[g->controlled].velocity.y = 6.0f;
		g->unit[g->controlled].has_order = 0;
	}

	for (i = 0; i < UNITS; i++)
		unit_step(g, &g->unit[i],
		    i == g->controlled ? wish : v3(0.0f, 0.0f, 0.0f), dt);

	/*  Камера обновляется ПОСЛЕ подопечных, иначе она весь кадр
	 *  показывает вчерашнее положение и картинка запаздывает.
	 *  Коробки берутся вокруг точки интереса, потому что поводок
	 *  упирается именно в стены рядом с ней.
	 */
	if (g->cam.mode == CAM_TOP) {
		nsolids = 0;
		cam_Update(&g->cam, g->map_focus, 0, 0);
	} else {
		nsolids = solids_around(g, g->unit[g->controlled].pos,
		    near_solids, 64);
		cam_Update(&g->cam, g->unit[g->controlled].pos, near_solids,
		    nsolids);
	}

	snd_Listener(g->cam.pos, cam_Forward(&g->cam));
}

static void
game_draw(void *user, float alpha)
{
	struct game *g;
	float view[16];
	float proj[16];
	float viewproj[16];
	float model[16];
	float spin[16];
	float move[16];
	float scale[16];
	float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float mark[4] = { 1.0f, 0.9f, 0.3f, 0.9f };
	int sectors[16];
	int nsectors;
	int i;
	float aspect;

	(void)alpha;
	g = (struct game *)user;

	aspect = (float)app_Width() / (float)app_Height();
	cam_View(&g->cam, view);
	m4_perspective(proj, 1.2f, aspect, 0.1f, 200.0f);
	m4_mul(viewproj, proj, view);

	gfx_BeginFrame(0.05f, 0.06f, 0.08f);
	gfx_SetCamera(view, proj);

	/*  Видимость считается от сектора КАМЕРЫ. В третьем лице это
	 *  регулярно не тот сектор, где стоит персонаж.
	 */
	nsectors = wld_Visible(&g->world, wld_SectorAt(&g->world, g->cam.pos),
	    viewproj, sectors, 16);

	m4_identity(model);
	for (i = 0; i < nsectors; i++)
		gfx_DrawMesh(g->world.sector[sectors[i]].mesh, model,
		    g->world.sector[sectors[i]].tex, white, 0, -1);

	if (g->model_mesh != 0) {
		for (i = 0; i < UNITS; i++) {
			/*  Из глаз своего же персонажа его модель не видна.  */
			if (g->cam.mode == CAM_FIRST && i == g->controlled)
				continue;

			m4_rot_y(spin, g->unit[i].yaw);
			m4_scale(scale, 0.004f, 0.004f, 0.004f);
			m4_translate(move, g->unit[i].pos.x,
			    g->unit[i].pos.y - 0.9f, g->unit[i].pos.z);
			m4_mul(model, spin, scale);
			m4_mul(model, move, model);
			gfx_DrawMesh(g->model_mesh, model, g->wall_tex,
			    g->unit[i].colour, 0, -1);
		}
	}

	/*  Метка на точке, над которой висит камера сверху.  */
	if (g->cam.mode == CAM_TOP)
		gfx_DrawSprite(v3(g->map_focus.x, g->map_focus.y + 0.2f,
		    g->map_focus.z), 0.6f, 0.6f, 0, mark);

	gui_Begin(app_Input());
	if (g->cam.mode == CAM_FIRST) {
		gui_SetColor(1.0f, 1.0f, 1.0f, 0.75f);
		gui_Rect((float)app_Width() * 0.5f - 6.0f,
		    (float)app_Height() * 0.5f - 1.0f, 12.0f, 2.0f);
		gui_Rect((float)app_Width() * 0.5f - 1.0f,
		    (float)app_Height() * 0.5f - 6.0f, 2.0f, 12.0f);
	}
	/*  Полоска, показывающая, кем управляют: по одной клетке на юнита. */
	for (i = 0; i < UNITS; i++) {
		if (i == g->controlled)
			gui_SetColor(g->unit[i].colour[0], g->unit[i].colour[1],
			    g->unit[i].colour[2], 0.95f);
		else
			gui_SetColor(g->unit[i].colour[0] * 0.35f,
			    g->unit[i].colour[1] * 0.35f,
			    g->unit[i].colour[2] * 0.35f, 0.7f);
		gui_Rect(16.0f + (float)i * 28.0f,
		    (float)app_Height() - 32.0f, 22.0f, 16.0f);
	}
	gui_End();
}

static void
game_quit(void *user)
{
	(void)user;
	printf("demo: %.1f fps at exit\n", (double)app_Fps());
}

int
main(void)
{
	static struct game g;
	struct app_hooks hooks;

	hooks.title = "engine demo";
	hooks.width = 1024;
	hooks.height = 640;
	hooks.step_hz = 60.0f;
	hooks.max_fps = 250;
	hooks.audio_rate = 44100;
	hooks.init = game_init;
	hooks.step = game_step;
	hooks.draw = game_draw;
	hooks.quit = game_quit;
	hooks.user = &g;

	return app_Run(&hooks);
}
