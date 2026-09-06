# Три ручки: PLAT выбирает оконную систему, GFX - рендер, AUDIO - звук.
# Остальное дерево не знает, что выбрано.
#
#	make				linux + программный рендер + fixed-point
#	make static			то же самое, но одним статическим файлом
#	make FIXED_BITS=8		помягче: примерно PlayStation
#	make FIXED_BITS=16		почти как float
#	make PLAT=x11_gl GFX=gl AUDIO=alsa	как на master, но без dlopen
#	make check			тесты, дисплей не нужен
#	make picture			отрендерить один кадр в файл, без окна
#
# НА ЭТОЙ ВЕТКЕ НЕТ dlopen: библиотеки подключаются обычным образом, и
# поэтому работает --static. Цена в том, что сборка с GFX=gl требует libGL
# при запуске, а не только при наличии.

PLAT ?= x11
GFX  ?= soft
AUDIO ?= none
FIXED_BITS ?= 4

CC     ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2 -g -DFIXED_BITS=$(FIXED_BITS)
LDLIBS  = -lm

ifeq ($(PLAT),x11)
LDLIBS += -lX11
endif
ifeq ($(PLAT),x11_gl)
LDLIBS += -lX11 -lGL
endif
ifeq ($(AUDIO),alsa)
LDLIBS += -lasound
endif

# Статическая сборка тянет за собой то, на чём стоит сама libX11.
STATIC_LIBS = -lX11 -lxcb -lXau -lXdmcp -lpthread -lm

CORE = core/m3.c core/arena.c core/app.c
ASSET = asset/tga.c asset/obj.c asset/wav.c asset/anim.c
WORLD = world/coll.c world/world.c
UI = ui/gui.c ui/cut.c
SND = snd/snd.c
BUF = buf/buffer.c

ENGINE = $(BUF) $(CORE) $(ASSET) $(WORLD) $(UI) $(SND) \
	 plat/plat_$(PLAT).c plat/audio_$(AUDIO).c gfx/gfx_$(GFX).c

DEMO = demo/main.c demo/camera.c
TEST = test/test.c

all: demo-bin

# Не "demo": так называется каталог, и make решил бы, что цель готова.
demo-bin: $(ENGINE) $(DEMO)
	$(CC) $(CFLAGS) -o $@ $(ENGINE) $(DEMO) $(LDLIBS)

static: $(ENGINE) $(DEMO)
	$(CC) $(CFLAGS) -static -o demo-static $(ENGINE) $(DEMO) $(STATIC_LIBS)
	@ls -l demo-static | awk '{print "статический бинарник:", $$5, "байт"}'
	@file demo-static | cut -d, -f1-3

# Тесты всегда против пустых бэкендов: окно им не нужно.
test-bin: $(BUF) $(CORE) $(ASSET) $(WORLD) $(UI) $(SND) plat/plat_null.c \
	  plat/audio_none.c gfx/gfx_null.c $(TEST)
	$(CC) $(CFLAGS) -o $@ $(BUF) $(CORE) $(ASSET) $(WORLD) $(UI) \
		$(SND) plat/plat_null.c plat/audio_none.c gfx/gfx_null.c \
		$(TEST) -lm

check: test-bin
	./test-bin

# Один кадр программным рендером в TGA, без дисплея. Так видно, что делает
# fixed-point, не запуская игру.
picture: $(BUF) $(CORE) $(ASSET) $(WORLD) $(UI) $(SND) plat/plat_null.c \
	 plat/audio_none.c gfx/gfx_soft.c test/picture.c demo/camera.c
	$(CC) $(CFLAGS) -o picture-bin $(BUF) $(CORE) $(ASSET) $(WORLD) \
		$(UI) $(SND) plat/plat_null.c plat/audio_none.c \
		gfx/gfx_soft.c demo/camera.c test/picture.c -lm
	./picture-bin

# Проверить обещание из buf/buffer.h: модуль ничего не импортирует.
freestanding:
	$(CC) -c -std=c99 -ffreestanding -fno-builtin -O2 buf/buffer.c \
		-o /tmp/buffer.o
	@echo "undefined symbols in buffer.o:"
	@nm -u /tmp/buffer.o || true

clean:
	rm -f demo-bin demo-static test-bin picture-bin /tmp/buffer.o *.tga

.PHONY: all check clean freestanding static picture
