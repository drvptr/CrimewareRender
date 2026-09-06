/*
 *	@(#)gfx_gl.c	1.0
 *
 *  OpenGL 2.1 (equivalently GLES2) with exactly one shader program.  Every
 *  entry point is a function pointer taken from plat_GlProc(), so this file
 *  includes no GL header and the binary links against no GL library.  The
 *  GL types are written out as what they actually are: unsigned int, int,
 *  float, char.
 *
 *  One program does all of it.  u_unlit turns the lighting off for 2D and
 *  sprites, u_textured turns the sampler off for untextured geometry.  Two
 *  branches on a uniform cost nothing and save a second shader.
 */
#include <stdio.h>
#include "gfx.h"
#include "../plat/plat.h"
#include "../buf/buffer.h"

#define GL_DEPTH_BUFFER_BIT		0x00000100
#define GL_COLOR_BUFFER_BIT		0x00004000
#define GL_TRIANGLES			0x0004
#define GL_LEQUAL			0x0203
#define GL_SRC_ALPHA			0x0302
#define GL_ONE_MINUS_SRC_ALPHA		0x0303
#define GL_CULL_FACE			0x0B44
#define GL_DEPTH_TEST			0x0B71
#define GL_BLEND			0x0BE2
#define GL_UNPACK_ALIGNMENT		0x0CF5
#define GL_TEXTURE_2D			0x0DE1
#define GL_UNSIGNED_BYTE		0x1401
#define GL_UNSIGNED_SHORT		0x1403
#define GL_FLOAT			0x1406
#define GL_RGB				0x1907
#define GL_RGBA				0x1908
#define GL_NEAREST			0x2600
#define GL_LINEAR			0x2601
#define GL_TEXTURE_MAG_FILTER		0x2800
#define GL_TEXTURE_MIN_FILTER		0x2801
#define GL_TEXTURE_WRAP_S		0x2802
#define GL_TEXTURE_WRAP_T		0x2803
#define GL_REPEAT			0x2901
#define GL_CLAMP_TO_EDGE		0x812F
#define GL_ARRAY_BUFFER			0x8892
#define GL_ELEMENT_ARRAY_BUFFER		0x8893
#define GL_STATIC_DRAW			0x88E4
#define GL_DYNAMIC_DRAW			0x88E8
#define GL_FRAGMENT_SHADER		0x8B30
#define GL_VERTEX_SHADER		0x8B31
#define GL_COMPILE_STATUS		0x8B81
#define GL_LINK_STATUS			0x8B82
#define GL_TEXTURE0			0x84C0

static void (*p_glClear)(unsigned int);
static void (*p_glClearColor)(float, float, float, float);
static void (*p_glEnable)(unsigned int);
static void (*p_glDisable)(unsigned int);
static void (*p_glViewport)(int, int, int, int);
static void (*p_glDepthFunc)(unsigned int);
static void (*p_glDepthMask)(unsigned char);
static void (*p_glBlendFunc)(unsigned int, unsigned int);
static void (*p_glPixelStorei)(unsigned int, int);
static void (*p_glGenTextures)(int, unsigned int *);
static void (*p_glBindTexture)(unsigned int, unsigned int);
static void (*p_glDeleteTextures)(int, const unsigned int *);
static void (*p_glTexImage2D)(unsigned int, int, int, int, int, int,
    unsigned int, unsigned int, const void *);
static void (*p_glTexParameteri)(unsigned int, unsigned int, int);
static void (*p_glActiveTexture)(unsigned int);
static void (*p_glGenBuffers)(int, unsigned int *);
static void (*p_glBindBuffer)(unsigned int, unsigned int);
static void (*p_glBufferData)(unsigned int, long, const void *, unsigned int);
static void (*p_glBufferSubData)(unsigned int, long, long, const void *);
static void (*p_glDeleteBuffers)(int, const unsigned int *);
static unsigned int (*p_glCreateShader)(unsigned int);
static void (*p_glShaderSource)(unsigned int, int, const char **, const int *);
static void (*p_glCompileShader)(unsigned int);
static void (*p_glGetShaderiv)(unsigned int, unsigned int, int *);
static void (*p_glGetShaderInfoLog)(unsigned int, int, int *, char *);
static void (*p_glDeleteShader)(unsigned int);
static unsigned int (*p_glCreateProgram)(void);
static void (*p_glAttachShader)(unsigned int, unsigned int);
static void (*p_glBindAttribLocation)(unsigned int, unsigned int,
    const char *);
