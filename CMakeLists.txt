cmake_minimum_required(VERSION 3.29.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("KalaServer")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

set(EXT_OPENSSL_DIR "${CMAKE_SOURCE_DIR}/external/OpenSSL")

set(OPENSSL_LIB1_RELEASE "${EXT_OPENSSL_DIR}/release/libssl.lib")
set(OPENSSL_LIB2_RELEASE "${EXT_OPENSSL_DIR}/release/libcrypto.lib")
set(OPENSSL_LIB1_DEBUG "${EXT_OPENSSL_DIR}/debug/libssl.lib")
set(OPENSSL_LIB2_DEBUG "${EXT_OPENSSL_DIR}/debug/libcrypto.lib")

file(GLOB_RECURSE SERVER_SOURCE_FILES CONFIGURE_DEPENDS
	${SRC_DIR}/*.cpp
	${SRC_DIR}/*/*.cpp
)

add_library(KalaServer STATIC ${SERVER_SOURCE_FILES})

target_include_directories(KalaServer PUBLIC 
	"${INCLUDE_DIR}"
	"${EXT_OPENSSL_DIR}/include"
)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	target_link_libraries(KalaServer PRIVATE
		"${OPENSSL_LIB1_RELEASE}"
		"${OPENSSL_LIB2_RELEASE}"
	)
else()
	target_link_libraries(KalaServer PRIVATE
		"${OPENSSL_LIB1_DEBUG}"
		"${OPENSSL_LIB2_DEBUG}"
	)
endif()