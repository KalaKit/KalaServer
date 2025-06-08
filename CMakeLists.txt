cmake_minimum_required(VERSION 3.30.3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("KalaServer")

set(KALASERVER_ROOT "${CMAKE_SOURCE_DIR}/KalaServer")
set(EXAMPLE_ROOT "${CMAKE_SOURCE_DIR}/example")

set(SRC_DIR "${KALASERVER_ROOT}/src")

set(INCLUDE_DIR "${KALASERVER_ROOT}/include")

file(GLOB_RECURSE SERVER_SOURCE_FILES CONFIGURE_DEPENDS
	${SRC_DIR}/*.cpp
	${SRC_DIR}/*/*.cpp
)

add_library(KalaServer STATIC ${SERVER_SOURCE_FILES} "KalaServer/src/response/response.cpp")

target_include_directories(KalaServer PUBLIC 
	"${INCLUDE_DIR}"
)