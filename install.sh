#!/bin/sh

mkdir ./build
cc -c IO/io_xlib.c -o build/io.o -O3 -I/usr/local/include/  -D_FIXED_POINT
cc -c GRAPHIC/algebra.c -o build/algebra.o -O3 -I/usr/local/include/ 
cc -c GRAPHIC/tgatool.c -o build/tgatool.o -O3 -I/usr/local/include/ 
cc -c GRAPHIC/wavefront.c -o build/wavefront.o -O3 -I/usr/local/include/ 
cc -c GRAPHIC/basics.c -o build/basics.o -O3 -I/usr/local/include/ -D_FIXED_POINT
cc -c GRAPHIC/render3d.c -o build/render3d.o -O3 -I/usr/local/include/ 
cc -o run main.c build/* -O3 -L/usr/local/lib -lX11 -lm 
