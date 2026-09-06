#include <math.h>
#include "m3.h"

struct vec3
v3(float x, float y, float z)
{
	struct vec3 r;

	r.x = x;
	r.y = y;
	r.z = z;
	return r;
}

struct vec3
v3_add(struct vec3 a, struct vec3 b)
{
	return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

struct vec3
v3_sub(struct vec3 a, struct vec3 b)
{
	return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

struct vec3
v3_scale(struct vec3 a, float s)
{
	return v3(a.x * s, a.y * s, a.z * s);
}

struct vec3
v3_cross(struct vec3 a, struct vec3 b)
{
	return v3(a.y * b.z - a.z * b.y,
	          a.z * b.x - a.x * b.z,
	          a.x * b.y - a.y * b.x);
}

float
v3_dot(struct vec3 a, struct vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

float
v3_len(struct vec3 a)
{
	return sqrtf(v3_dot(a, a));
}

float
v3_dist(struct vec3 a, struct vec3 b)
{
	return v3_len(v3_sub(a, b));
}

struct vec3
v3_norm(struct vec3 a)
{
	float len;

	len = v3_len(a);
	if (len < 0.000001f)
		return v3(0.0f, 0.0f, 0.0f);
	return v3_scale(a, 1.0f / len);
}

float
m3_clampf(float v, float lo, float hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

void
m4_identity(float out[16])
{
	int i;

	for (i = 0; i < 16; i++)
		out[i] = 0.0f;
	out[0] = 1.0f;
	out[5] = 1.0f;
	out[10] = 1.0f;
	out[15] = 1.0f;
}

void
m4_copy(float out[16], const float m[16])
{
	int i;

	for (i = 0; i < 16; i++)
		out[i] = m[i];
}

void
m4_mul(float out[16], const float a[16], const float b[16])
{
	float t[16];
	int col;
	int row;
	int i;
	float sum;

	for (col = 0; col < 4; col++) {
		for (row = 0; row < 4; row++) {
			sum = 0.0f;
			for (i = 0; i < 4; i++)
				sum += a[i * 4 + row] * b[col * 4 + i];
			t[col * 4 + row] = sum;
		}
	}
	m4_copy(out, t);
}

void
m4_translate(float out[16], float x, float y, float z)
{
	m4_identity(out);
	out[12] = x;
	out[13] = y;
	out[14] = z;
}

void
m4_scale(float out[16], float x, float y, float z)
{
	m4_identity(out);
	out[0] = x;
	out[5] = y;
	out[10] = z;
}

void
m4_rot_x(float out[16], float radians)
{
	float c;
	float s;

	c = cosf(radians);
	s = sinf(radians);
	m4_identity(out);
	out[5] = c;
	out[6] = s;
	out[9] = -s;
	out[10] = c;
}

void
m4_rot_y(float out[16], float radians)
{
	float c;
	float s;

	c = cosf(radians);
	s = sinf(radians);
	m4_identity(out);
	out[0] = c;
	out[2] = -s;
	out[8] = s;
	out[10] = c;
}

void
m4_rot_z(float out[16], float radians)
{
	float c;
	float s;

	c = cosf(radians);
	s = sinf(radians);
	m4_identity(out);
	out[0] = c;
	out[1] = s;
	out[4] = -s;
	out[5] = c;
}

void
m4_perspective(float out[16], float fov_y_radians, float aspect,
    float near_z, float far_z)
{
	float f;
	int i;

	f = 1.0f / tanf(fov_y_radians * 0.5f);
	for (i = 0; i < 16; i++)
		out[i] = 0.0f;
	out[0] = f / aspect;
	out[5] = f;
	out[10] = (far_z + near_z) / (near_z - far_z);
	out[11] = -1.0f;
	out[14] = (2.0f * far_z * near_z) / (near_z - far_z);
}

void
m4_ortho(float out[16], float left, float right, float bottom, float top,
    float near_z, float far_z)
{
	m4_identity(out);
	out[0] = 2.0f / (right - left);
	out[5] = 2.0f / (top - bottom);
	out[10] = -2.0f / (far_z - near_z);
	out[12] = -(right + left) / (right - left);
	out[13] = -(top + bottom) / (top - bottom);
	out[14] = -(far_z + near_z) / (far_z - near_z);
}

void
m4_look_at(float out[16], struct vec3 eye, struct vec3 at, struct vec3 up)
{
	struct vec3 f;
	struct vec3 s;
	struct vec3 u;

	f = v3_norm(v3_sub(at, eye));
	s = v3_norm(v3_cross(f, up));
	u = v3_cross(s, f);

	m4_identity(out);
	out[0] = s.x;
	out[4] = s.y;
	out[8] = s.z;
	out[1] = u.x;
	out[5] = u.y;
	out[9] = u.z;
	out[2] = -f.x;
	out[6] = -f.y;
	out[10] = -f.z;
	out[12] = -v3_dot(s, eye);
	out[13] = -v3_dot(u, eye);
	out[14] = v3_dot(f, eye);
}

void
m4_fps_view(float out[16], struct vec3 eye, float yaw, float pitch)
{
	struct vec3 dir;

	dir.x = cosf(pitch) * sinf(yaw);
	dir.y = sinf(pitch);
	dir.z = -cosf(pitch) * cosf(yaw);
	m4_look_at(out, eye, v3_add(eye, dir), v3(0.0f, 1.0f, 0.0f));
}

struct vec3
m4_mul_point(const float m[16], struct vec3 p)
{
	struct vec3 r;

	r.x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
	r.y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
	r.z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
	return r;
}

struct vec3
m4_mul_dir(const float m[16], struct vec3 d)
{
	struct vec3 r;

	r.x = m[0] * d.x + m[4] * d.y + m[8] * d.z;
	r.y = m[1] * d.x + m[5] * d.y + m[9] * d.z;
	r.z = m[2] * d.x + m[6] * d.y + m[10] * d.z;
	return r;
}

int
m4_invert(float out[16], const float m[16])
{
	float inv[16];
	float det;
	int i;

	inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] -
	    m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
	    m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
	inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
	    m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
	    m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] -
	    m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
	    m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
	    m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
	    m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
	    m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
	    m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
	inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] -
	    m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
	    m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
	inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] +
	    m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
	    m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
	inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] -
	    m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
	    m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] -
	    m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
	    m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
	inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] +
	    m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
	    m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
	inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] -
	    m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
	    m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] +
	    m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
	    m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] +
	    m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
	    m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] -
	    m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
	    m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] +
	    m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
	    m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] -
	    m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
	    m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if (det > -0.000001f && det < 0.000001f)
		return 0;
	det = 1.0f / det;
	for (i = 0; i < 16; i++)
		out[i] = inv[i] * det;
	return 1;
}

