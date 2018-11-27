# camlfiber

Fast native fibers for OCaml.

Fibers are implemented by tying together [libcoro](http://software.schmorp.de/pkg/libcoro.html) and OCaml's GC
hooks. This design allows **allocation free** context switches with
decent performance. For example, i7-8750H running Linux can perform
about 13.7M fiber context switches per second. If you want to measure
it by yourself just run `make bench` from the source directory.

## Documentation

[camlfiber API](https://delamonpansie.github.io/camlfiber/fiber/Fiber/index.html)

Also, there are few examples in [examples](https://github.com/delamonpansie/camlfiber/tree/master/examples) directory.


## Installing

### Prerequisites
* `dune`
* `odoc` if you are planning to build documentation.
* `bisect-ppx` and `lcov` are required for coverage testing.

### Available targets
* `make` build library
* `make install` install library
* `make bench` run benchmark
* `make doc` build documentation
* `make test` run tests
* `make coverage` run tests with coverage reporting

## License

This project is licensed under the BSD License
