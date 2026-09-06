/*
 *	@(#)m3.h	2.0-вектор
 *
 *  ВЕТКА "вектор". Здесь vector - это float[3], а не структура, и ни одна
 *  функция не принимает и не возвращает ничего крупнее указателя или
 *  float. Правило простое: если аргумент не влезает в один регистр, он
 *  передаётся адресом.
 *
 *  Результат всегда ПОСЛЕДНИЙ аргумент: vec_add(a, b, c) значит c = a + b.
 *
 *  ОСТОРОЖНО, ЗДЕСЬ ЛОВУШКА. У m4_* результат, наоборот, ПЕРВЫЙ:
 *  m4_mul(out, a, b). Так сложилось на master и так же устроены все
 *  загрузчики: tga_Decode(dst, ...), obj_Parse(out, ...). То есть в одном
 *  заголовке теперь два противоположных порядка, и строчки
 *
 *	m4_mul(mvp, viewproj, model);
 *	vec_sub(b, a, edge);
 *
 *  стоят рядом. Компилятор перестановку не поймает: и то и другое float *.
 *
 *  Матрицы и до этой ветки были float[16] по указателю: 64 байта не
 *  влезали в регистры и на master.
 */
#ifndef M3_H_SENTRY
#define M3_H_SENTRY

#include <math.h>

enum {
	X = 0,
	Y = 1,
	Z = 2
};

typedef float vector[3];

/*  Присваивание. Массив нельзя присвоить оператором =, поэтому копия
 *  вектора - это макрос или memcpy, третьего не дано.
 */
#define VEC_ASSIGMENT(v, u) do {	\
		(u)[X] = (v)[X];	\
		(u)[Y] = (v)[Y];	\
		(u)[Z] = (v)[Z];	\
	} while (0)

#define VEC_SET(u, a, b, c) do {	\
		(u)[X] = (a);		\
		(u)[Y] = (b);		\
		(u)[Z] = (c);		\
	} while (0)

#define VEC_ZERO(u) VEC_SET(u, 0.0f, 0.0f, 0.0f)

#define VEC_DOT(a, b) ((a)[X] * (b)[X] + (a)[Y] * (b)[Y] + (a)[Z] * (b)[Z])

#define VEC_ABS(v) (sqrtf((v)[X] * (v)[X] + (v)[Y] * (v)[Y] +		\
	    (v)[Z] * (v)[Z]))

/* NOTE:  VEC_DOT и VEC_ABS вычисляют аргументы дважды и трижды, поэтому
 *	  VEC_ABS(speed_of(unit++)) писать нельзя. Функции ниже от этого
 *	  свободны и компилируются в то же самое.
 */

static inline float
vec_dot(const vector a, const vector b)
{
	return a[X] * b[X] + a[Y] * b[Y] + a[Z] * b[Z];
}

static inline float
vec_abs(const vector v)
{
	return sqrtf(vec_dot(v, v));
}

static inline void
vec_add(const vector a, const vector b, vector c)
{
	c[X] = a[X] + b[X];
	c[Y] = a[Y] + b[Y];
	c[Z] = a[Z] + b[Z];
}

static inline void
vec_sub(const vector a, const vector b, vector c)
{
	c[X] = a[X] - b[X];
	c[Y] = a[Y] - b[Y];
	c[Z] = a[Z] - b[Z];
}

static inline void
vec_scalar_mul(const vector v, float k, vector u)
{
	u[X] = v[X] * k;
	u[Y] = v[Y] * k;
	u[Z] = v[Z] * k;
}

/*  Через временные переменные, а не сразу в c. Иначе vec_cross(a, b, a)
 *  затирает a[X], и следующая строка читает уже испорченное значение.
 *  Со структурами по значению такой ошибки не существовало в принципе;
 *  здесь она возможна в любой функции с выходным аргументом. После
 *  встраивания временные имена исчезают.
 */
static inline void
vec_cross(const vector a, const vector b, vector c)
{
	float x;
	float y;
	float z;

	x = a[Y] * b[Z] - a[Z] * b[Y];
	y = a[Z] * b[X] - a[X] * b[Z];
	z = a[X] * b[Y] - a[Y] * b[X];
	c[X] = x;
	c[Y] = y;
	c[Z] = z;
}

static inline float
vec_dist(const vector a, const vector b)
{
	float dx;
	float dy;
	float dz;

	dx = a[X] - b[X];
	dy = a[Y] - b[Y];
	dz = a[Z] - b[Z];
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

static inline void
vec_norm(const vector v, vector u)
{
	float len;

	len = vec_abs(v);
	if (len < 0.000001f) {
		VEC_ZERO(u);
		return;
	}
	vec_scalar_mul(v, 1.0f / len, u);
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

/*  MATRICES.  Результат ПЕРВЫЙ, см. предупреждение вверху файла.  */

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
void m4_look_at(float out[16], const vector eye, const vector at,
    const vector up);
void m4_fps_view(float out[16], const vector eye, float yaw, float pitch);

void m4_mul_point(const float m[16], const vector p, vector out);
void m4_mul_dir(const float m[16], const vector d, vector out);
int m4_invert(float out[16], const float m[16]);

struct frustum {
	float p[6][4];
};

void fr_from_matrix(struct frustum *f, const float viewproj[16]);
int fr_test_aabb(const struct frustum *f, const float min[3],
    const float max[3]);
int fr_test_sphere(const struct frustum *f, const vector c, float r);

#endif /* M3_H_SENTRY */
