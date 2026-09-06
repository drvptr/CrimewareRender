# 05. Платформа: окно, ввод, время, звук

Файлы `plat/plat.h`, `plat/plat_x11.c`, `plat/plat_null.c`. Здесь и только
здесь движок разговаривает с операционной системой.

## Договор

`plat.h` описывает всё, что движку нужно от системы:

```c
int plat_Init(void);
int plat_OpenWindow(const char *title, int width, int height);
void plat_Poll(struct plat_input *in);
void plat_Swap(void);
void plat_GrabMouse(int on);
double plat_Time(void);
void plat_Sleep(double seconds);
void *plat_GlProc(const char *name);
int plat_AudioOpen(int rate, int channels);
int plat_AudioSpace(void);
int plat_AudioWrite(const short *pcm, int frames);
```

Одиннадцать функций. Всё, что операционная система даёт сверх этого,
движку не нужно.

Окно ровно одно, и его дескриптор нигде не фигурирует. Это осознанное
упрощение: игре второе окно не нужно, а поддержка нескольких означала бы
таскать указатель через каждый вызов. Реализация хранит окно в статической
переменной файла.

Реализаций две. `plat_x11.c` — настоящая. `plat_null.c` — заглушка, где
работает только время: окна нет, ввода нет, звука нет. С ней собирается
`make check`, и вся логика игры проверяется в терминале.

## Как открывается окно

Разберём `plat_OpenWindow` по шагам. Xlib — библиотека 1985 года, и её
многословность нужно просто пережить.

```c
	vi = p_glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
```

**Визуал** — это описание формата пикселя окна: сколько бит на красный,
есть ли буфер глубины, двойная ли буферизация. Мы просим по 8 бит на цвет,
24 на глубину, 8 на трафарет и двойную буферизацию, а X-сервер выбирает
подходящий вариант из тех, что умеет видеокарта.

```c
	cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual,
	    AllocNone);
	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
	    ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
	    StructureNotifyMask | FocusChangeMask;
```

Карта цветов — наследие эпохи палитр, для TrueColor она формальность, но
без неё окно не создать. `event_mask` важнее: X-сервер присылает **только**
те события, о которых его попросили. Забыл `PointerMotionMask` — мышь не
двигается, и понять почему трудно, потому что ошибки нет, просто тишина.

```c
	XStoreName(dpy, win, title);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wm_delete, 1);
	XMapWindow(dpy, win);
```

`XStoreName` — заголовок окна. Дальше три строки про закрытие: в X11 крестик
рисует не приложение, а оконный менеджер, и по умолчанию он просто убивает
процесс. Чтобы получить шанс закрыться самому, надо зарегистрировать
протокол `WM_DELETE_WINDOW` — тогда вместо убийства придёт событие.
`XMapWindow` показывает окно; до этого вызова его нет на экране.

```c
	gl_ctx = p_glXCreateContext(dpy, vi, 0, True);
	p_glXMakeCurrent(dpy, win, gl_ctx);
```

**Контекст OpenGL** — это всё состояние рисования: текущий шейдер,
привязанные буферы, включённые режимы. Сам по себе он ничего не рисует;
`glXMakeCurrent` делает его текущим для этого потока, и после этого вызовы
`gl*` начинают работать. До этой строки любой вызов OpenGL — падение.

## Двойная буферизация и `plat_Swap`

Кадр рисуется не на экран, а в невидимый задний буфер. `plat_Swap`
(`glXSwapBuffers`) меняет буферы местами.

Без этого игрок видел бы процесс рисования: сначала фон, потом половину
стен, потом модель. Получилось бы мерцание и разрывы. С двойной
буферизацией кадр появляется целиком.

## dlopen: почему не `-lGL`

Обычно OpenGL подключают заголовком `<GL/gl.h>` и ключом `-lGL`. Здесь ни
того, ни другого:

```c
static XVisualInfo *(*p_glXChooseVisual)(Display *, int, int *);
static void *(*p_glXCreateContext)(Display *, XVisualInfo *, void *, int);
static int (*p_glXMakeCurrent)(Display *, unsigned long, void *);
static void (*p_glXSwapBuffers)(Display *, unsigned long);

static int
load_gl(void)
{
	gl_lib = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
	if (gl_lib == 0)
		return 0;
	p_glXChooseVisual = dlsym(gl_lib, "glXChooseVisual");
	...
}
```

