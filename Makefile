CC = gcc
SCANNER = wayland-scanner
PROTOCOLS = wlr-layer-shell-unstable-v1.xml xdg-shell.xml
HEADERS = $(PROTOCOLS:%.xml=generated/%-client-protocol.h)
SOURCES = $(PROTOCOLS:%.xml=generated/%-client-protocol.c)

UTILS_CORE = src/plugin_handler.c src/plugin_thread.c

UTILS_WL = src/backend/wayland/context.c src/backend/wayland/layer.c \
    src/backend/wayland/popup.c src/backend/wayland/toplevel.c \
    src/backend/wayland/backend_wl.c

UTILS_X11 = src/backend/x11/context.c src/backend/x11/dock.c \
    src/backend/x11/toplevel.c src/backend/x11/popup.c \
    src/backend/x11/backend_x11.c

USE_WAYLAND = true
USE_X11 = true
DEBUG =

WL_CFLAGS = $(shell pkg-config --cflags wayland-client)
WL_FLAGS = $(shell pkg-config --cflags --libs wayland-client)
X11_CFLAGS = $(shell pkg-config --cflags xcb-ewmh xcb)
X11_FLAGS = $(shell pkg-config --cflags --libs xcb-ewmh xcb)

DEFINES = $(if $(USE_WAYLAND), -DUSE_WAYLAND) $(if $(USE_X11), -DUSE_X11)
UTILS = $(UTILS_CORE) $(if $(USE_WAYLAND), $(UTILS_WL)) $(if $(USE_X11), $(UTILS_X11))
FLAGS = $(if $(USE_WAYLAND), $(WL_FLAGS)) $(if $(USE_X11), $(X11_FLAGS))
PLUGIN_CFLAGS =

generated/%-client-protocol.h: src/protocols/%.xml
	$(SCANNER) client-header < $< > $@

generated/%-client-protocol.c: src/protocols/%.xml
	$(SCANNER) private-code < $< > $@

generated: $(HEADERS) $(SOURCES)

main: generated
	$(CC) -g -o build/main $(if $(DEBUG), -DDEBUG) $(DEFINES) src/main.c $(SOURCES) $(UTILS) $(FLAGS) -ldl -pthread

bar: generated
	$(CC) -g -shared -fPIC -o build/bar.so $(if $(DEBUG), -DDEBUG) plugins/bar.c $(PLUGIN_CFLAGS)

all: main bar

main_debug: generated
	$(MAKE) main DEBUG=1

run: all
	build/main build/bar.so

run_debug: generated
	$(MAKE) all DEBUG=1
	build/main build/bar.so

clean:
	rm -f generated/* build/*
