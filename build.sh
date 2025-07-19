#!/bin/sh

set -e

g++ src/*.cpp src/glad.c -I ./include -lglfw -ldl -lGL -o sven
