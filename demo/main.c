/*
 *	@(#)main.c	1.0
 *
 *  A game, not an engine feature list.  Two rooms joined by a doorway, a
 *  model turning in the first one, boxes you cannot walk through, gravity,
 *  mouse look.  Everything the engine can do at all is visible here.
 *
 *  Notice where the file reading is: in this file, in slurp(), using plain
 *  fopen().  The engine never opens anything.  Replace slurp() with a
 *  pointer to an array made by "xxd -i" and nothing else changes.
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
#include "../ui/gui.h"

static char pool[48 * 1024 * 1024];

struct game {
	struct arena mem;
	struct world world;

	unsigned int room_mesh;
	unsigned int wall_tex;
	int room_index_count;

	struct obj_mesh model;
	unsigned int model_mesh;
	float model_spin;

	struct vec3 eye;
	struct vec3 velocity;
	float yaw;
	float pitch;
	int on_floor;
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

/*  A room is a box seen from the inside, so every quad is wound the other
 *  way round.  Floor, ceiling and four walls, with a gap left in one wall
 *  when `door` says so.
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

	g = (struct game *)user;
	arena_Init(&g->mem, pool, (long)sizeof pool);

	/*  Texture.  Loaded, decoded, uploaded, and the pixels thrown away
	 *  again: the GPU has them now.
	 */
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

	/*  Model.  Kept, because collision and animation want the vertices
	 *  on the CPU as well.
	 */
	mark = arena_Mark(&g->mem);
	bytes = slurp(&g->mem, "demo/freebsd.obj", &len);
	if (bytes != 0 && obj_Parse(&g->model, bytes, len, &g->mem) == 0) {
		g->model_mesh = gfx_MakeMesh(g->model.verts, g->model.nverts,
		    g->model.index, g->model.nindex, 0);
		printf("demo: model %d vertices, %d indices, %d groups\n",
		    g->model.nverts, g->model.nindex, g->model.ngroups);
	}

	/*  Level geometry, built in code.  A real game would load two .obj
	 *  files here instead; the engine cannot tell the difference.
	 */
	b.v = arena_Alloc(&g->mem, 256 * (long)sizeof(struct gfx_vertex), 4);
	b.i = arena_Alloc(&g->mem, 512 * (long)sizeof(unsigned short), 2);
	b.nv = 0;
	b.ni = 0;
	room(&b, -8.0f, -8.0f, 8.0f, 8.0f, 4.0f, 1);
	room(&b, -8.0f, 8.0f, 8.0f, 24.0f, 4.0f, 0);
	g->room_mesh = gfx_MakeMesh(b.v, b.nv, b.i, b.ni, 0);
	g->room_index_count = b.ni;

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

	g->eye = v3(0.0f, 1.7f, 5.0f);
	g->velocity = v3(0.0f, 0.0f, 0.0f);
	g->yaw = 0.0f;
	g->pitch = 0.0f;
	g->mouse_look = 1;
	plat_GrabMouse(1);

	gfx_SetLight(v3(-0.5f, -1.0f, -0.3f), 0.35f);
	gfx_SetFog(0.05f, 0.06f, 0.08f, 12.0f, 40.0f);

	printf("demo: arena %ld of %ld bytes used\n", g->mem.used,
	    g->mem.size);
	printf("demo: WASD, mouse to look, space to jump, TAB releases the "
	    "pointer, ESC quits\n");
	return 0;
}