static void (*p_glLinkProgram)(unsigned int);
static void (*p_glGetProgramiv)(unsigned int, unsigned int, int *);
static void (*p_glGetProgramInfoLog)(unsigned int, int, int *, char *);
static void (*p_glUseProgram)(unsigned int);
static int (*p_glGetUniformLocation)(unsigned int, const char *);
static void (*p_glUniform1i)(int, int);
static void (*p_glUniform1f)(int, float);
static void (*p_glUniform2f)(int, float, float);
static void (*p_glUniform3f)(int, float, float, float);
static void (*p_glUniform4fv)(int, int, const float *);
static void (*p_glUniformMatrix4fv)(int, int, unsigned char, const float *);
static void (*p_glEnableVertexAttribArray)(unsigned int);
static void (*p_glVertexAttribPointer)(unsigned int, int, unsigned int,
    unsigned char, int, const void *);
static void (*p_glDrawElements)(unsigned int, int, unsigned int,
    const void *);

#define GFX_MAX_MESHES 1024

struct gl_mesh {
	unsigned int vbo;
	unsigned int ibo;
	int nindex;
	int nverts;
	int used;
};

static struct gl_mesh meshes[GFX_MAX_MESHES];
static unsigned int prog;
static int u_mvp;
static int u_model;
static int u_lightdir;
static int u_ambient;
static int u_tint;
static int u_unlit;
static int u_textured;
static int u_fogcol;
static int u_fog;
static int u_tex;

static float cur_view[16];
static float cur_proj[16];
static float cur_viewproj[16];
static int screen_w = 640;
static int screen_h = 480;
static int draw_calls;
static unsigned int quad_mesh;

static const char *vertex_src =
"attribute vec3 a_pos;\n"
"attribute vec3 a_nrm;\n"
"attribute vec2 a_uv;\n"
"uniform mat4 u_mvp;\n"
"uniform mat4 u_model;\n"
"uniform vec3 u_lightdir;\n"
"uniform float u_ambient;\n"
"uniform vec4 u_tint;\n"
"uniform float u_unlit;\n"
"varying vec2 v_uv;\n"
"varying vec4 v_col;\n"
"varying float v_dist;\n"
"void main(){\n"
"  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
"  vec3 n = normalize(mat3(u_model) * a_nrm);\n"
"  float d = max(dot(n, -u_lightdir), 0.0);\n"
"  float l = min(u_ambient + d * (1.0 - u_ambient), 1.0);\n"
"  l = mix(l, 1.0, u_unlit);\n"
"  v_col = vec4(u_tint.rgb * l, u_tint.a);\n"
"  v_uv = a_uv;\n"
"  v_dist = gl_Position.w;\n"
"}\n";

static const char *fragment_src =
"#ifdef GL_ES\n"
"precision mediump float;\n"
"#endif\n"
"uniform sampler2D u_tex;\n"
"uniform float u_textured;\n"
"uniform vec3 u_fogcol;\n"
"uniform vec2 u_fog;\n"
"varying vec2 v_uv;\n"
"varying vec4 v_col;\n"
"varying float v_dist;\n"
"void main(){\n"
"  vec4 c = v_col;\n"
"  if (u_textured > 0.5)\n"
"    c = c * texture2D(u_tex, v_uv);\n"
"  if (c.a < 0.02)\n"
"    discard;\n"
"  float f = 0.0;\n"
"  if (u_fog.y > u_fog.x)\n"
"    f = clamp((v_dist - u_fog.x) / (u_fog.y - u_fog.x), 0.0, 1.0);\n"
"  gl_FragColor = vec4(mix(c.rgb, u_fogcol, f), c.a);\n"
"}\n";

/*  dlsym() hands back a void *, and a void * is not a function pointer in
 *  ISO C - it is in POSIX, which is why every dynamic loader on earth works
 *  this way.  The assignments below are correct on every platform this
 *  engine will ever run on, and -pedantic is right to say they are outside
 *  the standard, so the complaint is silenced here and nowhere else.
 */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

