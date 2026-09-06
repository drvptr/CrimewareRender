# Minimalistic monolithic engine

A 3D engine framework written in C99. No dependencies: it builds against libX11
and libm, and OpenGL and ALSA are opened via dlopen at runtime—the binary
runs even on a machine that doesn't have them.

Not C89: the geometry of buf requires unsigned long long, and the math requires sqrtf. Both requirements come from your own buffer.h, so there's no point in arguing.

```
make # demo (x11 + opengl) -> ./demo-bin
make check # tests, no display -> 77 checks
make PLAT=null GFX=null # the same demo without a window
``

## What is this

An engine framework, not an editor with a runtime. There's no division between resources and
engine: the level, AI, logic, and menus are all regular C code that you compile
along with the engine. The engine provides the window, input, triangles, sound, collisions, and
format loaders. Everything else is yours.

PS2-level graphics: textures, transparency, vertex lighting (Gouraud),
fog, sprites, 2D. No antialiasing, real-time shadows,
postprocessing, or physics.

## Layout

```
buf/ your buffer.c — buffer geometry, where textures are described
core/ m3 (vectors, matrices, frustum), arena (memory), app (main loop)
plat/ window, input, time, sound device [x11 | null]
gfx/ renderer behind the interface without a single GL type [gl | null]
asset/ tga, obj, wav, van — input bytes, output buffers
world/ sectors and portals, collisions
snd/ software mixer
ui/ immediate-mode GUI, cutscene timeline
tools/ Blender exporter, font atlas generator, xxd wrapper
demo/ example game: two rooms, portal, collisions, model
test/ things that are tested off-screen
docs/ this documentation
```
