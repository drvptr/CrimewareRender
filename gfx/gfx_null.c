/*
 *	@(#)gfx_null.c	1.0
 *
 *  Draws nothing, counts everything.  The game code that runs against this
 *  is byte for byte the game code that runs against GL, which is the point
 *  of gfx.h having no GL in it.
 */
#include "gfx.h"
#include "../buf/buffer.h"

static unsigned int next_handle = 1;
static int draw_calls;

int
gfx_Init(void)
{
	return 1;
}

void
gfx_Shutdown(void)
{
}

void
gfx_Viewport(int width, int height)
{
	(void)width;
	(void)height;
}

void
gfx_BeginFrame(float r, float g, float b)
{
	(void)r;
	(void)g;
	(void)b;
	draw_calls = 0;
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
	(void)smooth;
	(void)repeat;
	if (pixels == 0 || !bufGeomCheck(geo))
		return 0;
	return next_handle++;
}

void
gfx_FreeTexture(unsigned int tex)
{
	(void)tex;
}

unsigned int
gfx_MakeMesh(const struct gfx_vertex *verts, int nverts,
    const unsigned short *index, int nindex, int dynamic)
{
	(void)dynamic;
	if (verts == 0 || nverts <= 0 || index == 0 || nindex <= 0)
		return 0;
	return next_handle++;
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
	(void)mesh;
}

void
gfx_SetCamera(const float view[16], const float proj[16])
{
	(void)view;
	(void)proj;
}

void
gfx_SetLight(const vector direction, float ambient)
{
	(void)direction;
	(void)ambient;
}

void
gfx_SetFog(float r, float g, float b, float start, float end)
{
	(void)r;
	(void)g;
	(void)b;
	(void)start;
	(void)end;
}

void
gfx_DrawMesh(unsigned int mesh, const float model[16], unsigned int tex,
    const float rgba[4], int first, int count)
{
	(void)mesh;
	(void)model;
	(void)tex;
	(void)rgba;
	(void)first;
	(void)count;
	draw_calls++;
}

void
gfx_DrawSprite(const vector centre, float w, float h, unsigned int tex,
    const float rgba[4])
{
	(void)centre;
	(void)w;
	(void)h;
	(void)tex;
	(void)rgba;
	draw_calls++;
}

void
gfx_Begin2D(void)
{
}

void
gfx_Quad(float x, float y, float w, float h, unsigned int tex,
    float u0, float v0, float u1, float v1, const float rgba[4])
{
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)tex;
	(void)u0;
	(void)v0;
	(void)u1;
	(void)v1;
	(void)rgba;
	draw_calls++;
}

void
gfx_End2D(void)
{
}