static int
load_entry_points(void)
{
	p_glClear = plat_GlProc("glClear");
	p_glClearColor = plat_GlProc("glClearColor");
	p_glEnable = plat_GlProc("glEnable");
	p_glDisable = plat_GlProc("glDisable");
	p_glViewport = plat_GlProc("glViewport");
	p_glDepthFunc = plat_GlProc("glDepthFunc");
	p_glDepthMask = plat_GlProc("glDepthMask");
	p_glBlendFunc = plat_GlProc("glBlendFunc");
	p_glPixelStorei = plat_GlProc("glPixelStorei");
	p_glGenTextures = plat_GlProc("glGenTextures");
	p_glBindTexture = plat_GlProc("glBindTexture");
	p_glDeleteTextures = plat_GlProc("glDeleteTextures");
	p_glTexImage2D = plat_GlProc("glTexImage2D");
	p_glTexParameteri = plat_GlProc("glTexParameteri");
	p_glActiveTexture = plat_GlProc("glActiveTexture");
	p_glGenBuffers = plat_GlProc("glGenBuffers");
	p_glBindBuffer = plat_GlProc("glBindBuffer");
	p_glBufferData = plat_GlProc("glBufferData");
	p_glBufferSubData = plat_GlProc("glBufferSubData");
	p_glDeleteBuffers = plat_GlProc("glDeleteBuffers");
	p_glCreateShader = plat_GlProc("glCreateShader");
	p_glShaderSource = plat_GlProc("glShaderSource");
	p_glCompileShader = plat_GlProc("glCompileShader");
	p_glGetShaderiv = plat_GlProc("glGetShaderiv");
	p_glGetShaderInfoLog = plat_GlProc("glGetShaderInfoLog");
	p_glDeleteShader = plat_GlProc("glDeleteShader");
	p_glCreateProgram = plat_GlProc("glCreateProgram");
	p_glAttachShader = plat_GlProc("glAttachShader");
	p_glBindAttribLocation = plat_GlProc("glBindAttribLocation");
	p_glLinkProgram = plat_GlProc("glLinkProgram");
	p_glGetProgramiv = plat_GlProc("glGetProgramiv");
	p_glGetProgramInfoLog = plat_GlProc("glGetProgramInfoLog");
	p_glUseProgram = plat_GlProc("glUseProgram");
	p_glGetUniformLocation = plat_GlProc("glGetUniformLocation");
	p_glUniform1i = plat_GlProc("glUniform1i");
	p_glUniform1f = plat_GlProc("glUniform1f");
	p_glUniform2f = plat_GlProc("glUniform2f");
	p_glUniform3f = plat_GlProc("glUniform3f");
	p_glUniform4fv = plat_GlProc("glUniform4fv");
	p_glUniformMatrix4fv = plat_GlProc("glUniformMatrix4fv");
	p_glEnableVertexAttribArray = plat_GlProc("glEnableVertexAttribArray");
	p_glVertexAttribPointer = plat_GlProc("glVertexAttribPointer");
	p_glDrawElements = plat_GlProc("glDrawElements");

	if (p_glClear == 0 || p_glCreateProgram == 0 || p_glGenBuffers == 0 ||
	    p_glDrawElements == 0 || p_glUniformMatrix4fv == 0) {
		fprintf(stderr, "gfx: this GL has no shaders (need 2.0)\n");
		return 0;
	}
	return 1;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static unsigned int
compile(unsigned int kind, const char *src)
{
	unsigned int id;
	int ok;
	char log[1024];

	id = p_glCreateShader(kind);
	p_glShaderSource(id, 1, &src, 0);
	p_glCompileShader(id);
	ok = 0;
	p_glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		log[0] = 0;
		p_glGetShaderInfoLog(id, (int)sizeof log, 0, log);
		fprintf(stderr, "gfx: shader: %s\n", log);
		return 0;
	}
	return id;
}

