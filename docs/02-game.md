# 2. Как написать игру

Минимальная игра целиком — это один файл. Ниже он разобран по кускам;
рабочий и более полный вариант лежит в `demo/main.c`.

## Скелет

```c
#include "../core/app.h"
#include "../core/arena.h"
#include "../gfx/gfx.h"

static char pool[32 * 1024 * 1024];

struct game {
	struct arena mem;
	struct vec3 eye;
	float yaw;
};

static int  game_init(void *user);
static void game_step(void *user, float dt);
static void game_draw(void *user, float alpha);

int
main(void)
{
	static struct game g;
	struct app_hooks hooks;

	hooks.title = "моя игра";
	hooks.width = 1024;
	hooks.height = 640;
	hooks.step_hz = 60.0f;      /* шаг симуляции */
	hooks.max_fps = 250;        /* 0 — не ограничивать */
	hooks.audio_rate = 44100;   /* 0 — без звука */
	hooks.init = game_init;
	hooks.step = game_step;
	hooks.draw = game_draw;
	hooks.quit = 0;
	hooks.user = &g;

	return app_Run(&hooks);
}
```

`step` вызывается фиксированным шагом — там вся логика. `draw` вызывается
раз в кадр — там только рисование. Смешивать их можно, но тогда поведение
игры начнёт зависеть от частоты кадров.

## Загрузка ресурсов

Файлы читает игра, не движок:

```c
static void *
slurp(struct arena *a, const char *path, long *len)
{
	FILE *f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	void *mem = arena_Alloc(a, size + 1, 8);
	fread(mem, 1, size, f);
	fclose(f);
	*len = size;
	return mem;
}
```

Текстура: разобрали, залили в GPU, байты выбросили.

```c
long mark = arena_Mark(&g->mem);
long len;
void *file = slurp(&g->mem, "data/wall.tga", &len);
unsigned long long geo = tga_Geo(file);
void *pixels = arena_Alloc(&g->mem, tga_CanvasBytes(file), 4);
tga_Decode(pixels, tga_CanvasBytes(file), file, len);
g->wall = gfx_MakeTexture(pixels, geo, 1, 1);   /* smooth, repeat */
arena_Reset(&g->mem, mark);                     /* и файл, и пиксели */
```

Модель: вершины остаются в памяти, потому что они нужны коллизиям и
анимации.

```c
void *file = slurp(&g->mem, "data/guard.obj", &len);
obj_Parse(&g->model, file, len, &g->mem);
g->mesh = gfx_MakeMesh(g->model.verts, g->model.nverts,
    g->model.index, g->model.nindex, 0);
```

Несколько материалов в одной модели — это группы:

```c
for (i = 0; i < g->model.ngroups; i++)
	gfx_DrawMesh(g->mesh, model, texture_for(g->model.groups[i].name),
	    white, g->model.groups[i].first, g->model.groups[i].count);
```

## Уровень

Уровень — это секторы (что рисовать), порталы (куда видно) и solid-боксы
(куда нельзя):

```c
int hall = wld_AddSector(&g->world, hall_bounds, hall_mesh, wall_tex);
wld_AddSolid(&g->world, hall, box(-1.5f, 0, -1.5f, 1.5f, 1, 1.5f));

int room = wld_AddSector(&g->world, room_bounds, room_mesh, wall_tex);
wld_AddSolid(&g->world, room, box(-6, 0, 18, -2, 3, 22));

/* дверной проём: четыре угла, против часовой стрелки со стороны hall */
wld_AddPortal(&g->world, hall, room, a, b, c, d);
wld_AddPortal(&g->world, room, hall, b, a, d, c);  /* и обратно */
```

Порталы односторонние — второй вызов обязателен. Солиды добавляются строго
после своего сектора: сектор владеет непрерывным куском массива, поэтому
`wld_SolidsNear()` — это копирование, а не поиск.

## Движение и коллизии

```c
struct aabb body = coll_MakeAabb(g->eye, v3(0.35f, 0.9f, 0.35f));
struct aabb near[64];
int here = wld_SectorAt(&g->world, g->eye);
int n = wld_SolidsNear(&g->world, &here, 1, near, 64);

g->velocity.y -= 18.0f * dt;
if (g->on_floor && in->hold[' '])
	g->velocity.y = 6.0f;

g->eye = v3_add(g->eye, coll_MoveAabb(body, v3_scale(g->velocity, dt),
    near, n, &g->on_floor));
```

`coll_MoveAabb()` двигает коробку до столкновения, отбрасывает составляющую
скорости в стену и пробует снова — три раза. Этого хватает на угол.

