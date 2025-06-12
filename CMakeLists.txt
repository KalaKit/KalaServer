cmake_minimum_required(VERSION 3.29.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("KalaServer")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

set(EXT_OPENSSL_DIR "${CMAKE_SOURCE_DIR}/external/OpenSSL")

file(GLOB_RECURSE SERVER_SOURCE_FILES CONFIGURE_DEPENDS
	${SRC_DIR}/*.cpp
	${SRC_DIR}/*/*.cpp
)

add_library(KalaServer STATIC ${SERVER_SOURCE_FILES})

target_include_directories(KalaServer PUBLIC 
	"${INCLUDE_DIR}"
	"${EXT_OPENSSL_DIR}/include"
)

# Ensure linker uses static OpenSSL
target_compile_definitions(KalaServer PRIVATE
    OPENSSL_NO_DYNAMIC_ENGINE
)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	target_link_directories(KalaServer PRIVATE "${EXT_OPENSSL_DIR}/release")
else()
	target_link_directories(KalaServer PRIVATE "${EXT_OPENSSL_DIR}/debug")
endif()

target_link_libraries(KalaServer PRIVATE
	libssl.lib
	libcrypto.lib
)