static int
build_program(void)
{
	unsigned int vs;
	unsigned int fs;
	int ok;
	char log[1024];

	vs = compile(GL_VERTEX_SHADER, vertex_src);
	fs = compile(GL_FRAGMENT_SHADER, fragment_src);
	if (vs == 0 || fs == 0)
		return 0;

	prog = p_glCreateProgram();
	p_glAttachShader(prog, vs);
	p_glAttachShader(prog, fs);
	p_glBindAttribLocation(prog, 0, "a_pos");
	p_glBindAttribLocation(prog, 1, "a_nrm");
	p_glBindAttribLocation(prog, 2, "a_uv");
	p_glLinkProgram(prog);

	ok = 0;
	p_glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		log[0] = 0;
		p_glGetProgramInfoLog(prog, (int)sizeof log, 0, log);
		fprintf(stderr, "gfx: link: %s\n", log);
		return 0;
	}
	p_glDeleteShader(vs);
	p_glDeleteShader(fs);

	u_mvp = p_glGetUniformLocation(prog, "u_mvp");
	u_model = p_glGetUniformLocation(prog, "u_model");
	u_lightdir = p_glGetUniformLocation(prog, "u_lightdir");
	u_ambient = p_glGetUniformLocation(prog, "u_ambient");
	u_tint = p_glGetUniformLocation(prog, "u_tint");
	u_unlit = p_glGetUniformLocation(prog, "u_unlit");
	u_textured = p_glGetUniformLocation(prog, "u_textured");
	u_fogcol = p_glGetUniformLocation(prog, "u_fogcol");
	u_fog = p_glGetUniformLocation(prog, "u_fog");
	u_tex = p_glGetUniformLocation(prog, "u_tex");
	return 1;
}

static struct gl_mesh *
mesh_at(unsigned int handle)
{
	if (handle == 0 || handle > GFX_MAX_MESHES)
		return 0;
	if (!meshes[handle - 1].used)
		return 0;
	return &meshes[handle - 1];
}