static void
game_step(void *user, float dt)
{
	struct game *g;
	const struct plat_input *in;
	struct vec3 forward;
	struct vec3 right;
	struct vec3 wish;
	struct aabb body;
	struct aabb near_solids[64];
	int sectors[16];
	int nsectors;
	int nsolids;
	int here;
	float speed;

	g = (struct game *)user;
	in = app_Input();

	if (in->hit[PLAT_KEY_ESC])
		app_Quit();
	if (in->hit[PLAT_KEY_TAB]) {
		g->mouse_look = !g->mouse_look;
		plat_GrabMouse(g->mouse_look);
	}

	if (g->mouse_look) {
		g->yaw += (float)in->mouse_dx * 0.0025f;
		g->pitch -= (float)in->mouse_dy * 0.0025f;
		g->pitch = m3_clampf(g->pitch, -1.5f, 1.5f);
	}

	forward = v3(sinf(g->yaw), 0.0f, -cosf(g->yaw));
	right = v3(cosf(g->yaw), 0.0f, sinf(g->yaw));

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

	g->velocity.x = wish.x;
	g->velocity.z = wish.z;
	g->velocity.y -= 18.0f * dt;
	if (g->on_floor && in->hold[' '])
		g->velocity.y = 6.0f;

	/*  Collide against the sectors we can reach, not the whole level. */
	here = wld_SectorAt(&g->world, g->eye);
	sectors[0] = here;
	nsectors = 1;
	if (here < 0) {
		nsectors = g->world.nsectors < 16 ? g->world.nsectors : 16;
		for (nsolids = 0; nsolids < nsectors; nsolids++)
			sectors[nsolids] = nsolids;
	}
	nsolids = wld_SolidsNear(&g->world, sectors, nsectors, near_solids,
	    64);

	body = coll_MakeAabb(g->eye, v3(0.35f, 0.9f, 0.35f));
	g->eye = v3_add(g->eye, coll_MoveAabb(body,
	    v3_scale(g->velocity, dt), near_solids, nsolids, &g->on_floor));

	/*  The room has no floor box, so keep the feet on the ground the
	 *  cheap way.  A level built from .obj would give the floor a solid
	 *  like everything else.
	 */
	if (g->eye.y < 1.7f) {
		g->eye.y = 1.7f;
		g->velocity.y = 0.0f;
		g->on_floor = 1;
	}

	g->model_spin += dt * 0.6f;
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
	int sectors[16];
	int nsectors;
	int i;
	float aspect;

	(void)alpha;
	g = (struct game *)user;

	aspect = (float)app_Width() / (float)app_Height();
	m4_fps_view(view, g->eye, g->yaw, g->pitch);
	m4_perspective(proj, 1.2f, aspect, 0.1f, 200.0f);
	m4_mul(viewproj, proj, view);

	gfx_BeginFrame(0.05f, 0.06f, 0.08f);
	gfx_SetCamera(view, proj);

	nsectors = wld_Visible(&g->world, wld_SectorAt(&g->world, g->eye),
	    viewproj, sectors, 16);

	/*  Both sectors share one mesh here, so this draws it twice; with a
	 *  mesh per sector, which is what a real level has, each visible
	 *  sector is exactly one draw call.
	 */
	m4_identity(model);
	for (i = 0; i < nsectors; i++) {
		gfx_DrawMesh(g->world.sector[sectors[i]].mesh, model,
		    g->world.sector[sectors[i]].tex, white, 0, -1);
	}

	if (g->model_mesh != 0) {
		m4_rot_y(spin, g->model_spin);
		m4_scale(scale, 0.004f, 0.004f, 0.004f);
		m4_translate(move, 0.0f, 1.2f, 0.0f);
		m4_mul(model, spin, scale);
		m4_mul(model, move, model);
		gfx_DrawMesh(g->model_mesh, model, g->wall_tex, white, 0, -1);
	}

	gui_Begin(app_Input());
	gui_SetColor(1.0f, 1.0f, 1.0f, 0.75f);
	gui_Rect((float)app_Width() * 0.5f - 6.0f,
	    (float)app_Height() * 0.5f - 1.0f, 12.0f, 2.0f);
	gui_Rect((float)app_Width() * 0.5f - 1.0f,
	    (float)app_Height() * 0.5f - 6.0f, 2.0f, 12.0f);
	gui_End();
}

static void
game_quit(void *user)
{
	struct game *g;

	g = (struct game *)user;
	printf("demo: %.1f fps at exit\n", (double)app_Fps());
	(void)g;
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
