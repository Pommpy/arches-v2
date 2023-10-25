cmake_minimum_required(VERSION 3.14)

project(Arches-v2 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory("arches-v2")
add_subdirectory("trax-benchmark")
add_subdirectory("describo-benchmark")
add_subdirectory("dual-streaming-benchmark")

