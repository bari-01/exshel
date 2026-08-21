CC = gcc
SCANNER = wayland-scanner
PROTOCOLS = wlr-layer-shell-unstable-v1.xml xdg-shell.xml
HEADERS = $(PROTOCOLS:%.xml=generated/%-client-protocol.h)
SOURCES = $(PROTOCOLS:%.xml=generated/%-client-protocol.c)
UTILS = src/surface.c src/surface/layer.c src/surface/xdg_popup.c src/surface/xdg_toplevel.c src/plugin.c
MAIN = src/main.c
WL_CFLAGS = $(shell pkg-config --cflags wayland-client)
WL_FLAGS = $(shell pkg-config --cflags --libs wayland-client)
DEBUG =

generated/%-client-protocol.h: src/protocols/%.xml
	$(SCANNER) client-header < $< > $@

generated/%-client-protocol.c: src/protocols/%.xml
	$(SCANNER) private-code < $< > $@

generated: $(HEADERS) $(SOURCES)

main: generated
	gcc -g -O0 -o build/main $(if $(DEBUG), -DDEBUG) $(MAIN) $(SOURCES) $(UTILS) $(WL_FLAGS) -ldl -pthread

bar: generated
	gcc -g -O0 -shared -fPIC -o build/bar.so $(if $(DEBUG), -DDEBUG) plugins/bar.c $(WL_CFLAGS)

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