Прототипы четырёх функций GLX выписаны руками. Они не менялись с 1998 года,
так что риск нулевой, а выгода такая:

- не нужны пакеты `-dev` для сборки;
- бинарник запускается на машине, где OpenGL нет вообще — просто скажет
  «no libGL.so.1» и выйдет по-человечески, а не упадёт при старте с
  сообщением динамического компоновщика;
- то же самое работает для звука.

`RTLD_GLOBAL` важен: символы из libGL становятся видны для последующих
`dlsym` по всему процессу.

`plat_GlProc` — то, чем рендер добывает остальные три десятка точек входа:

```c
void *
plat_GlProc(const char *name)
{
	void *p = 0;

	if (p_glXGetProcAddressARB != 0)
		p = p_glXGetProcAddressARB((const unsigned char *)name);
	if (p == 0 && gl_lib != 0)
		p = dlsym(gl_lib, name);
	return p;
}
```

Сначала спрашиваем у GLX (так положено для функций расширений), потом
пробуем обычный `dlsym` (так находятся функции ядра OpenGL 1.1, которых в
GLX может не быть).

## Прагма про указатели на функции

Над обеими загрузками стоит:

```c
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
```

`dlsym` возвращает `void *`, а стандарт C запрещает присваивать `void *`
указателю на функцию: указатель на данные и указатель на код не обязаны
быть одного размера (были архитектуры, где это так). POSIX требует, чтобы
это работало, и на всех живых платформах работает.

Предупреждение выключено ровно вокруг двух функций-загрузчиков и больше
нигде. Весь остальной движок собирается с `-pedantic` начисто.

## Ввод

```c
struct plat_input {
	unsigned char hold[PLAT_KEYS];
	unsigned char hit[PLAT_KEYS];
	unsigned char rel[PLAT_KEYS];
	int mouse_x, mouse_y;
	int mouse_dx, mouse_dy;
	int width, height;
	int resized;
	int quit;
};
```

Три массива вместо очереди событий. Игре почти никогда не нужен порядок
нажатий внутри кадра, зато постоянно нужен вопрос «зажат ли сейчас W».
Массив отвечает на него за одно обращение.

Коды клавиш выбраны так, чтобы печатные символы были собой:

```c
	if (in->hold['W'])
		wish = v3_add(wish, forward);
	if (in->hold[' '])
		jump();
```

Никакой таблицы, никаких `KEY_W`. Служебные клавиши занимают числа 1..28,
где печатных символов нет:

```c
enum {
	PLAT_KEY_ESC = 1, PLAT_KEY_TAB = 2, PLAT_KEY_ENTER = 3,
	PLAT_KEY_SHIFT = 5, PLAT_KEY_CTRL = 6, PLAT_KEY_ALT = 7,
	PLAT_KEY_UP = 8, ... PLAT_KEY_F1 = 12, ...
	PLAT_MOUSE_L = 24, PLAT_MOUSE_R = 25, PLAT_MOUSE_M = 26,
	PLAT_WHEEL_UP = 27, PLAT_WHEEL_DOWN = 28
};
```

Кнопки мыши и колесо лежат в том же массиве. Для игры это то же самое, что
клавиша, и разделять их незачем.

Перевод из X11 делает `key_from_sym`:

```c
	if (sym >= XK_a && sym <= XK_z)
		return (int)('A' + (sym - XK_a));
```

X-сервер отдаёт **keysym** — символ с учётом раскладки, а не физическую
клавишу. Мы берём вариант без модификаторов (`XLookupKeysym(&ev.xkey, 0)`) и
приводим к верхнему регистру. Побочный эффект: в русской раскладке W не
сработает, потому что keysym будет «ц». Правильное лечение — читать
физический код (`ev.xkey.keycode`), но тогда нужна своя таблица под
раскладку клавиатуры. Известное ограничение, записано в план.

