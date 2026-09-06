/*
 *	@(#)obj.c	1.0
 *
 *  Two passes.  The first counts, so every array is allocated once at its
 *  real size.  The second fills them and folds duplicate v/vt/vn triples
 *  through a hash table, because .obj repeats a corner for every face that
 *  touches it and the GPU should not.
 *
 *  The numbers are parsed here rather than by strtod() on purpose: strtod()
 *  reads the decimal point from the locale, and an exporter that wrote
 *  "1.5" will silently become 1 on a machine set to ru_RU.  That bug is
 *  hard to find and this function is thirty lines.
 */
#include <math.h>
#include "obj.h"

struct parser {
	const char *p;
	const char *end;
};

static int
at_end(struct parser *s)
{
	return s->p >= s->end;
}

static void
skip_blanks(struct parser *s)
{
	while (!at_end(s) && (*s->p == ' ' || *s->p == '\t' || *s->p == '\r'))
		s->p++;
}

static void
skip_line(struct parser *s)
{
	while (!at_end(s) && *s->p != '\n')
		s->p++;
	if (!at_end(s))
		s->p++;
}

static int
is_digit(int c)
{
	return c >= '0' && c <= '9';
}

static int
parse_int(struct parser *s, int *out)
{
	int sign;
	int value;
	int any;

	skip_blanks(s);
	sign = 1;
	value = 0;
	any = 0;
	if (!at_end(s) && (*s->p == '-' || *s->p == '+')) {
		if (*s->p == '-')
			sign = -1;
		s->p++;
	}
	while (!at_end(s) && is_digit((unsigned char)*s->p)) {
		value = value * 10 + (*s->p - '0');
		s->p++;
		any = 1;
	}
	if (!any)
		return 0;
	*out = value * sign;
	return 1;
}

static int
parse_float(struct parser *s, float *out)
{
	double sign;
	double value;
	double frac;
	int any;
	int exp_sign;
	int exp_value;
	int i;

	skip_blanks(s);
	sign = 1.0;
	value = 0.0;
	any = 0;

	if (!at_end(s) && (*s->p == '-' || *s->p == '+')) {
		if (*s->p == '-')
			sign = -1.0;
		s->p++;
	}
	while (!at_end(s) && is_digit((unsigned char)*s->p)) {
		value = value * 10.0 + (double)(*s->p - '0');
		s->p++;
		any = 1;
	}
	if (!at_end(s) && *s->p == '.') {
		s->p++;
		frac = 0.1;
		while (!at_end(s) && is_digit((unsigned char)*s->p)) {
			value += (double)(*s->p - '0') * frac;
			frac *= 0.1;
			s->p++;
			any = 1;
		}
	}
	if (!any)
		return 0;

	if (!at_end(s) && (*s->p == 'e' || *s->p == 'E')) {
		s->p++;
		exp_sign = 1;
		exp_value = 0;
		if (!at_end(s) && (*s->p == '-' || *s->p == '+')) {
			if (*s->p == '-')
				exp_sign = -1;
			s->p++;
		}
		while (!at_end(s) && is_digit((unsigned char)*s->p)) {
			exp_value = exp_value * 10 + (*s->p - '0');
			s->p++;
		}
		for (i = 0; i < exp_value; i++) {
			if (exp_sign > 0)
				value *= 10.0;
			else
				value *= 0.1;
		}
	}

	*out = (float)(sign * value);
	return 1;
}

static int
keyword_is(struct parser *s, const char *word)
{
	const char *p;
	const char *w;

	p = s->p;
	w = word;
	while (*w != 0) {
		if (p >= s->end || *p != *w)
			return 0;
		p++;
		w++;
	}
	if (p < s->end && *p != ' ' && *p != '\t' && *p != '\n' &&
	    *p != '\r')
		return 0;
	s->p = p;
	return 1;
}

/*  One corner of a face: v, v/vt, v//vn or v/vt/vn, 1 based, and negative
 *  means "counting back from what we have read so far".
 */
