cmake_minimum_required(VERSION 3.16)

project(terrain)

find_package(imgui CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

add_executable(${PROJECT_NAME} 
Camera.cpp
Camera.h
QuadTree.h
imgui_impl_opengl2.cpp
imgui_impl_opengl2.h
imgui_impl_sdl.cpp
imgui_impl_sdl.h
terrain.cpp
)