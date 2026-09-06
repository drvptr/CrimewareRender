/*
 *	@(#)coll.h	2.0-вектор
 *
 *  Ветка "вектор": ничего крупнее указателя по значению не передаётся,
 *  поэтому struct aabb теперь везде ходит адресом, а функции, которые
 *  раньше возвращали вектор, пишут его в последний аргумент.
 *
 *  Коллизии, и ничего, что можно назвать физикой. Нет массы, нет упругости,
 *  нет решателя и нет шага по времени: ты спрашиваешь "столкнулось ли это
 *  с тем" и "как далеко можно уйти до препятствия", и ответы точные и без
 *  состояния.
 */
#ifndef COLL_H_SENTRY
#define COLL_H_SENTRY

#include "../core/m3.h"
#include "../gfx/gfx.h"

struct aabb {
	float min[3];
	float max[3];
};

void coll_MakeAabb(const vector centre, const vector half, struct aabb *out);
void coll_AabbCentre(const struct aabb *b, vector out);
void coll_AabbGrow(struct aabb *b, const vector p);

int coll_AabbAabb(const struct aabb *a, const struct aabb *b);
int coll_PointAabb(const vector p, const struct aabb *b);
int coll_SphereAabb(const vector centre, float radius, const struct aabb *b,
    vector push_out);
/* NOTE:  push_out (можно 0) получает кратчайший вектор, выталкивающий сферу
 *	  из коробки. Это и есть всё "не ходи сквозь стену" для персонажа,
 *	  который считается шаром.
 */

int coll_RayAabb(const vector origin, const vector dir, const struct aabb *b,
    float *t_out);
int coll_RayTri(const vector origin, const vector dir, const vector a,
    const vector b, const vector c, float *t_out);

int coll_RayMesh(const vector origin, const vector dir,
    const struct gfx_vertex *verts, const unsigned short *index, int nindex,
    const float model[16], float *t_out);
/* NOTE:  перебор всех треугольников. Для выстрела по одному объекту за кадр
 *	  это бесплатно; по всему уровню без сужения сектором - нет.
 */

void coll_MoveAabb(const struct aabb *body, const vector move,
    const struct aabb *solids, int nsolids, int *hit_ground, vector out);
/* USAGE:
	coll_MoveAabb(&body, wish, solids, n, &on_floor, delta);
	vec_add(pos, delta, pos);
   NOTE:
	свип коробки против неподвижных коробок, три прохода, со скольжением
	вдоль того, во что упёрлись. hit_ground (можно 0) выставляется, когда
	заблокированное направление было вниз - это всё, что нужно прыжку.
*/

#endif /* COLL_H_SENTRY */
