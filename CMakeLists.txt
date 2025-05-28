cmake_minimum_required(VERSION 3.30.3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("KalaServer")

set(KALASERVER_ROOT "${CMAKE_SOURCE_DIR}/KalaServer")
set(EXAMPLE_ROOT "${CMAKE_SOURCE_DIR}/example")

set(SRC_DIR "${KALASERVER_ROOT}/src")
set(INCLUDE_DIR "${KALASERVER_ROOT}/include")

add_library(KalaServer STATIC 
	"${SRC_DIR}/core.cpp"
	"${SRC_DIR}/server.cpp"
	"${SRC_DIR}/cloudflare.cpp"
	"${SRC_DIR}/dns.cpp"
)

target_include_directories(KalaServer PUBLIC 
	"${INCLUDE_DIR}"
)

add_subdirectory("${EXAMPLE_ROOT}")