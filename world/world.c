#include "world.h"

void
wld_Clear(struct world *w)
{
	w->nsectors = 0;
	w->nportals = 0;
	w->nsolids = 0;
}

int
wld_AddSector(struct world *w, const struct aabb *bounds, unsigned int mesh,
    unsigned int tex)
{
	struct wld_sector *s;

	if (w->nsectors >= WLD_MAX_SECTORS)
		return -1;
	s = &w->sector[w->nsectors];
	s->bounds = *bounds;
	s->mesh = mesh;
	s->tex = tex;
	s->first_solid = w->nsolids;
	s->nsolids = 0;
	w->nsectors++;
	return w->nsectors - 1;
}

int
wld_AddPortal(struct world *w, int from, int to, const vector a,
    const vector b, const vector c, const vector d)
{
	struct wld_portal *p;

	if (w->nportals >= WLD_MAX_PORTALS)
		return -1;
	if (from < 0 || from >= w->nsectors || to < 0 || to >= w->nsectors)
		return -1;

	p = &w->portal[w->nportals];
	VEC_ASSIGMENT(a, p->corner[0]);
	VEC_ASSIGMENT(b, p->corner[1]);
	VEC_ASSIGMENT(c, p->corner[2]);
	VEC_ASSIGMENT(d, p->corner[3]);
	p->from = from;
	p->to = to;
	w->nportals++;
	return w->nportals - 1;
}

int
wld_AddSolid(struct world *w, int sector, const struct aabb *box)
{
	if (w->nsolids >= WLD_MAX_SOLIDS)
		return -1;
	if (sector < 0 || sector >= w->nsectors)
		return -1;
	/*  Solids must be added sector by sector, in order: a sector owns a
	 *  contiguous run of them.  That keeps wld_SolidsNear() a memcpy
	 *  instead of a search.
	 */
	if (sector != w->nsectors - 1)
		return -1;

	w->solid[w->nsolids] = *box;
	w->nsolids++;
	w->sector[sector].nsolids++;
	return w->nsolids - 1;
}

int
wld_SectorAt(const struct world *w, const vector p)
{
	int i;

	for (i = 0; i < w->nsectors; i++) {
		if (coll_PointAabb(p, &w->sector[i].bounds))
			return i;
	}
	return -1;
}

static int
portal_in_frustum(const struct frustum *f, const struct wld_portal *p)
{
	struct aabb box;
	int i;

	VEC_ASSIGMENT(p->corner[0], box.min);
	VEC_ASSIGMENT(p->corner[0], box.max);
	for (i = 1; i < 4; i++)
		coll_AabbGrow(&box, p->corner[i]);

	/*  A portal is flat, and a flat box fails plane tests on the thin
	 *  axis for no good reason.  Give it a centimetre of thickness.
	 */
	for (i = 0; i < 3; i++) {
		box.min[i] -= 0.01f;
		box.max[i] += 0.01f;
	}
	return fr_test_aabb(f, box.min, box.max);
}

int
wld_Visible(const struct world *w, int from, const float viewproj[16],
    int *out, int max)
{
	struct frustum f;
	unsigned char seen[WLD_MAX_SECTORS];
	int queue[WLD_MAX_SECTORS];
	int head;
	int tail;
	int n;
	int i;
	int here;
	const struct wld_portal *p;

	if (w == 0 || out == 0 || max <= 0)
		return 0;
	fr_from_matrix(&f, viewproj);

	for (i = 0; i < WLD_MAX_SECTORS; i++)
		seen[i] = 0;
	n = 0;

	if (from < 0 || from >= w->nsectors) {
		for (i = 0; i < w->nsectors && n < max; i++) {
			if (fr_test_aabb(&f, w->sector[i].bounds.min,
			    w->sector[i].bounds.max))
				out[n++] = i;
		}
		return n;
	}

	head = 0;
	tail = 0;
	queue[tail++] = from;
	seen[from] = 1;

	while (head < tail && n < max) {
		here = queue[head++];
		out[n++] = here;

		for (i = 0; i < w->nportals; i++) {
			p = &w->portal[i];
			if (p->from != here)
				continue;
			if (seen[p->to])
				continue;
			if (!portal_in_frustum(&f, p))
				continue;
			if (!fr_test_aabb(&f, w->sector[p->to].bounds.min,
			    w->sector[p->to].bounds.max))
				continue;
			seen[p->to] = 1;
			if (tail < WLD_MAX_SECTORS)
				queue[tail++] = p->to;
		}
	}
	return n;
}

int
wld_SolidsNear(const struct world *w, const int *sectors, int nsectors,
    struct aabb *out, int max)
{
	int n;
	int i;
	int j;
	const struct wld_sector *s;

	n = 0;
	for (i = 0; i < nsectors; i++) {
		if (sectors[i] < 0 || sectors[i] >= w->nsectors)
			continue;
		s = &w->sector[sectors[i]];
		for (j = 0; j < s->nsolids && n < max; j++)
			out[n++] = w->solid[s->first_solid + j];
	}
	return n;
}
