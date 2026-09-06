#include <math.h>
#include "coll.h"

struct aabb
coll_MakeAabb(struct vec3 centre, struct vec3 half)
{
	struct aabb b;

	b.min[0] = centre.x - half.x;
	b.min[1] = centre.y - half.y;
	b.min[2] = centre.z - half.z;
	b.max[0] = centre.x + half.x;
	b.max[1] = centre.y + half.y;
	b.max[2] = centre.z + half.z;
	return b;
}

struct vec3
coll_AabbCentre(const struct aabb *b)
{
	return v3((b->min[0] + b->max[0]) * 0.5f,
	    (b->min[1] + b->max[1]) * 0.5f,
	    (b->min[2] + b->max[2]) * 0.5f);
}

void
coll_AabbGrow(struct aabb *b, struct vec3 p)
{
	float v[3];
	int i;

	v[0] = p.x;
	v[1] = p.y;
	v[2] = p.z;
	for (i = 0; i < 3; i++) {
		if (v[i] < b->min[i])
			b->min[i] = v[i];
		if (v[i] > b->max[i])
			b->max[i] = v[i];
	}
}

int
coll_AabbAabb(const struct aabb *a, const struct aabb *b)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (a->max[i] < b->min[i])
			return 0;
		if (a->min[i] > b->max[i])
			return 0;
	}
	return 1;
}

int
coll_PointAabb(struct vec3 p, const struct aabb *b)
{
	float v[3];
	int i;

	v[0] = p.x;
	v[1] = p.y;
	v[2] = p.z;
	for (i = 0; i < 3; i++) {
		if (v[i] < b->min[i] || v[i] > b->max[i])
			return 0;
	}
	return 1;
}

int
coll_SphereAabb(struct vec3 centre, float radius, const struct aabb *b,
    struct vec3 *push_out)
{
	float c[3];
	float nearest[3];
	float d[3];
	float dist;
	int i;

	c[0] = centre.x;
	c[1] = centre.y;
	c[2] = centre.z;
	for (i = 0; i < 3; i++)
		nearest[i] = m3_clampf(c[i], b->min[i], b->max[i]);

	for (i = 0; i < 3; i++)
		d[i] = c[i] - nearest[i];
	dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);

	if (dist > radius)
		return 0;
	if (push_out == 0)
		return 1;

	if (dist > 0.000001f) {
		*push_out = v3_scale(v3(d[0], d[1], d[2]),
		    (radius - dist) / dist);
		return 1;
	}

	/*  The centre is inside the box: push out along the shallowest
	 *  face, which is the only choice that does not teleport anyone.
	 */
	{
		float best;
		int axis;
		int sign;
		float low;
		float high;

		best = 1e30f;
		axis = 1;
		sign = 1;
		for (i = 0; i < 3; i++) {
			low = c[i] - b->min[i];
			high = b->max[i] - c[i];
			if (low < best) {
				best = low;
				axis = i;
				sign = -1;
			}
			if (high < best) {
				best = high;
				axis = i;
				sign = 1;
			}
		}
		*push_out = v3(0.0f, 0.0f, 0.0f);
		if (axis == 0)
			push_out->x = (best + radius) * (float)sign;
		else if (axis == 1)
			push_out->y = (best + radius) * (float)sign;
		else
			push_out->z = (best + radius) * (float)sign;
	}
	return 1;
}

int
coll_RayAabb(struct vec3 origin, struct vec3 dir, const struct aabb *b,
    float *t_out)
{
	float o[3];
	float d[3];
	float tmin;
	float tmax;
	float t1;
	float t2;
	float swap;
	int i;

	o[0] = origin.x;
	o[1] = origin.y;
	o[2] = origin.z;
	d[0] = dir.x;
	d[1] = dir.y;
	d[2] = dir.z;

	tmin = 0.0f;
	tmax = 1e30f;
	for (i = 0; i < 3; i++) {
		if (d[i] > -0.000001f && d[i] < 0.000001f) {
			if (o[i] < b->min[i] || o[i] > b->max[i])
				return 0;
			continue;
		}
		t1 = (b->min[i] - o[i]) / d[i];
		t2 = (b->max[i] - o[i]) / d[i];
		if (t1 > t2) {
			swap = t1;
			t1 = t2;
			t2 = swap;
		}
		if (t1 > tmin)
			tmin = t1;
		if (t2 < tmax)
			tmax = t2;
		if (tmin > tmax)
			return 0;
	}
	if (t_out != 0)
		*t_out = tmin;
	return 1;
}

int
coll_RayTri(struct vec3 origin, struct vec3 dir, struct vec3 a, struct vec3 b,
    struct vec3 c, float *t_out)
{
	struct vec3 e1;
	struct vec3 e2;
	struct vec3 pv;
	struct vec3 tv;
	struct vec3 qv;
	float det;
	float inv;
	float u;
	float v;
	float t;

	e1 = v3_sub(b, a);
	e2 = v3_sub(c, a);
	pv = v3_cross(dir, e2);
	det = v3_dot(e1, pv);
	if (det > -0.000001f && det < 0.000001f)
		return 0;

	inv = 1.0f / det;
	tv = v3_sub(origin, a);
	u = v3_dot(tv, pv) * inv;
	if (u < 0.0f || u > 1.0f)
		return 0;