Выстрел — луч:

```c
float t;
if (coll_RayMesh(eye, direction, enemy.verts, enemy.index, enemy.nindex,
    enemy_model, &t))
	hit(&enemy, t);
```

## Камера и отрисовка

```c
float view[16], proj[16], viewproj[16];

m4_fps_view(view, g->eye, g->yaw, g->pitch);
m4_perspective(proj, 1.2f, (float)app_Width() / app_Height(), 0.1f, 200.0f);
m4_mul(viewproj, proj, view);

gfx_BeginFrame(0.05f, 0.06f, 0.08f);
gfx_SetCamera(view, proj);

int list[16];
int n = wld_Visible(&g->world, wld_SectorAt(&g->world, g->eye),
    viewproj, list, 16);
for (i = 0; i < n; i++)
	gfx_DrawMesh(g->world.sector[list[i]].mesh, identity,
	    g->world.sector[list[i]].tex, white, 0, -1);
```

Свет и туман ставятся один раз, а не на объект:

```c
gfx_SetLight(v3(-0.5f, -1.0f, -0.3f), 0.35f);      /* направление, фон */
gfx_SetFog(0.05f, 0.06f, 0.08f, 12.0f, 40.0f);     /* цвет, от, до */
```

Туман здесь не украшение, а то, что прячет дальнюю плоскость отсечения.
Цвет тумана должен совпадать с цветом фона в `gfx_BeginFrame()`.

## Анимация

```c
anim_Sample(&g->walk, g->clock, 1, g->model.verts, g->model.nverts,
    g->model.source);
gfx_UpdateMesh(g->mesh, g->model.verts, g->model.nverts);
```

`source` — это отображение «вершина для GPU → строка `v` в .obj»,
которое `obj_Parse()` посчитал сам. Благодаря ему кадры анимации
индексируются вершинами исходного файла, а не результатом дедупликации.
Меш для анимации создавай с `dynamic = 1`.

## Звук

```c
snd_Listener(g->eye, forward);          /* каждый кадр */
snd_Play(&shot_wav, gun_position, 1.0f, 0);   /* пространственный */
snd_Play2D(&music_wav, 0.5f, 1);              /* без позиции, зациклен */
```

Микшер зовёт `app_Run()` сам. `snd_SetRange(ref, max)` — полная громкость
до `ref` метров, тишина после `max`.

## Меню и HUD

Immediate mode: виджет живёт ровно в той строке, где ты его вызвал.

```c
gui_Begin(app_Input());
gui_SetFont(g->font_tex, 8, 8);

if (g->state == MENU) {
	gui_SetColor(0, 0, 0, 0.7f);
	gui_Rect(0, 0, app_Width(), app_Height());
	gui_SetColor(1, 1, 1, 1);
	if (gui_Button(40, 40, 200, 36, "NEW GAME"))
		g->state = PLAYING;
	if (gui_Button(40, 86, 200, 36, "QUIT"))
		app_Quit();
	gui_Slider(40, 140, 200, 20, &g->volume);
}
gui_End();
```

Атлас шрифта делается один раз: `python3 tools/mkfont.py <шрифт.psf> font.tga`,
дальше он грузится как обычная текстура.

## Катсцена

```c
cut_Clear(&g->scene);
cut_Camera(&g->scene, 0.0f, v3(0, 2, 10), v3(0, 1, 0));
cut_Camera(&g->scene, 4.0f, v3(6, 3, 2), v3(0, 1, 0));
cut_Subtitle(&g->scene, 0.5f, 3.5f, "MOST VZYAT NA RASSVETE");
cut_Mark(&g->scene, 4.0f, EVENT_DOOR);
cut_Fades(&g->scene, 1.0f, 1.0f);
cut_Start(&g->scene);
```

В `step`:

```c
if (cut_Playing(&g->scene)) {
	cut_Advance(&g->scene, dt);
	int event;
	while ((event = cut_Poll(&g->scene)) >= 0)
		handle_event(g, event);   /* что это значит — решаешь ты */
}
```

В `draw` — `cut_View()` вместо своей камеры, `cut_Fade()` как чёрный
прямоугольник поверх, `cut_Text()` внизу экрана.

## Как это отлаживать

- `make PLAT=null GFX=null` — игра идёт без окна; вся логика, загрузка и
  коллизии проверяются под `valgrind` и в CI.
- `gfx_DrawCalls()` — сколько вызовов отрисовки ушло за кадр. Если при
  повороте спиной к двери число не падает, портальный граф неверен.
- `app_Fps()` — измеренная частота.
- `g->mem.peak` — сколько арены реально понадобилось.