## Захват мыши

Чтобы вертеть головой, курсор надо спрятать и не давать ему уйти за край
экрана.

```c
static void
warp_to_centre(void)
{
	last_mx = win_w / 2;
	last_my = win_h / 2;
	XWarpPointer(dpy, None, win, 0, 0, 0, 0, last_mx, last_my);
}
```

После каждого движения курсор телепортируется в центр окна. Смещение при
этом считается до телепортации:

```c
	} else if (ev.type == MotionNotify) {
		acc_dx += ev.xmotion.x - last_mx;
		acc_dy += ev.xmotion.y - last_my;
		last_mx = ev.xmotion.x;
		last_my = ev.xmotion.y;
	}
```

Телепортация сама порождает событие движения, но оно даёт нулевое
смещение: `last_mx` уже равен центру, куда курсор и приехал. Это тот
случай, где легко получить бесконечный дёрганый цикл, если перепутать
порядок присваиваний.

Курсор прячется пустым курсором из битмапа 8×8:

```c
	blank = XCreateBitmapFromData(dpy, win, nothing, 8, 8);
	cur = XCreatePixmapCursor(dpy, blank, blank, &black, &black, 0, 0);
	XDefineCursor(dpy, win, cur);
```

В X11 нет функции «спрятать курсор», поэтому его заменяют полностью
прозрачным.

## Время

```c
static double
now_seconds(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}
```

`CLOCK_MONOTONIC` — часы, которые только идут вперёд. Обычное системное
время (`CLOCK_REALTIME`) можно перевести вручную или синхронизацией по
сети, и тогда игровой цикл получит отрицательный `dt` со всеми
вытекающими. Монотонные часы этого не допускают.

`plat_Time` возвращает секунды от `plat_Init`, а не от 1970 года: у `double`
52 бита мантиссы, и разрешение тем лучше, чем меньше число.

## Звук

Модель push, разобранная в главе 03. Здесь — как она выглядит со стороны
системы.

```c
	alsa_lib = dlopen("libasound.so.2", RTLD_LAZY);
	if (alsa_lib == 0)
		return 0;

	p_snd_pcm_open = dlsym(alsa_lib, "snd_pcm_open");
	p_snd_pcm_set_params = dlsym(alsa_lib, "snd_pcm_set_params");
	...
	err = p_snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
	    SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, 60000);
```

ALSA тоже через `dlopen`. Взята «простая» часть её API: `snd_pcm_set_params`
делает одним вызовом то, на что в полном API уходит два десятка.

Параметры: 16 бит со знаком little-endian, чередующиеся каналы (левый,
правый, левый, правый), разрешено программное преобразование частоты,
желаемая задержка 60 мс.

```c
int
plat_AudioSpace(void)
{
	avail = p_snd_pcm_avail_update(pcm);
	if (avail < 0) {
		p_snd_pcm_recover(pcm, (int)avail, 1);
		return 0;
	}
	return (int)avail;
}
```

Отрицательный ответ — это **underrun**: игра не успела подать данные, буфер
опустел, устройство встало. Слышно как щелчок. `snd_pcm_recover` поднимает
устройство обратно. Игнорировать нельзя: без восстановления звук пропадёт
до конца сессии.

Если звука нет вообще, все три функции возвращают ноль, `push_audio` тихо
выходит, игра работает молча. Отсутствие звуковой карты не должно мешать
играть.

## Порты на другие системы

Что нужно написать для Windows (`plat_win32.c`):

| функция | Win32 |
|---|---|
| `plat_OpenWindow` | `RegisterClass` + `CreateWindowEx` + `wglCreateContext` |
| `plat_Poll` | `PeekMessage` в цикле, оконная процедура заполняет структуру |
| `plat_Swap` | `SwapBuffers(hdc)` |
| `plat_Time` | `QueryPerformanceCounter` |
| `plat_GlProc` | `wglGetProcAddress` с откатом на `GetProcAddress` |
| `plat_Audio*` | `waveOutWrite` с очередью заголовков |

Ничего за пределами `plat/` при этом не меняется. Проверить, что ничего не
забыто, можно грепом из главы 00.
