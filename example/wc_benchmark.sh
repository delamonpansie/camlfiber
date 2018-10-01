#!/bin/sh
set -ue
test -d "$LINUX_SRC" || exit 1

src_cat() {
    find "$LINUX_SRC/drivers/gpu" -type f -a -name \*.c -print0 | xargs -0 cat
}

bench() {
    echo -n "$1:\t"
    (src_cat | time --format "%e" ./$1 2>&1 >/dev/null;
     src_cat | time --format "%e" ./$1 2>&1 >/dev/null;
     src_cat | time --format "%e" ./$1 2>&1 >/dev/null;
     src_cat | time --format "%e" ./$1 2>&1 >/dev/null;
     src_cat | time --format "%e" ./$1 2>&1 >/dev/null;
    ) | sort | head -n1
}

# warmup cache
src_cat > /dev/null

# bench
for wc in $@; do
    bench "$wc"
done
