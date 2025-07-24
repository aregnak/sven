#!/bin/sh

set -e

IMGUI_SOURCES="
include/external/imgui/imgui.cpp
include/external/imgui/imgui_draw.cpp
include/external/imgui/imgui_tables.cpp
include/external/imgui/imgui_widgets.cpp
include/external/imgui/imgui_impl_glfw.cpp
include/external/imgui/imgui_impl_opengl3.cpp"

if (g++ src/*.cpp $IMGUI_SOURCES include/external/glad/glad.c -I./include -I./include/external -lglfw -ldl -lGL -lfmt -o sven); then
    ./sven
fi