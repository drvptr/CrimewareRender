## WHAT IS THIS
This is a module for 3D software rendering of models in the wavefront obj format. Glory to https://www.siberianbattalion.com/

## DEPENDENCIES
- Windows: gcc (mingw)
- FreeBSD: Xlib

## STRUCTURE
- **IO/io.h** - header that deals with input and output to the screen. Here, abstractions such as "window" and functions above the window are defined.The implementation of a set of functions over a "window" as well as the "window" type itself can be defined differently depending on the framework. The implementation of the functions itself is in the .c file. Thus, for Unix systems the implementation is done in io_xlib.c, and for Windows in io_winapi.c. However, the function profiles must be the same everywhere.
```
window *io_InitWindow();		//Constructor-func for window_t
int io_GetWidth(window *w);	//Acsessors-funcs for window_t
int io_GetHeight(window *w);
void io_SetPixel(window *w, int x, int y, int color); //the basic function of drawing a pixel (output) (whatever the pixel is, and whatever the color is)
void io_UpdateFrame(window *w); //function for updating the video buffer (if any)
void io_CloseWindow(window *w);	//Destructor-func window_t
controls *io_InitControl();	//Constructor-func for control_t
void io_PollControls(window *w, controls *c, int mode); //polling control (input) devices (whatever these devices are)
#define io_FreeControl(control) (free(control)) //Destructor-func/macro control_t
```
Controls are a structure that contains an array of pressed keys, an array of activated keys, and mouse (or other pointer) coordinates. (Mouse buttons belong to the array of keys)
- **GRAPHIC/algebra.h** - A module that defines operations on vectors. Also defined in this module is the type of fixed-point number and operations on it.
- **GRAPHIC/tgatool.h** - TGA image parser. Also can draw on the image, find out its size, and take the color by coordinates from the image.
- **GRAPHIC/wavefront.h** - Wavefront parser. Also can recalculate normals (if there are no normals, for example), rotate an object, scale, move. Can print a log for debugging.
- **GRAPHIC/basic.h** - Graphic primitives module. Here are the main two-dimensional algorithms for drawing lines (Bresenham algorithm), for drawing triangles, for clipping triangles and lines. For drawing gradients and text. It is worth paying attention to the function for drawing a triangle. As a parameter, it accepts a function of the plotter type. Plotter is a function with a profile almost like SetPixel(), but it has an additional argument, the *void userdata. What is the point: the function for drawing a triangle only calculates the coordinates of the triangle by which the pixel needs to be painted. And how to paint it is decided by this function.
```
//EXAMPLE:

void DefaultPlot(window *w,int x,int y,int color,void *userdata){
	io_SetPixel(w,x,y,color);
};

//THEN WE CAN CALL TRIANGLE DRAWER
DrawTriangle(w,300,300,100,100,220,500,DefaultPlot,0xFFAA2020,NULL);
```
- **GRAPHIC/render3d.h** -This module contains a dynamic perspective camera. The camera is described as simply another coordinate system into which all points are projected. The camera also contains a depth buffer. The depth buffer is a two-dimensional array of integers, the size of the screen, where each cell indicates how far away the camera is from the camera. It is possible to render the buffer separately for debugging.
- **main.c** - Demonstration program. Just open this file and comment what you don't need.