static void
plane_normalize(float p[4])
{
	float len;

	len = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
	if (len < 0.000001f)
		return;
	p[0] /= len;
	p[1] /= len;
	p[2] /= len;
	p[3] /= len;
}

void
fr_from_matrix(struct frustum *f, const float m[16])
{
	int i;

	/*  Gribb and Hartmann: the planes are sums and differences of the
	 *  matrix rows.  Row i of a column major matrix is m[i], m[4+i],
	 *  m[8+i], m[12+i].
	 */
	for (i = 0; i < 4; i++) {
		f->p[0][i] = m[i * 4 + 3] + m[i * 4 + 0];	/* left	 */
		f->p[1][i] = m[i * 4 + 3] - m[i * 4 + 0];	/* right */
		f->p[2][i] = m[i * 4 + 3] + m[i * 4 + 1];	/* bottom*/
		f->p[3][i] = m[i * 4 + 3] - m[i * 4 + 1];	/* top	 */
		f->p[4][i] = m[i * 4 + 3] + m[i * 4 + 2];	/* near	 */
		f->p[5][i] = m[i * 4 + 3] - m[i * 4 + 2];	/* far	 */
	}
	for (i = 0; i < 6; i++)
		plane_normalize(f->p[i]);
}

int
fr_test_aabb(const struct frustum *f, const float min[3], const float max[3])
{
	int i;
	float px;
	float py;
	float pz;

	for (i = 0; i < 6; i++) {
		/*  The corner furthest along the plane normal.  If even that
		 *  one is behind the plane, the whole box is outside.
		 */
		px = f->p[i][0] >= 0.0f ? max[0] : min[0];
		py = f->p[i][1] >= 0.0f ? max[1] : min[1];
		pz = f->p[i][2] >= 0.0f ? max[2] : min[2];
		if (f->p[i][0] * px + f->p[i][1] * py + f->p[i][2] * pz +
		    f->p[i][3] < 0.0f)
			return 0;
	}
	return 1;
}

int
fr_test_sphere(const struct frustum *f, struct vec3 c, float r)
{
	int i;
	float d;

	for (i = 0; i < 6; i++) {
		d = f->p[i][0] * c.x + f->p[i][1] * c.y + f->p[i][2] * c.z +
		    f->p[i][3];
		if (d < -r)
			return 0;
	}
	return 1;
}
