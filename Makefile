# Two knobs.  PLAT picks the window system, GFX picks the renderer, and
# nothing else in the tree knows which you chose.
#
#	make			x11 + gl, the demo (binary: demo-bin)
#	make check		null + null, the tests, no display needed
#	make PLAT=null GFX=null	the demo, headless
#
# There is no -lGL and no -lasound: both are opened with dlopen at run time
# by plat_x11.c, so the binary starts on a machine that has neither.

PLAT ?= x11
GFX  ?= gl

CC     ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2 -g
LDLIBS  = -lm

ifeq ($(PLAT),x11)
LDLIBS += -lX11 -ldl
endif

CORE = core/m3.c core/arena.c core/app.c
ASSET = asset/tga.c asset/obj.c asset/wav.c asset/anim.c
WORLD = world/coll.c world/world.c
UI = ui/gui.c ui/cut.c
SND = snd/snd.c
BUF = buf/buffer.c

ENGINE = $(BUF) $(CORE) $(ASSET) $(WORLD) $(UI) $(SND) \
	 plat/plat_$(PLAT).c gfx/gfx_$(GFX).c

DEMO = demo/main.c demo/camera.c
TEST = test/test.c

all: demo-bin

# Not called "demo": that is the name of a directory here, and make would
# find the directory up to date and build nothing.
demo-bin: $(ENGINE) $(DEMO)
	$(CC) $(CFLAGS) -o $@ $(ENGINE) $(DEMO) $(LDLIBS)

# The tests never open a window, so they always build against the null
# backends whatever PLAT says.
test-bin: $(BUF) $(CORE) $(ASSET) $(WORLD) $(UI) $(SND) plat/plat_null.c \
	  gfx/gfx_null.c $(TEST)
	$(CC) $(CFLAGS) -o $@ $(BUF) $(CORE) $(ASSET) $(WORLD) $(UI) \
		$(SND) plat/plat_null.c gfx/gfx_null.c $(TEST) -lm

check: test-bin
	./test-bin

# Prove the claim in buf/buffer.h: the buffer core imports nothing.
freestanding:
	$(CC) -c -std=c99 -ffreestanding -fno-builtin -O2 buf/buffer.c \
		-o /tmp/buffer.o
	@echo "undefined symbols in buffer.o:"
	@nm -u /tmp/buffer.o || true

clean:
	rm -f demo-bin test-bin /tmp/buffer.o

.PHONY: all check clean freestanding
