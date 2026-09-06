#include <math.h>
#include "camera.h"

void
cam_Init(struct camera *c, int mode)
{
	c->yaw = 0.0f;
	c->pitch = 0.0f;
	VEC_ZERO(c->target);
	VEC_ZERO(c->pos);
	cam_SetMode(c, mode);
}

void
cam_SetMode(struct camera *c, int mode)
{
	c->mode = mode;

	if (mode == CAM_FIRST) {
		c->distance = 0.0f;
		c->eye_height = 0.8f;
		c->pitch = m3_clampf(c->pitch, -1.5f, 1.5f);
	} else if (mode == CAM_THIRD) {
		c->distance = 4.0f;
		c->eye_height = 0.9f;
		c->pitch = m3_clampf(c->pitch, -1.2f, 0.6f);
	} else {
		c->distance = 18.0f;
		c->eye_height = 0.0f;
		c->pitch = -0.9f;
	}
}

void
cam_Look(struct camera *c, float dx, float dy)
{
	c->yaw += dx * 0.0025f;
	c->pitch -= dy * 0.0025f;

	if (c->mode == CAM_FIRST)
		c->pitch = m3_clampf(c->pitch, -1.5f, 1.5f);
	else if (c->mode == CAM_THIRD)
		c->pitch = m3_clampf(c->pitch, -1.2f, 0.6f);
	else
		c->pitch = m3_clampf(c->pitch, -1.4f, -0.3f);
}

void
cam_Forward(const struct camera *c, vector out)
{
	VEC_SET(out, cosf(c->pitch) * sinf(c->yaw), sinf(c->pitch),
	    -cosf(c->pitch) * cosf(c->yaw));
}

void
cam_Right(const struct camera *c, vector out)
{
	VEC_SET(out, cosf(c->yaw), 0.0f, sinf(c->yaw));
}

void
cam_Update(struct camera *c, const vector target, const struct aabb *solids,
    int nsolids)
{
	vector anchor;
	vector back;
	vector lift;
	vector forward;
	float best;
	float t;
	int i;

	VEC_ASSIGMENT(target, c->target);
	VEC_SET(lift, 0.0f, c->eye_height, 0.0f);
	vec_add(target, lift, anchor);

	if (c->distance < 0.01f) {
		VEC_ASSIGMENT(anchor, c->pos);
		return;
	}

	/*  Куда камера хотела бы встать: назад по взгляду на длину поводка.
	 *  Луч оттуда проверяется на стены, и если стена ближе - поводок
	 *  укорачивается до неё. Без этого камера в третьем лице уезжает в
	 *  соседнюю комнату и показывает изнанку стен.
	 */
	cam_Forward(c, forward);
	vec_scalar_mul(forward, -c->distance, back);

	best = 1.0f;
	for (i = 0; i < nsolids; i++) {
		if (coll_RayAabb(anchor, back, &solids[i], &t) && t < best)
			best = t;
	}
	if (best < 1.0f) {
		best -= 0.08f;	/* не прижиматься к стене вплотную */
		if (best < 0.0f)
			best = 0.0f;
	}

	vec_scalar_mul(back, best, back);
	vec_add(anchor, back, c->pos);
}

void
cam_View(const struct camera *c, float view[16])
{
	vector forward;
	vector at;
	vector up;

	/*  Одна формула на все три режима: камера стоит в pos и смотрит
	 *  вдоль forward. Для третьего лица и вида сверху точка интереса
	 *  как раз лежит на этом луче, потому что pos от неё и отсчитан.
	 */
	cam_Forward(c, forward);
	vec_add(c->pos, forward, at);
	VEC_SET(up, 0.0f, 1.0f, 0.0f);
	m4_look_at(view, c->pos, at, up);
}
