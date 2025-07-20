#!/bin/sh

set -e

if (g++ src/*.cpp src/glad.c -I ./include -I ./imgui -lglfw -ldl -lGL -o sven); then
    ./sven
fi