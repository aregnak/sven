#!/bin/sh

set -e

if (g++ src/*.cpp src/glad.c imgui/*.cpp -I ./include -I ./imgui -lglfw -ldl -lGL -lfmt -o sven); then
    ./sven
fi