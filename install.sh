gcc -c io_xlib.c -o io.o
gcc main.c io.o -lX11 -lXext
gcc main.c io.o wavefront.o render3d.o -lX11 -lXext -lGL -lGLU -lm
