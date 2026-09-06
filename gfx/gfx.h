/*
 *	@(#)gfx.h	1.0
 *
 *  The renderer, as seen from the game.  No GL type, no GL constant and no
 *  GL header appears here or anywhere above here: textures and meshes are
 *  opaque handles, and 0 always means "none".
 *
 *  What this deliberately cannot do: shaders written by the game, render
 *  targets, instancing, skeletal skinning on the GPU, post processing.
 *  It draws lit textured triangles, sprites and 2D quads with fog.  That
 *  is a PlayStation 2 with better filtering, which is the whole target.
 *
 *  Backends:
 *	gfx_gl.c	OpenGL 2.1 / GLES2, one shader, entry points from
 *			plat_GlProc()
 *	gfx_null.c	counts draw calls and draws nothing
 */
#ifndef GFX_H_SENTRY
#define GFX_H_SENTRY

#include "../core/m3.h"

/*  32 bytes, interleaved, the layout every loader produces.  */
struct gfx_vertex {
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u;
	float v;
};

int gfx_Init(void);
void gfx_Shutdown(void);
void gfx_Viewport(int width, int height);

void gfx_BeginFrame(float r, float g, float b);
void gfx_EndFrame(void);

/*  TEXTURES.  geo is a buf geometry: unit 4 is RGBA, unit 3 is RGB, and
 *  the rows must be packed.  Nothing else about a picture matters here.
 */
unsigned int gfx_MakeTexture(void *pixels, unsigned long long geo,
    int smooth, int repeat);
void gfx_FreeTexture(unsigned int tex);

/*  MESHES.  dynamic = 1 if you will call gfx_UpdateMesh() every frame,
 *  which is what vertex animation does.
 */
unsigned int gfx_MakeMesh(const struct gfx_vertex *verts, int nverts,
    const unsigned short *index, int nindex, int dynamic);
void gfx_UpdateMesh(unsigned int mesh, const struct gfx_vertex *verts,
    int nverts);
void gfx_FreeMesh(unsigned int mesh);

/*  STATE.  Set once per frame, not per object.  */
void gfx_SetCamera(const float view[16], const float proj[16]);
void gfx_SetLight(const vector direction, float ambient);
void gfx_SetFog(float r, float g, float b, float start, float end);
/* NOTE:  end <= start turns fog off.  Fog is not decoration here: it is
 *	  what hides the far clip plane when the world streams in.
 */

/*  DRAWING.  first and count are indices, not triangles; count < 0 means
 *  the whole mesh.  A material group from an .obj is exactly a (first,
 *  count, texture) triple, which is why this is not hidden.
 */
void gfx_DrawMesh(unsigned int mesh, const float model[16],
    unsigned int tex, const float rgba[4], int first, int count);
void gfx_DrawSprite(const vector centre, float w, float h, unsigned int tex,
    const float rgba[4]);

/*  2D, in pixels, origin top left.  Menus, HUD, subtitles.  */
void gfx_Begin2D(void);
void gfx_Quad(float x, float y, float w, float h, unsigned int tex,
    float u0, float v0, float u1, float v1, const float rgba[4]);
void gfx_End2D(void);

int gfx_DrawCalls(void);	/* reset every gfx_BeginFrame() */

#endif /* GFX_H_SENTRY */
