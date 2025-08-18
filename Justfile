default:
    just --list

bootstrap:
    clang -std=c23 -o build build.c

build:
    ./build

run:
    ./build run

clean:
    rm -rf _bin/
    rm -f build
