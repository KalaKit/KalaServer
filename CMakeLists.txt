cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Miniweb)

set(MINIWEB_ROOT "${CMAKE_SOURCE_DIR}/miniweb")
set(EXAMPLE_ROOT "${CMAKE_SOURCE_DIR}/example")

add_library(miniweb STATIC "${MINIWEB_ROOT}/http_server.cpp")
target_include_directories(miniweb PUBLIC "${MINIWEB_ROOT}")

add_subdirectory("${EXAMPLE_ROOT}")

include(CPack)