static int
parse_corner(struct parser *s, int nv, int nt, int nn, int out[3])
{
	int value;

	out[0] = -1;
	out[1] = -1;
	out[2] = -1;

	skip_blanks(s);
	if (at_end(s) || *s->p == '\n')
		return 0;
	if (!parse_int(s, &value))
		return 0;
	out[0] = value > 0 ? value - 1 : nv + value;

	if (!at_end(s) && *s->p == '/') {
		s->p++;
		if (!at_end(s) && *s->p != '/') {
			if (parse_int(s, &value))
				out[1] = value > 0 ? value - 1 : nt + value;
		}
		if (!at_end(s) && *s->p == '/') {
			s->p++;
			if (parse_int(s, &value))
				out[2] = value > 0 ? value - 1 : nn + value;
		}
	}
	return 1;
}

static void
count_pass(const void *data, long len, int *nv, int *nt, int *nn, int *ntri)
{
	struct parser s;
	int corners;
	int dummy[3];

	s.p = (const char *)data;
	s.end = (const char *)data + len;
	*nv = 0;
	*nt = 0;
	*nn = 0;
	*ntri = 0;

	while (!at_end(&s)) {
		skip_blanks(&s);
		if (keyword_is(&s, "v"))
			(*nv)++;
		else if (keyword_is(&s, "vt"))
			(*nt)++;
		else if (keyword_is(&s, "vn"))
			(*nn)++;
		else if (keyword_is(&s, "f")) {
			corners = 0;
			while (parse_corner(&s, 0, 0, 0, dummy))
				corners++;
			if (corners >= 3)
				*ntri += corners - 2;
		}
		skip_line(&s);
	}
}

long
obj_EstimateBytes(const void *data, long len)
{
	int nv;
	int nt;
	int nn;
	int ntri;
	long bytes;

	count_pass(data, len, &nv, &nt, &nn, &ntri);
	bytes = (long)ntri * 3 * (long)sizeof(unsigned short);
	bytes += (long)ntri * 3 * (long)sizeof(struct gfx_vertex);
	bytes += (long)nv * 3 * (long)sizeof(float);
	bytes += (long)nt * 2 * (long)sizeof(float);
	bytes += (long)nn * 3 * (long)sizeof(float);
	bytes += (long)ntri * 3 * 8 * (long)sizeof(int);	/* hash table */
	return bytes + 4096;
}

static unsigned int
hash_triple(int a, int b, int c)
{
	unsigned int h;

	h = 2166136261u;
	h = (h ^ (unsigned int)a) * 16777619u;
	h = (h ^ (unsigned int)b) * 16777619u;
	h = (h ^ (unsigned int)c) * 16777619u;
	return h;
}

struct build {
	struct obj_mesh *m;
	float *pos;
	float *uv;
	float *nrm;
	int nv;
	int nt;
	int nn;
	int *table;	/* vertex index + 1, 0 is empty	*/
	int *keys;	/* three ints per slot		*/
	int mask;
};

static int
intern(struct build *b, const int key[3])
{
	unsigned int slot;
	int at;
	struct gfx_vertex *v;

	slot = hash_triple(key[0], key[1], key[2]) & (unsigned int)b->mask;
	while (b->table[slot] != 0) {
		at = b->table[slot] - 1;
		if (b->keys[slot * 3 + 0] == key[0] &&
		    b->keys[slot * 3 + 1] == key[1] &&
		    b->keys[slot * 3 + 2] == key[2])
			return at;
		slot = (slot + 1) & (unsigned int)b->mask;
	}

	at = b->m->nverts;
	if (at >= 65535)
		return -1;
	v = &b->m->verts[at];
	b->m->source[at] = key[0];

	if (key[0] >= 0 && key[0] < b->nv) {
		v->x = b->pos[key[0] * 3 + 0];
		v->y = b->pos[key[0] * 3 + 1];
		v->z = b->pos[key[0] * 3 + 2];
	}
	if (key[1] >= 0 && key[1] < b->nt) {
		v->u = b->uv[key[1] * 2 + 0];
		v->v = b->uv[key[1] * 2 + 1];
	}
	if (key[2] >= 0 && key[2] < b->nn) {
		v->nx = b->nrm[key[2] * 3 + 0];
		v->ny = b->nrm[key[2] * 3 + 1];
		v->nz = b->nrm[key[2] * 3 + 2];
	}

	b->table[slot] = at + 1;
	b->keys[slot * 3 + 0] = key[0];
	b->keys[slot * 3 + 1] = key[1];
	b->keys[slot * 3 + 2] = key[2];
	b->m->nverts++;
	return at;
}

