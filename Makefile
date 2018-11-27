all: build
install: build
build install:
	dune $@
clean:
	dune $@
	rm -rf _coverage
test:
	dune runtest
build: libev/config.h
libev/config.h:
	cd libev && autoreconf --install --symlink
	cd libev && ./configure
coverage: clean
	mkdir -p _coverage/c _coverage/ml
	BISECT_ENABLE=YES C_FLAGS="--coverage" C_LIBRARY_FLAGS="-lgcov" dune runtest
	bisect-ppx-report -I _build/default -html _coverage/ml \
		`find _build/default/test -name "bisect*.out"`
	lcov --output-file _coverage/c/fiber.cov --capture --directory _build/default
	genhtml --output-directory _coverage/c _coverage/c/fiber.cov
bench:
	dune build bench/bench.exe
	_build/default/bench/bench.exe
.PHONY: all clean install coverage test bench
