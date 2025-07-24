#!/bin/sh

set -e

if (g++ src/*.cpp glad/glad.c imgui/*.cpp -I ./include -I ./imgui -I./glad -lglfw -ldl -lGL -lfmt -o sven); then
    ./sven
fi