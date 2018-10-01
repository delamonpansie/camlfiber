all: build
install: build
build clean install:
	dune $@
build: libev/config.h
libev/config.h:
	cd libev && autoreconf --install --symlink
	cd libev && ./configure
.PHONY: all clean install