static void
smooth_normals(struct obj_mesh *m)
{
	int i;
	struct vec3 a;
	struct vec3 b;
	struct vec3 c;
	struct vec3 n;
	struct gfx_vertex *v;
	float len;

	for (i = 0; i < m->nverts; i++) {
		m->verts[i].nx = 0.0f;
		m->verts[i].ny = 0.0f;
		m->verts[i].nz = 0.0f;
	}
	for (i = 0; i + 2 < m->nindex; i += 3) {
		v = m->verts;
		a = v3(v[m->index[i]].x, v[m->index[i]].y, v[m->index[i]].z);
		b = v3(v[m->index[i + 1]].x, v[m->index[i + 1]].y,
		    v[m->index[i + 1]].z);
		c = v3(v[m->index[i + 2]].x, v[m->index[i + 2]].y,
		    v[m->index[i + 2]].z);
		n = v3_cross(v3_sub(b, a), v3_sub(c, a));
		v[m->index[i]].nx += n.x;
		v[m->index[i]].ny += n.y;
		v[m->index[i]].nz += n.z;
		v[m->index[i + 1]].nx += n.x;
		v[m->index[i + 1]].ny += n.y;
		v[m->index[i + 1]].nz += n.z;
		v[m->index[i + 2]].nx += n.x;
		v[m->index[i + 2]].ny += n.y;
		v[m->index[i + 2]].nz += n.z;
	}
	for (i = 0; i < m->nverts; i++) {
		v = &m->verts[i];
		len = sqrtf(v->nx * v->nx + v->ny * v->ny + v->nz * v->nz);
		if (len < 0.000001f) {
			v->ny = 1.0f;
			continue;
		}
		v->nx /= len;
		v->ny /= len;
		v->nz /= len;
	}
}

static void
open_group(struct obj_mesh *m, struct parser *s)
{
	int i;

	if (m->ngroups > 0)
		m->groups[m->ngroups - 1].count =
		    m->nindex - m->groups[m->ngroups - 1].first;
	if (m->ngroups >= OBJ_MAX_GROUPS)
		return;

	skip_blanks(s);
	i = 0;
	while (!at_end(s) && *s->p != '\n' && *s->p != '\r' &&
	    i < OBJ_NAME_LEN - 1) {
		m->groups[m->ngroups].name[i] = *s->p;
		s->p++;
		i++;
	}
	m->groups[m->ngroups].name[i] = 0;
	m->groups[m->ngroups].first = m->nindex;
	m->groups[m->ngroups].count = 0;
	m->ngroups++;
}

