/*
 *	@(#)camera.h	1.0
 *
 *  Камера. Обрати внимание, где лежит этот файл: в demo/, а не в движке.
 *
 *  Движок про камеру не знает ничего. gfx_SetCamera() принимает две
 *  готовые матрицы, wld_Visible() принимает номер сектора, snd_Listener()
 *  принимает точку и направление. Откуда они взялись - из глаз персонажа,
 *  из точки над полем боя, из рельсов катсцены - его не касается.
 *
 *  Поэтому камера здесь: то, как она следует за персонажем, насколько
 *  запаздывает, как ведёт себя у стены - это дизайн игры, а не техника.
 *  Скопируй файл к себе и правь, ничего не сломается.
 *
 *  Три режима, чтобы было видно, что первое лицо ничем не выделено:
 *
 *	CAM_FIRST	из глаз подопечного
 *	CAM_THIRD	сзади, поводок укорачивается у стены
 *	CAM_TOP		сверху, над точкой, которая ни к кому не привязана
 *
 *  Все три - одна и та же математика с разной длиной поводка и разными
 *  пределами наклона. Первое лицо - это третье лицо с нулевым поводком.
 */
#ifndef CAMERA_H_SENTRY
#define CAMERA_H_SENTRY

#include "../core/m3.h"
#include "../world/coll.h"

enum {
	CAM_FIRST = 0,
	CAM_THIRD = 1,
	CAM_TOP = 2
};

struct camera {
	int mode;
	float yaw;
	float pitch;
	float distance;		/* длина поводка, 0 - первое лицо	*/
	float eye_height;	/* насколько выше точки интереса	*/
	vector target;		/* точка интереса, куда смотрим		*/
	vector pos;		/* где оказалась камера, считает cam_Update */
};

void cam_Init(struct camera *c, int mode);
void cam_SetMode(struct camera *c, int mode);

void cam_Look(struct camera *c, float dx, float dy);
/* NOTE:  смещение мыши в пикселях. Наклон ограничивается по-разному в
 *	  разных режимах: сверху смотреть горизонтально бессмысленно.
 */

void cam_Update(struct camera *c, const vector target,
    const struct aabb *solids, int nsolids);
/* USAGE:
	cam_Update(&cam, unit[controlled].pos, near_solids, n);
   NOTE:
	считает pos. Для третьего лица стреляет лучом от точки интереса
	назад и укорачивает поводок, если между ними стена - иначе камера
	уезжает внутрь геометрии и видно изнанку уровня.
	solids можно передать 0, тогда поводок не укорачивается.
*/

void cam_View(const struct camera *c, float view[16]);
void cam_Forward(const struct camera *c, vector out);
void cam_Right(const struct camera *c, vector out);
/* NOTE:  Forward и Right нужны не только матрице: по ним считается
 *	  движение относительно взгляда и панорама звука.
 */

#endif /* CAMERA_H_SENTRY */
