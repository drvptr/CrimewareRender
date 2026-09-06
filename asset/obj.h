/*
 *	@(#)obj.h	1.0
 *
 *  Wavefront .obj to an interleaved vertex array and an index array, which
 *  is what the GPU wants and what the collision code wants.  Bytes in,
 *  arena out; no files, no malloc.
 *
 *  Every vertex remembers which "v" line of the file it came from, in
 *  out->source.  That is what makes vertex animation work: a .van file is
 *  indexed by the file's own vertex list, which is stable and readable,
 *  not by whatever order the dedup happened to produce.
 *
 *  usemtl becomes a group: a range of indices and a name.  The game maps
 *  the name to a texture, because only the game knows where its textures
 *  live.  Without groups you cannot texture a room, so they are not
 *  optional and not hidden.
 *
 *  Not supported on purpose: free form surfaces, .mtl files, smoothing
 *  groups.  Missing normals are computed by averaging face normals, which
 *  is what Gouraud shading needs.
 */
#ifndef OBJ_H_SENTRY
#define OBJ_H_SENTRY

#include "../gfx/gfx.h"
#include "../core/arena.h"

#define OBJ_MAX_GROUPS 64
#define OBJ_NAME_LEN 32

struct obj_group {
	char name[OBJ_NAME_LEN];
	int first;	/* first index	*/
	int count;	/* index count	*/
};

struct obj_mesh {
	struct gfx_vertex *verts;
	int *source;	/* which "v" line each vertex came from	*/
	int nverts;
	unsigned short *index;
	int nindex;
	struct obj_group groups[OBJ_MAX_GROUPS];
	int ngroups;
	float min[3];
	float max[3];
};

int obj_Parse(struct obj_mesh *out, const void *data, long len,
    struct arena *a);
/* RETURN VALUE: 0 on success, -1 on a parse error or a full arena.
 * NOTE:  more than 65535 unique vertices is an error, not a silent wrap.
 *	  Split the model; a PlayStation 2 could not draw it in one call
 *	  either.
 */

long obj_EstimateBytes(const void *data, long len);
/* NOTE:  how much arena obj_Parse() will want.  Overestimates a little. */

#endif /* OBJ_H_SENTRY */