	qv = v3_cross(tv, e1);
	v = v3_dot(dir, qv) * inv;
	if (v < 0.0f || u + v > 1.0f)
		return 0;

	t = v3_dot(e2, qv) * inv;
	if (t < 0.0f)
		return 0;
	if (t_out != 0)
		*t_out = t;
	return 1;
}

int
coll_RayMesh(struct vec3 origin, struct vec3 dir,
    const struct gfx_vertex *verts, const unsigned short *index, int nindex,
    const float model[16], float *t_out)
{
	int i;
	int found;
	float best;
	float t;
	struct vec3 a;
	struct vec3 b;
	struct vec3 c;

	found = 0;
	best = 1e30f;
	for (i = 0; i + 2 < nindex; i += 3) {
		a = v3(verts[index[i]].x, verts[index[i]].y,
		    verts[index[i]].z);
		b = v3(verts[index[i + 1]].x, verts[index[i + 1]].y,
		    verts[index[i + 1]].z);
		c = v3(verts[index[i + 2]].x, verts[index[i + 2]].y,
		    verts[index[i + 2]].z);
		if (model != 0) {
			a = m4_mul_point(model, a);
			b = m4_mul_point(model, b);
			c = m4_mul_point(model, c);
		}
		if (coll_RayTri(origin, dir, a, b, c, &t) && t < best) {
			best = t;
			found = 1;
		}
	}
	if (found && t_out != 0)
		*t_out = best;
	return found;
}

/*  Swept box against one static box, by the Minkowski trick: grow the
 *  static box by the mover's half size and cast a ray from the mover's
 *  centre.  One ray test replaces the whole separating axis dance.
 */
static int
sweep_one(struct vec3 centre, struct vec3 half, struct vec3 move,
    const struct aabb *solid, float *t_out, int *axis_out)
{
	struct aabb grown;
	float o[3];
	float d[3];
	float t1;
	float t2;
	float swap;
	float tmin;
	float tmax;
	int i;
	int axis;

	for (i = 0; i < 3; i++) {
		grown.min[i] = solid->min[i];
		grown.max[i] = solid->max[i];
	}
	grown.min[0] -= half.x;
	grown.max[0] += half.x;
	grown.min[1] -= half.y;
	grown.max[1] += half.y;
	grown.min[2] -= half.z;
	grown.max[2] += half.z;

	o[0] = centre.x;
	o[1] = centre.y;
	o[2] = centre.z;
	d[0] = move.x;
	d[1] = move.y;
	d[2] = move.z;

	tmin = 0.0f;
	tmax = 1.0f;
	axis = -1;
	for (i = 0; i < 3; i++) {
		if (d[i] > -0.000001f && d[i] < 0.000001f) {
			if (o[i] < grown.min[i] || o[i] > grown.max[i])
				return 0;
			continue;
		}
		t1 = (grown.min[i] - o[i]) / d[i];
		t2 = (grown.max[i] - o[i]) / d[i];
		if (t1 > t2) {
			swap = t1;
			t1 = t2;
			t2 = swap;
		}
		if (t1 > tmin) {
			tmin = t1;
			axis = i;
		}
		if (t2 < tmax)
			tmax = t2;
		if (tmin > tmax)
			return 0;
	}
	if (axis < 0)
		return 0;	/* already overlapping, not a new hit */

	*t_out = tmin;
	*axis_out = axis;
	return 1;
}

struct vec3
coll_MoveAabb(struct aabb body, struct vec3 move, const struct aabb *solids,
    int nsolids, int *hit_ground)
{
	struct vec3 centre;
	struct vec3 half;
	struct vec3 total;
	struct vec3 step;
	float best_t;
	int best_axis;
	float t;
	int axis;
	int i;
	int pass;

	centre = coll_AabbCentre(&body);
	half = v3((body.max[0] - body.min[0]) * 0.5f,
	    (body.max[1] - body.min[1]) * 0.5f,
	    (body.max[2] - body.min[2]) * 0.5f);
	total = v3(0.0f, 0.0f, 0.0f);
	if (hit_ground != 0)
		*hit_ground = 0;

	/*  Three passes: hit a wall, keep the part of the motion that runs
	 *  along it, try again.  Three is enough for a corner; a fourth
	 *  only matters in geometry a level designer should not build.
	 */
	for (pass = 0; pass < 3; pass++) {
		best_t = 1.0f;
		best_axis = -1;
		for (i = 0; i < nsolids; i++) {
			if (sweep_one(centre, half, move, &solids[i], &t,
			    &axis) && t < best_t) {
				best_t = t;
				best_axis = axis;
			}
		}

		/*  Stop a hair short of the surface, or the next frame starts
		 *  exactly on it and floating point decides which side.
		 */
		if (best_axis >= 0) {
			best_t -= 0.0005f;
			if (best_t < 0.0f)
				best_t = 0.0f;
		}

		step = v3_scale(move, best_t);
		centre = v3_add(centre, step);
		total = v3_add(total, step);

		if (best_axis < 0)
			break;

		if (best_axis == 0)
			move.x = 0.0f;
		else if (best_axis == 1) {
			if (move.y < 0.0f && hit_ground != 0)
				*hit_ground = 1;
			move.y = 0.0f;
		} else
			move.z = 0.0f;

		move = v3_scale(move, 1.0f - best_t);
	}
	return total;
}
