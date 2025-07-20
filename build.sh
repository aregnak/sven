#!/bin/sh

set -e

if (g++ src/*.cpp src/glad.c -I ./include -lglfw -ldl -lGL -o sven); then
    ./sven
fi