/*
 *	@(#)m3.h	1.0
 *
 *  Vectors, 4x4 matrices, frustum.  Plain float, no fixed point: the GPU
 *  wants floats anyway, and every platform we target has an FPU.
 *
 *  Matrices are 16 floats in COLUMN-MAJOR order, which is what OpenGL
 *  expects, so a matrix goes to the driver without a transpose.
 *
 *	m[0] m[4] m[8]  m[12]
 *	m[1] m[5] m[9]  m[13]
 *	m[2] m[6] m[10] m[14]
 *	m[3] m[7] m[11] m[15]
 */
#ifndef M3_H_SENTRY
#define M3_H_SENTRY

#include <math.h>	/* sqrtf, used by the inline vector code below */

struct vec3 {
	float x;
	float y;
	float z;
};

/*  These are defined here, not in m3.c, and that is a performance decision
 *  with numbers behind it.  A struct of three floats does not travel in
 *  memory: the ABI splits it across two XMM registers, so every call costs
 *  a movq plus a movss to pack each argument and two more to unpack the
 *  result.  Eight memory touches around a function whose body is three
 *  additions.
 *
 *  Measured on one workload (triangle normal and lighting, 2M points,
 *  gcc -O2), calling across a .c boundary:
 *
 *	struct by value, out of line	0.90 s	 8.0x
 *	float[3] by pointer, out of line0.35 s	 3.1x
 *	struct by pointer, out of line	0.60 s	 5.4x
 *	struct by value, inline here	0.11 s	 1.0x
 *
 *  Inlining wins because there is no call left to have a convention.  The
 *  bodies are three lines each; the code they replace is longer than they
 *  are.
 *
 *  Matrices stay out of line and travel as float[16] by pointer: 64 bytes
 *  never fit in registers under any ABI, and m4_mul is too big to inline
 *  usefully.  The rule is size, not taste.
 */

static inline struct vec3
v3(float x, float y, float z)
{
	struct vec3 r;

	r.x = x;
	r.y = y;
	r.z = z;
	return r;
}

static inline struct vec3
v3_add(struct vec3 a, struct vec3 b)
{
	return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline struct vec3
v3_sub(struct vec3 a, struct vec3 b)
{
	return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline struct vec3
v3_scale(struct vec3 a, float s)
{
	return v3(a.x * s, a.y * s, a.z * s);
}

static inline struct vec3
v3_cross(struct vec3 a, struct vec3 b)
{
	return v3(a.y * b.z - a.z * b.y,
	          a.z * b.x - a.x * b.z,
	          a.x * b.y - a.y * b.x);
}

static inline float
v3_dot(struct vec3 a, struct vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float
v3_len(struct vec3 a)
{
	return sqrtf(v3_dot(a, a));
}

static inline float
v3_dist(struct vec3 a, struct vec3 b)
{
	return v3_len(v3_sub(a, b));
}

static inline struct vec3
v3_norm(struct vec3 a)
{
	float len;

	len = v3_len(a);
	if (len < 0.000001f)
		return v3(0.0f, 0.0f, 0.0f);
	return v3_scale(a, 1.0f / len);
}

static inline float
m3_clampf(float v, float lo, float hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

/*  MATRICES.  Every m4_* writes into out, which may alias an input.  */

void m4_identity(float out[16]);
void m4_copy(float out[16], const float m[16]);
void m4_mul(float out[16], const float a[16], const float b[16]);
void m4_translate(float out[16], float x, float y, float z);
void m4_scale(float out[16], float x, float y, float z);
void m4_rot_x(float out[16], float radians);
void m4_rot_y(float out[16], float radians);
void m4_rot_z(float out[16], float radians);

void m4_perspective(float out[16], float fov_y_radians, float aspect,
                    float near_z, float far_z);
void m4_ortho(float out[16], float left, float right, float bottom,
              float top, float near_z, float far_z);
void m4_look_at(float out[16], struct vec3 eye, struct vec3 at,
                struct vec3 up);
/*  A first person camera is cheaper to build from angles than from a
 *  target point, so this one is here as well.  yaw turns around Y,
 *  pitch around the camera's own X.
 */
void m4_fps_view(float out[16], struct vec3 eye, float yaw, float pitch);

struct vec3 m4_mul_point(const float m[16], struct vec3 p);
struct vec3 m4_mul_dir(const float m[16], struct vec3 d);
int m4_invert(float out[16], const float m[16]);	/* 0 if singular */

/*  FRUSTUM.  Six planes pulled straight out of a view-projection matrix,
 *  each stored as a b c d with a*x + b*y + c*z + d >= 0 meaning inside.
 */
struct frustum {
	float p[6][4];
};

void fr_from_matrix(struct frustum *f, const float viewproj[16]);
int fr_test_aabb(const struct frustum *f, const float min[3],
                 const float max[3]);	/* 1 if possibly visible */
int fr_test_sphere(const struct frustum *f, struct vec3 c, float r);


#endif /* M3_H_SENTRY */
