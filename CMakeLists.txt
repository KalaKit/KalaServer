cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("KalaServer")

set(KALASERVER_ROOT "${CMAKE_SOURCE_DIR}/KalaServer")
set(EXAMPLE_ROOT "${CMAKE_SOURCE_DIR}/example")

add_library(KalaServer STATIC "${KALASERVER_ROOT}/http_server.cpp")
target_include_directories(KalaServer PUBLIC "${KALASERVER_ROOT}")

add_subdirectory("${EXAMPLE_ROOT}")