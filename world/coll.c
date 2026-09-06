#include <math.h>
#include "coll.h"

void
coll_MakeAabb(const vector centre, const vector half, struct aabb *out)
{
	int i;

	for (i = 0; i < 3; i++) {
		out->min[i] = centre[i] - half[i];
		out->max[i] = centre[i] + half[i];
	}
}

void
coll_AabbCentre(const struct aabb *b, vector out)
{
	int i;

	for (i = 0; i < 3; i++)
		out[i] = (b->min[i] + b->max[i]) * 0.5f;
}

void
coll_AabbGrow(struct aabb *b, const vector p)
{
	const float *v;
	int i;

	v = p;
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
coll_PointAabb(const vector p, const struct aabb *b)
{
	const float *v;
	int i;

	v = p;
	for (i = 0; i < 3; i++) {
		if (v[i] < b->min[i] || v[i] > b->max[i])
			return 0;
	}
	return 1;
}

int
coll_SphereAabb(const vector centre, float radius, const struct aabb *b,
    vector push_out)
{
	const float *c;
	float nearest[3];
	float d[3];
	float dist;
	int i;

	c = centre;
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
		vec_scalar_mul(d, (radius - dist) / dist, push_out);
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
		VEC_ZERO(push_out);
		push_out[axis] = (best + radius) * (float)sign;
	}
	return 1;
}

int
coll_RayAabb(const vector origin, const vector dir, const struct aabb *b,
    float *t_out)
{
	const float *o;
	const float *d;
	float tmin;
	float tmax;
	float t1;
	float t2;
	float swap;
	int i;

	o = origin;
	d = dir;

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
coll_RayTri(const vector origin, const vector dir, const vector a,
    const vector b, const vector c, float *t_out)
{
	vector e1;
	vector e2;
	vector pv;
	vector tv;
	vector qv;
	float det;
	float inv;
	float u;
	float v;
	float t;

	vec_sub(b, a, e1);
	vec_sub(c, a, e2);
	vec_cross(dir, e2, pv);
	det = vec_dot(e1, pv);
	if (det > -0.000001f && det < 0.000001f)
		return 0;

	inv = 1.0f / det;
	vec_sub(origin, a, tv);
	u = vec_dot(tv, pv) * inv;
	if (u < 0.0f || u > 1.0f)
		return 0;

	vec_cross(tv, e1, qv);
	v = vec_dot(dir, qv) * inv;
	if (v < 0.0f || u + v > 1.0f)
		return 0;

	t = vec_dot(e2, qv) * inv;
	if (t < 0.0f)
		return 0;
	if (t_out != 0)
		*t_out = t;
	return 1;
}

int
coll_RayMesh(const vector origin, const vector dir,
    const struct gfx_vertex *verts, const unsigned short *index, int nindex,
    const float model[16], float *t_out)
{
	int i;
	int found;
	float best;
	float t;
	vector a;
	vector b;
	vector c;

	found = 0;
	best = 1e30f;
	for (i = 0; i + 2 < nindex; i += 3) {
		VEC_SET(a, verts[index[i]].x, verts[index[i]].y,
		    verts[index[i]].z);
		VEC_SET(b, verts[index[i + 1]].x, verts[index[i + 1]].y,
		    verts[index[i + 1]].z);
		VEC_SET(c, verts[index[i + 2]].x, verts[index[i + 2]].y,
		    verts[index[i + 2]].z);
		if (model != 0) {
			m4_mul_point(model, a, a);
			m4_mul_point(model, b, b);
			m4_mul_point(model, c, c);
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
sweep_one(const vector centre, const vector half, const vector move,
    const struct aabb *solid, float *t_out, int *axis_out)
{
	struct aabb grown;
	const float *o;
	const float *d;
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
	for (i = 0; i < 3; i++) {
		grown.min[i] -= half[i];
		grown.max[i] += half[i];
	}

	o = centre;
	d = move;

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

void
coll_MoveAabb(const struct aabb *body, const vector move_in,
    const struct aabb *solids, int nsolids, int *hit_ground, vector out)
{
	vector centre;
	vector half;
	vector move;
	vector step;
	float best_t;
	int best_axis;
	float t;
	int axis;
	int i;
	int pass;

	coll_AabbCentre(body, centre);
	for (i = 0; i < 3; i++)
		half[i] = (body->max[i] - body->min[i]) * 0.5f;
	VEC_ASSIGMENT(move_in, move);
	VEC_ZERO(out);
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

		vec_scalar_mul(move, best_t, step);
		vec_add(centre, step, centre);
		vec_add(out, step, out);

		if (best_axis < 0)
			break;

		if (best_axis == Y && move[Y] < 0.0f && hit_ground != 0)
			*hit_ground = 1;
		move[best_axis] = 0.0f;

		vec_scalar_mul(move, 1.0f - best_t, move);
	}
}
