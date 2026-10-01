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

# Игра - unity-сборка: main.c включает остальные demo/*.c, поэтому
# пересобирать её надо при правке любого из них.
DEMO_SRC = $(wildcard demo/*.c demo/*.h)

# РЕСУРСЫ ИГРЫ. Всё, что лежит в demo/assets/, попадает внутрь программы:
# xxd -i делает из каждого файла .c с массивом байт, demo/tools/assets.sh
# пишет оглавление - таблицу "имя файла -> массив". Игра читает ресурсы
# только оттуда, файлов рядом с собой ей не нужно.
#
#	demo/assets/textures/grass.tga
#	  -> demo-res/textures/grass.tga.c	textures_grass_tga[]
#	  -> demo-res/textures/grass.tga.o
#	demo-res/index.c			{ "textures/grass.tga", ... }
#
# Каждый файл - свой объектный файл: поменял одну текстуру - пересобрался
# один массив, а make -j собирает их параллельно. Оглавление зависит от
# каталогов: файл добавили или убрали - у каталога сменилось время, и
# оглавление пишется заново.
ASSETS := $(shell cd demo/assets 2>/dev/null && find . -type f \
	! -name '*.md' ! -name '.*' | sed 's|^\./||' | LC_ALL=C sort)
ASSET_DIRS := $(shell find demo/assets -type d 2>/dev/null)
RES_O = $(ASSETS:%=demo-res/%.o) demo-res/index.o

demo-res/%.c: demo/assets/%
	@mkdir -p $(@D)
	cd demo/assets && xxd -i $* > ../../$@

demo-res/%.o: demo-res/%.c
	$(CC) -c -o $@ $<

demo-res/index.c: demo/tools/assets.sh $(ASSET_DIRS)
	@mkdir -p demo-res
	sh demo/tools/assets.sh $(ASSETS) > $@

demo-res/index.o: demo-res/index.c demo/res.h
	$(CC) $(CFLAGS) -c -o $@ demo-res/index.c

# Сделанные xxd файлы .c не удалять: их полезно открыть и посмотреть.
.PRECIOUS: demo-res/%.c

# Не "demo": так называется каталог, и make решил бы, что цель готова.
demo-bin: $(ENGINE) $(DEMO) $(DEMO_SRC) $(RES_O)
	$(CC) $(CFLAGS) -o $@ $(ENGINE) $(DEMO) $(RES_O) $(LDLIBS)

static: $(ENGINE) $(DEMO) $(DEMO_SRC) $(RES_O)
	$(CC) $(CFLAGS) -static -o demo-static $(ENGINE) $(DEMO) $(RES_O) \
		$(STATIC_LIBS)
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
	rm -rf demo-res

.PHONY: all check clean freestanding static picture