int
obj_Parse(struct obj_mesh *out, const void *data, long len, struct arena *a)
{
	struct parser s;
	struct build b;
	int nv;
	int nt;
	int nn;
	int ntri;
	int table_size;
	long verts_offset;
	int corner[3];
	int fan[3];
	int corners;
	int at;
	int i;
	float value;

	if (out == 0 || data == 0 || a == 0 || len <= 0)
		return -1;

	for (i = 0; i < 3; i++) {
		out->min[i] = 0.0f;
		out->max[i] = 0.0f;
	}
	out->nverts = 0;
	out->nindex = 0;
	out->ngroups = 0;

	count_pass(data, len, &nv, &nt, &nn, &ntri);
	if (ntri <= 0 || nv <= 0)
		return -1;

	table_size = 1;
	while (table_size < ntri * 6)
		table_size *= 2;

	/*  Index array first at its exact size, then the vertex array at its
	 *  upper bound, then everything temporary.  When the parse is done
	 *  one arena_Reset() throws away the temporaries AND the unused tail
	 *  of the vertex array in a single move.
	 */
	out->index = arena_Alloc(a, (long)ntri * 3 *
	    (long)sizeof(unsigned short), 2);
	verts_offset = arena_Mark(a);
	out->verts = arena_Alloc(a, (long)ntri * 3 *
	    (long)sizeof(struct gfx_vertex), 4);
	out->source = arena_Alloc(a, (long)ntri * 3 * (long)sizeof(int), 4);

	b.m = out;
	b.pos = arena_Alloc(a, (long)nv * 3 * (long)sizeof(float), 4);
	b.uv = nt > 0 ? arena_Alloc(a, (long)nt * 2 * (long)sizeof(float), 4)
	    : 0;
	b.nrm = nn > 0 ? arena_Alloc(a, (long)nn * 3 * (long)sizeof(float), 4)
	    : 0;
	b.table = arena_Alloc(a, (long)table_size * (long)sizeof(int), 4);
	b.keys = arena_Alloc(a, (long)table_size * 3 * (long)sizeof(int), 4);
	b.mask = table_size - 1;
	b.nv = 0;
	b.nt = 0;
	b.nn = 0;

	if (a->failed)
		return -1;

	s.p = (const char *)data;
	s.end = (const char *)data + len;

	while (!at_end(&s)) {
		skip_blanks(&s);
		if (keyword_is(&s, "v")) {
			for (i = 0; i < 3; i++) {
				value = 0.0f;
				parse_float(&s, &value);
				b.pos[b.nv * 3 + i] = value;
				if (b.nv == 0 || value < out->min[i])
					out->min[i] = value;
				if (b.nv == 0 || value > out->max[i])
					out->max[i] = value;
			}
			b.nv++;
		} else if (keyword_is(&s, "vt")) {
			for (i = 0; i < 2; i++) {
				value = 0.0f;
				parse_float(&s, &value);
				if (b.uv != 0)
					b.uv[b.nt * 2 + i] = value;
			}
			/*  .obj puts v = 0 at the bottom, every GPU puts it
			 *  at the top.  Flip once, here, and never again.
			 */
			if (b.uv != 0)
				b.uv[b.nt * 2 + 1] = 1.0f -
				    b.uv[b.nt * 2 + 1];
			b.nt++;
		} else if (keyword_is(&s, "vn")) {
			for (i = 0; i < 3; i++) {
				value = 0.0f;
				parse_float(&s, &value);
				if (b.nrm != 0)
					b.nrm[b.nn * 3 + i] = value;
			}
			b.nn++;
		} else if (keyword_is(&s, "usemtl")) {
			open_group(out, &s);
		} else if (keyword_is(&s, "f")) {
			if (out->ngroups == 0) {
				out->groups[0].name[0] = 0;
				out->groups[0].first = 0;
				out->groups[0].count = 0;
				out->ngroups = 1;
			}
			corners = 0;
			while (parse_corner(&s, b.nv, b.nt, b.nn, corner)) {
				at = intern(&b, corner);
				if (at < 0)
					return -1;
				if (corners < 2) {
					fan[corners] = at;
				} else {
					out->index[out->nindex++] =
					    (unsigned short)fan[0];
					out->index[out->nindex++] =
					    (unsigned short)fan[1];
					out->index[out->nindex++] =
					    (unsigned short)at;
					fan[1] = at;
				}
				corners++;
			}
		}
		skip_line(&s);
	}

	if (out->ngroups > 0)
		out->groups[out->ngroups - 1].count = out->nindex -
		    out->groups[out->ngroups - 1].first;

	if (nn == 0)
		smooth_normals(out);

	/*  Both arrays were allocated at their upper bound.  Slide the
	 *  source table down against the real end of the vertex array and
	 *  cut the arena there: the overshoot and every temporary go away
	 *  in one move.  The copy runs forward and the destination is
	 *  always below the source, so it is safe overlapping.
	 */
	{
		int *packed;
		long at;

		at = verts_offset + (long)out->nverts *
		    (long)sizeof(struct gfx_vertex);
		while (at % 4 != 0)
			at++;
		packed = (int *)(void *)(a->base + at);
		for (i = 0; i < out->nverts; i++)
			packed[i] = out->source[i];
		out->source = packed;
		arena_Reset(a, at + (long)out->nverts * (long)sizeof(int));
	}
	return a->failed ? -1 : 0;
}
