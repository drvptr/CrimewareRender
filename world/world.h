/*
 *	@(#)world.h	1.0
 *
 *  Sectors and portals.  A sector is a convex-ish region with its own mesh
 *  and its own solid boxes; a portal is a quad that says "through here you
 *  can see sector N".  Drawing starts in the sector the camera is in and
 *  walks outward through portals that survive the frustum test.
 *
 *  WHY NOT BSP.  A BSP tree solves a problem OpenGL already solved: it
 *  orders polygons back to front so you can draw without a depth buffer,
 *  and it does so by splitting geometry at build time.  With a z-buffer the
 *  ordering is free and the splitting is pure loss.  What still costs money
 *  is deciding what NOT to draw, and for that a BSP is only useful together
 *  with a PVS, which means writing a compiler that does brush CSG, leaf
 *  extraction and portal flood fill offline.  That compiler is a bigger
 *  program than this engine.  Portals give you the same "only what is
 *  actually visible" at run time, in the 200 lines you are reading, and
 *  they let a level be a pile of .obj files instead of a compiled artefact.
 *
 *  When a portal graph is wrong the picture has holes.  When a BSP compiler
 *  is wrong you get nothing at all and no idea why.
 */
#ifndef WORLD_H_SENTRY
#define WORLD_H_SENTRY

#include "../core/m3.h"
#include "coll.h"

#define WLD_MAX_SECTORS 256
#define WLD_MAX_PORTALS 1024
#define WLD_MAX_SOLIDS 4096

struct wld_portal {
	vector corner[4];	/* counter clockwise, seen from `from`	*/
	int from;
	int to;
};

struct wld_sector {
	struct aabb bounds;
	unsigned int mesh;
	unsigned int tex;
	int first_solid;
	int nsolids;
};

struct world {
	struct wld_sector sector[WLD_MAX_SECTORS];
	int nsectors;
	struct wld_portal portal[WLD_MAX_PORTALS];
	int nportals;
	struct aabb solid[WLD_MAX_SOLIDS];
	int nsolids;
};

void wld_Clear(struct world *w);
int wld_AddSector(struct world *w, const struct aabb *bounds,
    unsigned int mesh, unsigned int tex);
int wld_AddPortal(struct world *w, int from, int to, const vector a,
    const vector b, const vector c, const vector d);
int wld_AddSolid(struct world *w, int sector, const struct aabb *box);

int wld_SectorAt(const struct world *w, const vector p);
/* RETURN VALUE: sector index, or -1 when the point is in none of them. */

int wld_Visible(const struct world *w, int from, const float viewproj[16],
    int *out, int max);
/* USAGE:
	int list[64];
	int n = wld_Visible(&world, here, viewproj, list, 64);
   NOTE:
	breadth first walk from `from` through portals that are inside the
	frustum.  With from < 0 it falls back to testing every sector's
	bounds, which is what you want outdoors.
	The order returned is near to far, which is also the order you want
	to draw in so the depth test rejects early.
*/

int wld_SolidsNear(const struct world *w, const int *sectors, int nsectors,
    struct aabb *out, int max);
/* NOTE:  the solid boxes belonging to a set of sectors, ready to hand to
 *	  coll_MoveAabb().  Collision follows visibility: if you cannot see
 *	  it you are not standing on it either.
 */

#endif /* WORLD_H_SENTRY */
