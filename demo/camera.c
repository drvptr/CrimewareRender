#include <math.h>
#include "camera.h"

void
cam_Init(struct camera *c, int mode)
{
	c->yaw = 0.0f;
	c->pitch = 0.0f;
	c->target = v3(0.0f, 0.0f, 0.0f);
	c->pos = v3(0.0f, 0.0f, 0.0f);
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

struct vec3
cam_Forward(const struct camera *c)
{
	struct vec3 d;

	d.x = cosf(c->pitch) * sinf(c->yaw);
	d.y = sinf(c->pitch);
	d.z = -cosf(c->pitch) * cosf(c->yaw);
	return d;
}

struct vec3
cam_Right(const struct camera *c)
{
	return v3(cosf(c->yaw), 0.0f, sinf(c->yaw));
}

void
cam_Update(struct camera *c, struct vec3 target, const struct aabb *solids,
    int nsolids)
{
	struct vec3 anchor;
	struct vec3 back;
	float best;
	float t;
	int i;

	c->target = target;
	anchor = v3_add(target, v3(0.0f, c->eye_height, 0.0f));

	if (c->distance < 0.01f) {
		c->pos = anchor;
		return;
	}

	/*  Куда камера хотела бы встать: назад по взгляду на длину поводка.
	 *  Луч оттуда проверяется на стены, и если стена ближе - поводок
	 *  укорачивается до неё. Без этого камера в третьем лице уезжает в
	 *  соседнюю комнату и показывает изнанку стен.
	 */
	back = v3_scale(cam_Forward(c), -c->distance);

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

	c->pos = v3_add(anchor, v3_scale(back, best));
}

void
cam_View(const struct camera *c, float view[16])
{
	struct vec3 forward;

	/*  Одна формула на все три режима: камера стоит в pos и смотрит
	 *  вдоль forward. Для третьего лица и вида сверху точка интереса
	 *  как раз лежит на этом луче, потому что pos от неё и отсчитан.
	 */
	forward = cam_Forward(c);
	m4_look_at(view, c->pos, v3_add(c->pos, forward), v3(0.0f, 1.0f, 0.0f));
}
