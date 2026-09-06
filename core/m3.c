#include "m3.h"

/*  The vector functions are not here: they live in m3.h as static inline,
 *  and the comment there says why.  What stays out of line is everything
 *  that works on float[16] or is too large to inline.
 */

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
m4_look_at(float out[16], const vector eye, const vector at,
    const vector up)
{
	vector f;
	vector s;
	vector u;
	vector tmp;

	vec_sub(at, eye, tmp);
	vec_norm(tmp, f);
	vec_cross(f, up, tmp);
	vec_norm(tmp, s);
	vec_cross(s, f, u);

	m4_identity(out);
	out[0] = s[X];
	out[4] = s[Y];
	out[8] = s[Z];
	out[1] = u[X];
	out[5] = u[Y];
	out[9] = u[Z];
	out[2] = -f[X];
	out[6] = -f[Y];
	out[10] = -f[Z];
	out[12] = -vec_dot(s, eye);
	out[13] = -vec_dot(u, eye);
	out[14] = vec_dot(f, eye);
}

void
m4_fps_view(float out[16], const vector eye, float yaw, float pitch)
{
	vector dir;
	vector at;
	vector up;

	VEC_SET(dir, cosf(pitch) * sinf(yaw), sinf(pitch),
	    -cosf(pitch) * cosf(yaw));
	vec_add(eye, dir, at);
	VEC_SET(up, 0.0f, 1.0f, 0.0f);
	m4_look_at(out, eye, at, up);
}

void
m4_mul_point(const float m[16], const vector p, vector out)
{
	float x;
	float y;
	float z;

	x = m[0] * p[X] + m[4] * p[Y] + m[8] * p[Z] + m[12];
	y = m[1] * p[X] + m[5] * p[Y] + m[9] * p[Z] + m[13];
	z = m[2] * p[X] + m[6] * p[Y] + m[10] * p[Z] + m[14];
	VEC_SET(out, x, y, z);
}

void
m4_mul_dir(const float m[16], const vector d, vector out)
{
	float x;
	float y;
	float z;

	x = m[0] * d[X] + m[4] * d[Y] + m[8] * d[Z];
	y = m[1] * d[X] + m[5] * d[Y] + m[9] * d[Z];
	z = m[2] * d[X] + m[6] * d[Y] + m[10] * d[Z];
	VEC_SET(out, x, y, z);
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
fr_test_sphere(const struct frustum *f, const vector c, float r)
{
	int i;
	float d;

	for (i = 0; i < 6; i++) {
		d = f->p[i][0] * c[X] + f->p[i][1] * c[Y] +
		    f->p[i][2] * c[Z] + f->p[i][3];
		if (d < -r)
			return 0;
	}
	return 1;
}
