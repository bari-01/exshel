CC = gcc
SCANNER = wayland-scanner
PROTOCOLS = wlr-layer-shell-unstable-v1.xml xdg-shell.xml
HEADERS = $(PROTOCOLS:%.xml=generated/%-client-protocol.h)
SOURCES = $(PROTOCOLS:%.xml=generated/%-client-protocol.c)

UTILS_CORE = src/plugin_handler.c src/plugin_thread.c src/config.c
UTILS_RENDER = src/renderer/vulkan/vulkan_runtime.c src/widget/widget.c \
	src/font/font.c src/font/glyph_atlas.c
UTILS_WL = src/wayland/context.c src/wayland/layer.c src/wayland/popup.c \
	src/wayland/toplevel.c src/wayland/backend_wl.c
UTILS_X11 = src/x11/context.c src/x11/dock.c src/x11/popup.c \
	src/x11/toplevel.c src/x11/backend_x11.c

USE_WAYLAND = 1
USE_X11 = 1
DEBUG = 1

WL_FLAGS = $(shell pkg-config --cflags --libs wayland-client)
X11_FLAGS = $(shell pkg-config --cflags --libs xcb-ewmh xcb)

DEFINES = -DUSE_WAYLAND=$(USE_WAYLAND) -DUSE_X11=$(USE_X11)
UTILS = $(UTILS_CORE) $(UTILS_RENDER) $(if $(filter 1,$(USE_WAYLAND)),$(UTILS_WL)) \
	$(if $(filter 1,$(USE_X11)),$(UTILS_X11))

FLAGS = $(if $(USE_WAYLAND), $(WL_FLAGS)) $(if $(USE_X11), $(X11_FLAGS)) \
	$(shell pkg-config --cflags --libs vulkan) \
	$(shell pkg-config --cflags --libs freetype2 harfbuzz)
PLUGIN_CFLAGS =

generated/%-client-protocol.h: src/protocols/%.xml
	$(SCANNER) client-header < $< > $@

generated/%-client-protocol.c: src/protocols/%.xml
	$(SCANNER) private-code < $< > $@

generated: $(HEADERS) $(SOURCES)

STAGING_SRC = staging/main.c

main: generated
	$(CC) -g -o build/main -DDEBUG=$(DEBUG) $(DEFINES) src/main.c $(SOURCES) \
		$(UTILS) $(FLAGS) -lyyjson -ldl -pthread

#.PHONY: staging
staging: generated
	$(CC) -g -o build/staging -DDEBUG=$(DEBUG) $(DEFINES) $(STAGING_SRC) \
		$(SOURCES) $(UTILS) $(FLAGS) -ldl -pthread

bar: generated
	$(CC) -g -shared -fPIC -o build/bar.so -DDEBUG=$(DEBUG) plugins/bar.c $(PLUGIN_CFLAGS)

bar2: shaders generated
	$(CC) -g -shared -fPIC -o build/bar2.so -DDEBUG=$(DEBUG) plugins/bar2.c \
		src/widget/widget.c src/font/font.c src/font/glyph_atlas.c \
		$(PLUGIN_CFLAGS) $(FREETYPE_FLAGS) -lyyjson -lm -lpthread

shaders: src/renderer/vulkan/shaders/text.vert src/renderer/vulkan/shaders/text.frag
	glslc -mfmt=c src/renderer/vulkan/shaders/text.vert -o \
		src/renderer/vulkan/shaders/text_vert_spv.h
	glslc -mfmt=c src/renderer/vulkan/shaders/text.frag -o \
		src/renderer/vulkan/shaders/text_frag_spv.h

all: main bar2 shaders

main_debug: generated
	$(MAKE) main DEBUG=1

run: all
	build/main build/bar.so

run_debug: generated
	$(MAKE) all DEBUG=1
	build/main build/bar.so

clean:
	rm -f generated/* build/*