int
gfx_Init(void)
{
	struct gfx_vertex quad[4];
	unsigned short qidx[6] = { 0, 1, 2, 0, 2, 3 };
	int i;

	if (!load_entry_points())
		return 0;
	if (!build_program())
		return 0;

	p_glEnable(GL_DEPTH_TEST);
	p_glDepthFunc(GL_LEQUAL);
	p_glEnable(GL_CULL_FACE);
	p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	p_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	p_glUseProgram(prog);
	if (u_tex >= 0)
		p_glUniform1i(u_tex, 0);

	/*  A unit quad, reused by every 2D call and every sprite.  */
	for (i = 0; i < 4; i++) {
		quad[i].nx = 0.0f;
		quad[i].ny = 0.0f;
		quad[i].nz = 1.0f;
	}
	quad[0].x = 0.0f; quad[0].y = 0.0f; quad[0].z = 0.0f;
	quad[1].x = 1.0f; quad[1].y = 0.0f; quad[1].z = 0.0f;
	quad[2].x = 1.0f; quad[2].y = 1.0f; quad[2].z = 0.0f;
	quad[3].x = 0.0f; quad[3].y = 1.0f; quad[3].z = 0.0f;
	quad[0].u = 0.0f; quad[0].v = 0.0f;
	quad[1].u = 1.0f; quad[1].v = 0.0f;
	quad[2].u = 1.0f; quad[2].v = 1.0f;
	quad[3].u = 0.0f; quad[3].v = 1.0f;
	quad_mesh = gfx_MakeMesh(quad, 4, qidx, 6, 1);

	m4_identity(cur_view);
	m4_identity(cur_proj);
	m4_identity(cur_viewproj);
	gfx_SetLight(v3(-0.4f, -0.8f, -0.4f), 0.35f);
	gfx_SetFog(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	return 1;
}

void
gfx_Shutdown(void)
{
	int i;

	for (i = 0; i < GFX_MAX_MESHES; i++) {
		if (meshes[i].used)
			gfx_FreeMesh((unsigned int)(i + 1));
	}
}

void
gfx_Viewport(int width, int height)
{
	screen_w = width;
	screen_h = height;
	if (p_glViewport != 0)
		p_glViewport(0, 0, width, height);
}

void
gfx_BeginFrame(float r, float g, float b)
{
	draw_calls = 0;
	p_glClearColor(r, g, b, 1.0f);
	p_glDepthMask(1);
	p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	p_glUseProgram(prog);
	p_glEnable(GL_DEPTH_TEST);
	p_glEnable(GL_CULL_FACE);
	p_glEnable(GL_BLEND);
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

unsigned int
gfx_MakeTexture(void *pixels, unsigned long long geo, int smooth, int repeat)
{
	unsigned int tex;
	int format;
	int filter;
	int wrap;

	if (pixels == 0 || !bufGeomCheck(geo))
		return 0;
	if (!bufGeomIsPacked(geo)) {
		fprintf(stderr, "gfx: texture rows are not packed\n");
		return 0;
	}
	if (BUF_UNIT(geo) == 4)
		format = GL_RGBA;
	else if (BUF_UNIT(geo) == 3)
		format = GL_RGB;
	else {
		fprintf(stderr, "gfx: texture unit must be 3 or 4 bytes\n");
		return 0;
	}

	filter = smooth ? GL_LINEAR : GL_NEAREST;
	wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;

	tex = 0;
	p_glGenTextures(1, &tex);
	p_glBindTexture(GL_TEXTURE_2D, tex);
	p_glTexImage2D(GL_TEXTURE_2D, 0, format, (int)BUF_COLS(geo),
	    (int)BUF_ROWS(geo), 0, (unsigned int)format, GL_UNSIGNED_BYTE,
	    pixels);
	p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	return tex;
}

void
gfx_FreeTexture(unsigned int tex)
{
	if (tex != 0)
		p_glDeleteTextures(1, &tex);
}

unsigned int
gfx_MakeMesh(const struct gfx_vertex *verts, int nverts,
    const unsigned short *index, int nindex, int dynamic)
{
	int slot;
	struct gl_mesh *m;
	unsigned int usage;

	if (verts == 0 || nverts <= 0 || index == 0 || nindex <= 0)
		return 0;

	slot = -1;
	for (slot = 0; slot < GFX_MAX_MESHES; slot++) {
		if (!meshes[slot].used)
			break;
	}
	if (slot == GFX_MAX_MESHES)
		return 0;

	m = &meshes[slot];
	usage = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

	p_glGenBuffers(1, &m->vbo);
	p_glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
	p_glBufferData(GL_ARRAY_BUFFER,
	    (long)(nverts * (int)sizeof(struct gfx_vertex)), verts, usage);

	p_glGenBuffers(1, &m->ibo);
	p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
	p_glBufferData(GL_ELEMENT_ARRAY_BUFFER,
	    (long)(nindex * (int)sizeof(unsigned short)), index,
	    GL_STATIC_DRAW);

	m->nindex = nindex;
	m->nverts = nverts;
	m->used = 1;
	return (unsigned int)(slot + 1);
}

void
gfx_UpdateMesh(unsigned int handle, const struct gfx_vertex *verts,
    int nverts)
{
	struct gl_mesh *m;

	m = mesh_at(handle);
	if (m == 0 || verts == 0 || nverts <= 0)
		return;
	if (nverts > m->nverts)
		nverts = m->nverts;
	p_glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
	p_glBufferSubData(GL_ARRAY_BUFFER, 0,
	    (long)(nverts * (int)sizeof(struct gfx_vertex)), verts);
}

void
gfx_FreeMesh(unsigned int handle)
{
	struct gl_mesh *m;

	m = mesh_at(handle);
	if (m == 0)
		return;
	p_glDeleteBuffers(1, &m->vbo);
	p_glDeleteBuffers(1, &m->ibo);
	m->used = 0;
	m->nindex = 0;
	m->nverts = 0;
}

void
gfx_SetCamera(const float view[16], const float proj[16])
{
	m4_copy(cur_view, view);
	m4_copy(cur_proj, proj);
	m4_mul(cur_viewproj, cur_proj, cur_view);
}

void
gfx_SetLight(struct vec3 direction, float ambient)
{
	struct vec3 d;

	d = v3_norm(direction);
	p_glUseProgram(prog);
	if (u_lightdir >= 0)
		p_glUniform3f(u_lightdir, d.x, d.y, d.z);
	if (u_ambient >= 0)
		p_glUniform1f(u_ambient, ambient);
}

void
gfx_SetFog(float r, float g, float b, float start, float end)
{
	p_glUseProgram(prog);
	if (u_fogcol >= 0)
		p_glUniform3f(u_fogcol, r, g, b);
	if (u_fog >= 0)
		p_glUniform2f(u_fog, start, end);
}

static void
bind_attributes(struct gl_mesh *m)
{
	long stride;

	stride = (long)sizeof(struct gfx_vertex);
	p_glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
	p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
	p_glEnableVertexAttribArray(0);
	p_glEnableVertexAttribArray(1);
	p_glEnableVertexAttribArray(2);
	p_glVertexAttribPointer(0, 3, GL_FLOAT, 0, (int)stride,
	    (const void *)0);
	p_glVertexAttribPointer(1, 3, GL_FLOAT, 0, (int)stride,
	    (const void *)(3 * sizeof(float)));
	p_glVertexAttribPointer(2, 2, GL_FLOAT, 0, (int)stride,
	    (const void *)(6 * sizeof(float)));
}

static void
draw_indexed(struct gl_mesh *m, const float model[16], unsigned int tex,
    const float rgba[4], int first, int count, float unlit)
{
	float mvp[16];
	float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	if (count < 0)
		count = m->nindex;
	if (first < 0)
		first = 0;
	if (first + count > m->nindex)
		count = m->nindex - first;
	if (count <= 0)
		return;

	m4_mul(mvp, cur_viewproj, model);
	p_glUniformMatrix4fv(u_mvp, 1, 0, mvp);
	p_glUniformMatrix4fv(u_model, 1, 0, model);
	p_glUniform4fv(u_tint, 1, rgba != 0 ? rgba : white);
	p_glUniform1f(u_unlit, unlit);
	p_glUniform1f(u_textured, tex != 0 ? 1.0f : 0.0f);

	if (tex != 0) {
		p_glActiveTexture(GL_TEXTURE0);
		p_glBindTexture(GL_TEXTURE_2D, tex);
	}

	bind_attributes(m);
	p_glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT,
	    (const void *)((long)first * (long)sizeof(unsigned short)));
	draw_calls++;
}

void
gfx_DrawMesh(unsigned int handle, const float model[16], unsigned int tex,
    const float rgba[4], int first, int count)
{
	struct gl_mesh *m;

	m = mesh_at(handle);
	if (m == 0)
		return;
	draw_indexed(m, model, tex, rgba, first, count, 0.0f);
}

void
gfx_DrawSprite(struct vec3 centre, float w, float h, unsigned int tex,
    const float rgba[4])
{
	struct gl_mesh *m;
	float model[16];
	struct vec3 right;
	struct vec3 up;

	m = mesh_at(quad_mesh);
	if (m == 0)
		return;

	/*  A billboard is the camera's own axes, which are the rows of the
	 *  view matrix, scaled and moved to the sprite.
	 */
	right = v3(cur_view[0], cur_view[4], cur_view[8]);
	up = v3(cur_view[1], cur_view[5], cur_view[9]);

	m4_identity(model);
	model[0] = right.x * w;
	model[1] = right.y * w;
	model[2] = right.z * w;
	model[4] = up.x * h;
	model[5] = up.y * h;
	model[6] = up.z * h;
	model[12] = centre.x - (right.x * w + up.x * h) * 0.5f;
	model[13] = centre.y - (right.y * w + up.y * h) * 0.5f;
	model[14] = centre.z - (right.z * w + up.z * h) * 0.5f;

	p_glDisable(GL_CULL_FACE);
	draw_indexed(m, model, tex, rgba, 0, 6, 1.0f);
	p_glEnable(GL_CULL_FACE);
}

static float saved_view[16];
static float saved_proj[16];

void
gfx_Begin2D(void)
{
	float ortho[16];
	float ident[16];

	m4_copy(saved_view, cur_view);
	m4_copy(saved_proj, cur_proj);
	m4_ortho(ortho, 0.0f, (float)screen_w, (float)screen_h, 0.0f,
	    -1.0f, 1.0f);
	m4_identity(ident);
	gfx_SetCamera(ident, ortho);
	p_glDisable(GL_DEPTH_TEST);
	p_glDisable(GL_CULL_FACE);
}

void
gfx_Quad(float x, float y, float w, float h, unsigned int tex,
    float u0, float v0, float u1, float v1, const float rgba[4])
{
	struct gl_mesh *m;
	struct gfx_vertex v[4];
	float model[16];
	int i;

	m = mesh_at(quad_mesh);
	if (m == 0)
		return;

	for (i = 0; i < 4; i++) {
		v[i].nx = 0.0f;
		v[i].ny = 0.0f;
		v[i].nz = 1.0f;
		v[i].z = 0.0f;
	}
	v[0].x = x;     v[0].y = y;     v[0].u = u0; v[0].v = v0;
	v[1].x = x + w; v[1].y = y;     v[1].u = u1; v[1].v = v0;
	v[2].x = x + w; v[2].y = y + h; v[2].u = u1; v[2].v = v1;
	v[3].x = x;     v[3].y = y + h; v[3].u = u0; v[3].v = v1;

	gfx_UpdateMesh(quad_mesh, v, 4);
	m4_identity(model);
	draw_indexed(m, model, tex, rgba, 0, 6, 1.0f);
}

void
gfx_End2D(void)
{
	gfx_SetCamera(saved_view, saved_proj);
	p_glEnable(GL_DEPTH_TEST);
	p_glEnable(GL_CULL_FACE);
}
