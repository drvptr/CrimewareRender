/*
 *	@(#)plat_win32.c	1.0-вектор
 *
 *  Win32: окно, ввод и программный фреймбуфер через GDI. Без GL, без
 *  звука - ровно тот же набор, что даёт plat_x11.c, чтобы игра собиралась
 *  под Windows тем же кодом.
 *
 *	x86_64-w64-mingw32-gcc -O2 -DFIXED_BITS=4 -o demo.exe \
 *		<файлы движка> plat/plat_win32.c plat/audio_none.c \
 *		gfx/gfx_soft.c demo/main.c demo/camera.c -lgdi32
 *
 *  ЧЕСТНОЕ ПРЕДУПРЕЖДЕНИЕ: этот файл написан, но ни разу не собран и не
 *  запущен - в контейнере, где он писался, нет ни windows.h, ни mingw.
 *  Все остальные файлы дерева проверены сборкой и прогоном, этот нет.
 *  Считай его наброском на день работы, а не готовым бэкендом.
 *
 *  Что взято из твоего io_winapi.c: раскладка BITMAPINFO с отрицательной
 *  высотой (сверху вниз, как у нас), обработка кнопок мыши и общая форма
 *  цикла сообщений. Что изменено:
 *
 *	*  SetDIBitsToDevice зовётся по готовому холсту, без создания
 *	   совместимого DC и битмапа на каждый кадр. У тебя в io_UpdateFrame
 *	   CreateCompatibleDC и CreateCompatibleBitmap вызывались каждый
 *	   кадр и тут же удалялись, хотя результат не использовался;
 *	*  WM_QUIT больше не закрывает окно изнутри опроса ввода - это
 *	   разрушало окно, пока цикл ещё работал. Ставится флаг quit, и
 *	   выход делает главный цикл;
 *	*  коды клавиш общие с X11-бэкендом: печатные символы это они сами.
 */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include "plat.h"
#include "../buf/buffer.h"

static HWND hwnd;
static HDC hdc;
static BITMAPINFO bmi;
static unsigned int *canvas;
static int win_w;
static int win_h;
static int grabbed;
static int last_mx;
static int last_my;
static int acc_dx;
static int acc_dy;
static int want_quit;
static int was_resized;
static LARGE_INTEGER freq;
static LARGE_INTEGER start_count;

static LRESULT CALLBACK
window_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
	if (msg == WM_DESTROY || msg == WM_CLOSE) {
		want_quit = 1;
		return 0;
	}
	if (msg == WM_SIZE) {
		was_resized = 1;
		return 0;
	}
	return DefWindowProc(h, msg, wp, lp);
}

int
plat_Init(void)
{
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start_count);
	return 1;
}

void
plat_Shutdown(void)
{
}

static int
make_canvas(int width, int height)
{
	if (canvas != 0)
		free(canvas);
	canvas = malloc((unsigned long)width * (unsigned long)height * 4);
	if (canvas == 0)
		return 0;

	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;	/* сверху вниз */
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	win_w = width;
	win_h = height;
	return 1;
}

int
plat_OpenWindow(const char *title, int width, int height)
{
	WNDCLASS wc;
	RECT r;

	memset(&wc, 0, sizeof wc);
	wc.lpfnWndProc = window_proc;
	wc.hInstance = GetModuleHandle(0);
	wc.lpszClassName = "engine";
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	RegisterClass(&wc);

	/*  Размер задаётся для клиентской области, а не для окна с рамкой:
	 *  иначе холст не совпадёт с тем, что видно.
	 */
	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

	hwnd = CreateWindowEx(0, "engine", title, WS_OVERLAPPEDWINDOW,
	    CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
	    0, 0, GetModuleHandle(0), 0);
	if (hwnd == 0) {
		fprintf(stderr, "plat: cannot create a window\n");
		return 0;
	}
	ShowWindow(hwnd, SW_SHOW);
	hdc = GetDC(hwnd);

	return make_canvas(width, height);
}

void
plat_CloseWindow(void)
{
	if (canvas != 0) {
		free(canvas);
		canvas = 0;
	}
	if (hwnd != 0) {
		ReleaseDC(hwnd, hdc);
		DestroyWindow(hwnd);
		hwnd = 0;
	}
}

void *
plat_Framebuffer(unsigned long long *geo_out)
{
	if (geo_out != 0)
		*geo_out = bufNewGeom(4, win_w, win_h);
	return canvas;
}

void
plat_Swap(void)
{
	if (hwnd == 0 || canvas == 0)
		return;
	SetDIBitsToDevice(hdc, 0, 0, (DWORD)win_w, (DWORD)win_h, 0, 0, 0,
	    (UINT)win_h, canvas, &bmi, DIB_RGB_COLORS);
}

void *
plat_GlProc(const char *name)
{
	(void)name;
	return 0;
}

