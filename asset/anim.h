/*
 *	@(#)anim.h	1.0
 *
 *  Vertex animation: whole frames of absolute vertex positions, played back
 *  by interpolating between two of them.  Exactly what Quake and Half-Life
 *  models did, and exactly what you described.
 *
 *  Why not skeletal: a skeleton needs bones, weights, a bind pose, an
 *  inverse bind matrix per bone and an exporter that gets all four right.
 *  Vertex frames need an exporter that walks the mesh once.  The cost is
 *  memory - 5000 vertices at 30 frames is 1.8 MB - and no bone attachment
 *  points.  For a PlayStation 2 sized game that trade is the right one.
 *
 *  FILE FORMAT (.van), little endian, no padding:
 *
 *	0	"VAN1"
 *	4	int   nverts		matches the "v" count of the .obj
 *	8	int   nframes
 *	12	int   fps
 *	16	int   flags		1 = frames carry normals
 *	20	int   reserved, 0
 *	24	float frames[nframes][nverts][3 or 6]
 *
 *  Frame n, vertex k is the position of the k-th "v" line of the .obj.
 *  tools/van_export.py writes both files from the same Blender mesh in one
 *  run, so the two cannot drift apart.
 */
#ifndef ANIM_H_SENTRY
#define ANIM_H_SENTRY

#include "../gfx/gfx.h"

struct anim {
	const float *frames;
	int nverts;
	int nframes;
	int fps;
	int has_normals;
	int stride;	/* floats per vertex: 3 or 6 */
};

int anim_Parse(struct anim *out, const void *data, long len);

void anim_Sample(const struct anim *a, float seconds, int loop,
    struct gfx_vertex *dst, int nverts, const int *source);
/* USAGE:
	anim_Sample(&walk, t, 1, mesh.verts, mesh.nverts, mesh.source);
	gfx_UpdateMesh(handle, mesh.verts, mesh.nverts);
   NOTE:
	writes positions (and normals if the file has them) into dst and
	leaves u and v alone, so you sample straight into the vertex array
	that came out of obj_Parse().
	`source` is obj_mesh.source: the frames are indexed by the .obj's
	own vertex list, and this maps a drawing vertex back to it.  Pass 0
	when the two are already one to one.
*/

float anim_Length(const struct anim *a);	/* seconds */

#endif /* ANIM_H_SENTRY */
