#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include "io.h"

static int ConvertKeysyms(int keysym);


#define MAX_PATH_LENGTH 512
#define MAX_NAME 265

FILE *fmemopen(unsigned char arr[], unsigned int len, const char *mode) {
	char temp_path[MAX_PATH_LENGTH];
	if (GetTempPath(MAX_PATH_LENGTH, temp_path) == 0) {
		printf("(fmemopen ERROR): cant find temp path\n");
		return NULL;
	}
	srand(time(NULL));
	int random_number = rand()%99999999;
	char numstr[MAX_NAME];
	snprintf(numstr, sizeof(numstr), "%i\0", random_number);
	strncat(temp_path, "fmem_temp_file",  sizeof(temp_path) - strlen(temp_path) - 1);
	strncat(temp_path, numstr, sizeof(temp_path) - strlen(temp_path) - 1);
	FILE *temp = fopen(temp_path, "wb");
	if(temp == NULL){
		printf("(fmemopen ERROR): cant open temp file\n");
		return NULL;
	}
	if(fwrite(arr, sizeof(unsigned char), len, temp) != len){
		printf("(fmemopen ERROR): can't copy memory to temp file\n");
		fclose(temp);
		return NULL;
	}
	fclose(temp);
	temp = fopen(temp_path, mode);
	//printf("(fmemopen Done): file %s sucsessfuly opened\n",temp_path);
	return temp;
}


struct window_t {
	HWND hwnd;
	HDC hdc;
	HBITMAP hBitmap;
	int width;
	int height;
	BITMAPINFO bmi;
	unsigned char *buf;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

window *io_InitWindow() {
	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASS wc = {0};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "WindowClass";
	RegisterClass(&wc);
	HWND hwnd = CreateWindowEx(0, "WindowClass", TITLE, WS_OVERLAPPEDWINDOW,
		10, 10, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
		NULL, NULL, hInstance, NULL);
	if (!hwnd) {
		fprintf(stderr, "Cannot create window\n");
		exit(1);
	}
	ShowWindow(hwnd, SW_SHOW);
	HDC hdc = GetDC(hwnd);
	window *res = malloc(sizeof(window));
	res->width = DEFAULT_WINDOW_WIDTH;
	res->height = DEFAULT_WINDOW_HEIGHT;
	res->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	res->bmi.bmiHeader.biWidth = res->width;
	res->bmi.bmiHeader.biHeight = -res->height;  // Top-down image
	res->bmi.bmiHeader.biPlanes = 1;
	res->bmi.bmiHeader.biBitCount = 32;
	res->bmi.bmiHeader.biCompression = BI_RGB;
	res->bmi.bmiHeader.biSizeImage = 0;
	res->bmi.bmiHeader.biXPelsPerMeter = 0;
	res->bmi.bmiHeader.biYPelsPerMeter = 0;
	res->bmi.bmiHeader.biClrUsed = 0;
	res->bmi.bmiHeader.biClrImportant = 0;
	res->buf = malloc(res->width * res->height * 4);
	res->hwnd = hwnd;
	res->hdc = hdc;
	return res;
}

int io_GetWidth(window *w) {
	return w->width;
}

int io_GetHeight(window *w) {
	return w->height;
}

void io_SetPixel(window *w, int x, int y, int color) {
	// Assuming the buffer is in BGRA format (32 bits per pixel)
	int index = (y * w->width + x) * 4;
	w->buf[index + 0] = color & 0xFF;	   // Blue
	w->buf[index + 1] = (color >> 8) & 0xFF; // Green
	w->buf[index + 2] = (color >> 16) & 0xFF; // Red
	w->buf[index + 3] = (color >> 24) & 0xFF; // Alpha
}

void io_UpdateFrame(window *w) {
	HDC hdcMem = CreateCompatibleDC(w->hdc);
	HBITMAP hBitmap = CreateCompatibleBitmap(w->hdc, w->width, w->height);
	SelectObject(hdcMem, hBitmap);
	SetDIBitsToDevice(w->hdc, 0, 0, w->width, w->height,
			0, 0, 0, w->height, w->buf, &w->bmi, DIB_RGB_COLORS);
	DeleteDC(hdcMem);
	DeleteObject(hBitmap);
}

void io_CloseWindow(window *w) {
	free(w->buf);
	ReleaseDC(w->hwnd, w->hdc);
	DestroyWindow(w->hwnd);
	free(w);
}

controls *io_InitControl() {
	controls *c = malloc(sizeof(controls));
	c->type = none;
	for (int i = 0; i <= MAX_KEYS; i++) {
		HOLD(c, i) = 0;
		TOGGLE(c, i) = 0;
	}
	c->x = 0;
	c->y = 0;
	return c;
}

void io_PollControls(window *w, controls *c, int mode) {
	c->type = none;
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		int key;
		switch (msg.message) {
			case WM_KEYDOWN:
				c->type = press;
				key = ConvertKeysyms((int)msg.wParam);
				if (key >= 0 && key < MAX_KEYS) {
					HOLD(c, key) = 1;
					TOGGLE(c, key) = !TOGGLE(c, key);
				}
				break;
			case WM_KEYUP:
				c->type = release;
				key = ConvertKeysyms((int)msg.wParam);
				if (key >= 0 && key < MAX_KEYS) {
					HOLD(c, key) = 0;
				}
				break;
			case WM_LBUTTONDOWN:
				c->type = press;
				key = 100;
				HOLD(c, key) = 1;
				TOGGLE(c, key) = !TOGGLE(c, key);
				break;
			case WM_LBUTTONUP:
				c->type = release;
				key = 100;
				HOLD(c, key) = 0;
				break;
			case WM_RBUTTONDOWN:
				c->type = press;
				key = 101;
				HOLD(c, key) = 1;
				TOGGLE(c, key) = !TOGGLE(c, key);
				break;
			case WM_RBUTTONUP:
				c->type = release;
				key = 101;
				HOLD(c, key) = 0;
				break;
			case WM_MBUTTONDOWN:
				c->type = press;
				key = 102;
				HOLD(c, key) = 1;
				TOGGLE(c, key) = !TOGGLE(c, key);
				break;
			case WM_MBUTTONUP:
				c->type = release;
				key = 102;
				HOLD(c, key) = 0;
				break;
			case WM_MOUSEMOVE:
				MOUSE_X(c) = GET_X_LPARAM(msg.lParam);
				MOUSE_Y(c) = GET_Y_LPARAM(msg.lParam);
				break;
			case WM_QUIT:
				io_CloseWindow(w);
				break;
		}
	}
}

/*STATIC FUNCTIONS*/
static int ConvertKeysyms(int keysym) {
	// You can customize this mapping according to your key handling needs.
	if (keysym >= '0' && keysym <= '9') {
		return keysym - '0';
	}
	if (keysym >= 'A' && keysym <= 'Z') {
		return keysym - 'A' + 10;
	}
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}
