mkdir .\build
gcc -c IO\io_winapi.c -o build\io.o 
gcc -c GRAPHIC\tgatool.c -o build\tgatool.o 
gcc -c GRAPHIC\algebra.c -o build\algebra.o -D_FIXED_POINT
gcc -c GRAPHIC\wavefront.c -o build\wavefront.o 
gcc -c GRAPHIC\basics.c -o build\basics.o -D_FIXED_POINT
gcc -c GRAPHIC\render3d.c -o build\render3d.o 
gcc -static -o run.exe main.c build\* -lm -lgdi32 -luser32 -mwindows