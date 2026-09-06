/*
 *	@(#)coll.h	1.0
 *
 *  Collision, and nothing that could be called physics.  There is no mass,
 *  no restitution, no solver and no time step: you ask "did this hit that"
 *  and "how far can I move before I hit something", and the answers are
 *  exact and stateless.
 *
 *  This is what a game of this size actually needs.  Rigid body dynamics
 *  is a project in itself and it makes level design harder, not easier.
 */
#ifndef COLL_H_SENTRY
#define COLL_H_SENTRY

#include "../core/m3.h"
#include "../gfx/gfx.h"

struct aabb {
	float min[3];
	float max[3];
};

struct aabb coll_MakeAabb(struct vec3 centre, struct vec3 half);
struct vec3 coll_AabbCentre(const struct aabb *b);
void coll_AabbGrow(struct aabb *b, struct vec3 p);

int coll_AabbAabb(const struct aabb *a, const struct aabb *b);
int coll_PointAabb(struct vec3 p, const struct aabb *b);
int coll_SphereAabb(struct vec3 centre, float radius, const struct aabb *b,
    struct vec3 *push_out);
/* NOTE:  push_out (may be 0) receives the shortest vector that separates
 *	  the sphere from the box.  That is the whole of "do not walk into
 *	  the wall" for a character that is a sphere.
 */

int coll_RayAabb(struct vec3 origin, struct vec3 dir, const struct aabb *b,
    float *t_out);
int coll_RayTri(struct vec3 origin, struct vec3 dir, struct vec3 a,
    struct vec3 b, struct vec3 c, float *t_out);

int coll_RayMesh(struct vec3 origin, struct vec3 dir,
    const struct gfx_vertex *verts, const unsigned short *index, int nindex,
    const float model[16], float *t_out);
/* NOTE:  brute force over the triangles.  For a gun shot against one
 *	  object per frame that is free; do not call it for every bullet
 *	  against the whole level without a sector to narrow it down first.
 */

struct vec3 coll_MoveAabb(struct aabb body, struct vec3 move,
    const struct aabb *solids, int nsolids, int *hit_ground);
/* USAGE:
	pos = v3_add(pos, coll_MoveAabb(body, wish, solids, n, &on_floor));
   NOTE:
	swept box against static boxes, three passes, sliding along whatever
	it hits.  hit_ground (may be 0) is set when the blocked direction
	was downward, which is all a jump needs to know.
*/

#endif /* COLL_H_SENTRY */
