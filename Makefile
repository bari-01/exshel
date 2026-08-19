CC = gcc
SCANNER = wayland-scanner
PROTOCOLS = wlr-layer-shell-unstable-v1.xml xdg-shell.xml
HEADERS = $(PROTOCOLS:%.xml=generated/%-client-protocol.h)
SOURCES = $(PROTOCOLS:%.xml=generated/%-client-protocol.c)
UTILS = src/surface.c
MAIN = src/main.c

generated/%-client-protocol.h: src/protocols/%.xml
	$(SCANNER) client-header < $< > $@

generated/%-client-protocol.c: src/protocols/%.xml
	$(SCANNER) private-code < $< > $@

generated: $(HEADERS) $(SOURCES)

main: generated
	gcc -o build/main src/main.c $(SOURCES) $(UTILS) $(shell pkg-config --cflags --libs wayland-client)
#-I/usr/lib64/libffi/include -lwayland-client -lm #$(pkg-config --cflags --libs wayland-client)

run: main
	build/main

clean:
	rm -f generated/* build/* #*-protocol.h *-protocol.c
