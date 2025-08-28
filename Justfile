default:
    just --list

bootstrap:
    git submodule update --init --recursive
    clang --std=c23 -g -o build build.c

build:
    ./build

run:
    ./build run

clean:
    rm -rf _bin/
    rm -f build
    rm -f build.exe
    rm -f build.pdb
    rm -f build.ilk
    rm -f *.old