double
plat_Time(void)
{
	LARGE_INTEGER now;

	QueryPerformanceCounter(&now);
	return (double)(now.QuadPart - start_count.QuadPart) /
	    (double)freq.QuadPart;
}

void
plat_Sleep(double seconds)
{
	if (seconds <= 0.0)
		return;
	Sleep((DWORD)(seconds * 1000.0));
}

static int
key_from_vk(int vk)
{
	if (vk >= 'A' && vk <= 'Z')
		return vk;
	if (vk >= '0' && vk <= '9')
		return vk;
	if (vk == VK_SPACE)
		return ' ';
	if (vk >= VK_F1 && vk <= VK_F12)
		return PLAT_KEY_F1 + (vk - VK_F1);
	if (vk == VK_ESCAPE)
		return PLAT_KEY_ESC;
	if (vk == VK_TAB)
		return PLAT_KEY_TAB;
	if (vk == VK_RETURN)
		return PLAT_KEY_ENTER;
	if (vk == VK_BACK)
		return PLAT_KEY_BACKSPACE;
	if (vk == VK_SHIFT)
		return PLAT_KEY_SHIFT;
	if (vk == VK_CONTROL)
		return PLAT_KEY_CTRL;
	if (vk == VK_MENU)
		return PLAT_KEY_ALT;
	if (vk == VK_UP)
		return PLAT_KEY_UP;
	if (vk == VK_DOWN)
		return PLAT_KEY_DOWN;
	if (vk == VK_LEFT)
		return PLAT_KEY_LEFT;
	if (vk == VK_RIGHT)
		return PLAT_KEY_RIGHT;
	return PLAT_KEY_NONE;
}

static void
press(struct plat_input *in, int key, int down)
{
	if (key <= 0 || key >= PLAT_KEYS)
		return;
	if (down) {
		if (!in->hold[key])
			in->hit[key] = 1;
		in->hold[key] = 1;
	} else {
		if (in->hold[key])
			in->rel[key] = 1;
		in->hold[key] = 0;
	}
}

static void
warp_to_centre(void)
{
	POINT p;

	last_mx = win_w / 2;
	last_my = win_h / 2;
	p.x = last_mx;
	p.y = last_my;
	ClientToScreen(hwnd, &p);
	SetCursorPos(p.x, p.y);
}

void
plat_GrabMouse(int on)
{
	grabbed = on;
	ShowCursor(on ? FALSE : TRUE);
	if (on)
		warp_to_centre();
}

void
plat_Poll(struct plat_input *in)
{
	MSG msg;
	RECT client;
	int i;
	int key;

	for (i = 0; i < PLAT_KEYS; i++) {
		in->hit[i] = 0;
		in->rel[i] = 0;
	}
	acc_dx = 0;
	acc_dy = 0;
	in->resized = 0;
	was_resized = 0;

	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);

		if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP) {
			key = key_from_vk((int)msg.wParam);
			press(in, key, msg.message == WM_KEYDOWN);
		} else if (msg.message == WM_LBUTTONDOWN) {
			press(in, PLAT_MOUSE_L, 1);
		} else if (msg.message == WM_LBUTTONUP) {
			press(in, PLAT_MOUSE_L, 0);
		} else if (msg.message == WM_RBUTTONDOWN) {
			press(in, PLAT_MOUSE_R, 1);
		} else if (msg.message == WM_RBUTTONUP) {
			press(in, PLAT_MOUSE_R, 0);
		} else if (msg.message == WM_MBUTTONDOWN) {
			press(in, PLAT_MOUSE_M, 1);
		} else if (msg.message == WM_MBUTTONUP) {
			press(in, PLAT_MOUSE_M, 0);
		} else if (msg.message == WM_MOUSEWHEEL) {
			if (GET_WHEEL_DELTA_WPARAM(msg.wParam) > 0)
				press(in, PLAT_WHEEL_UP, 1);
			else
				press(in, PLAT_WHEEL_DOWN, 1);
		} else if (msg.message == WM_MOUSEMOVE) {
			int x;
			int y;

			x = GET_X_LPARAM(msg.lParam);
			y = GET_Y_LPARAM(msg.lParam);
			acc_dx += x - last_mx;
			acc_dy += y - last_my;
			last_mx = x;
			last_my = y;
			in->mouse_x = x;
			in->mouse_y = y;
		} else if (msg.message == WM_QUIT) {
			want_quit = 1;
		}

		DispatchMessage(&msg);
	}

	if (was_resized && hwnd != 0) {
		GetClientRect(hwnd, &client);
		if (client.right != win_w || client.bottom != win_h) {
			make_canvas(client.right, client.bottom);
			in->resized = 1;
		}
	}

	in->mouse_dx = acc_dx;
	in->mouse_dy = acc_dy;
	in->width = win_w;
	in->height = win_h;
	if (want_quit)
		in->quit = 1;

	if (grabbed && (acc_dx != 0 || acc_dy != 0))
		warp_to_centre();
}
